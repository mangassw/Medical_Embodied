#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""latency_probe.py —— ROS2 语音全链路延迟测量探针（单进程同钟采集）。

设计要点
--------
* **单进程同钟**：所有话题的到达时刻都用本进程 ``time.time_ns()`` 记录；
  跨节点（C++ ASR / Python TTS 的 /latency_event 埋点）使用节点内上报的
  ``t_ns``（同一台机器、epoch 纳秒），两者可直接相减。
* **每轮独立 JSON**：一次提问为一轮，轮次结束立即写
  ``records/<tag>_roundNN.json``；全部轮次结束后写
  ``records/<tag>_summary.json``（统计）、``<tag>_stats.csv``、``<tag>_rounds.csv``
  并在 stdout 打印 markdown 统计表。
* **轮次 t0 定义**：
    - ``--inject-text``：本项目向 ``/question_asr`` 发布的时刻（t_publish_question）；
    - ``--question-wav`` / ``--wake-wav``：``/question_asr`` 的到达时刻（ASR 节点发布）。
  同时保留 t_play_start / t_play_end，用于计算“问句播放结束→首音”。

订阅话题（全部 std_msgs/String，/latency_event 为 JSON 字符串）
---------------------------------------------------------------
    /question_asr  /llm_to_openai  /llm_to_dashapp  /ali_llm_output
    /tts_realtime_data  /tts_session_finished  /latency_event

发布话题
--------
    /question_asr（inject 模式注入问题）、/start_asr（Bool，唤醒后启动 ASR）、
    /latency_probe_round（探针内部轮次标记，供 fake_chain_test 自测用，生产可忽略）

/latency_event 契约（宽容解析：未知 stage / 缺字段 / 非 JSON 都不会崩）
--------------------------------------------------------------------
    {"node": "asr"|"tts_local", "stage": "<stage>", "t_ns": <epoch ns int>, "detail": {...}}

典型运行
--------
    source /opt/ros/jazzy/setup.bash
    source /home/artrc/medical_ws/setup_env.sh
    conda run -n qwen_tts_online --no-capture-output python latency_probe.py \
        --record-dir ./records --tag demo --repeat 3 \
        --inject-text "病人血压偏高怎么办" --interval 1.0

    # 生产链路：先播唤醒词，等 kws_detected，再 /start_asr，再播问题音频
    conda run -n qwen_tts_online --no-capture-output python latency_probe.py \
        --record-dir ./records --tag prod --repeat 5 \
        --wake-wav /path/wake.wav --question-wav /tmp/tts_local_test.wav \
        --play-cmd "pw-play --target latency_sink {wav}" --pre-delay 0.3
"""

from __future__ import annotations

import argparse
import copy
import json
import os
import shlex
import socket
import subprocess
import sys
import threading
import time
from datetime import datetime
from typing import Any, Dict, List, Optional

import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, HistoryPolicy
from std_msgs.msg import Bool, String

_HERE = os.path.dirname(os.path.abspath(__file__))
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)

from latency_stats import (  # noqa: E402
    METRIC_KEYS,
    build_stats,
    markdown_stats_table,
    missing_metric_note,
    write_rounds_csv,
    write_stats_csv,
)

SCHEMA_VERSION = 1
TOOL_NAME = "latency_probe"

# 订阅话题
TOPIC_QUESTION = "/question_asr"
TOPIC_LLM_OPENAI = "/llm_to_openai"
TOPIC_LLM_DASHAPP = "/llm_to_dashapp"
TOPIC_LLM_OUTPUT = "/ali_llm_output"
TOPIC_TTS_DATA = "/tts_realtime_data"
TOPIC_TTS_FINISHED = "/tts_session_finished"
TOPIC_LATENCY_EVENT = "/latency_event"
# 发布话题
TOPIC_START_ASR = "/start_asr"
TOPIC_ROUND_MARKER = "/latency_probe_round"

CTRL_START = "[START]"
CTRL_DONE = "[DONE]"
SETTLE_NS = 300_000_000  # 收到 session_finished 后再多收 300ms 尾随埋点
SPIN_TIMEOUT = 0.005     # 主循环 spin 超时（秒），保证到达时间戳精度 ~ms


def now_ns() -> int:
    return time.time_ns()


def iso_str(ts_ns: Optional[int]) -> Optional[str]:
    if ts_ns is None:
        return None
    return datetime.fromtimestamp(ts_ns / 1e9).isoformat(timespec="milliseconds")


def delta_ms(a: Optional[int], b: Optional[int]) -> Optional[float]:
    if a is None or b is None:
        return None
    return round((b - a) / 1e6, 3)


def delta_ns(a: Optional[int], b: Optional[int]) -> Optional[int]:
    if a is None or b is None:
        return None
    return int(b - a)


def _first(values: List[int]) -> Optional[int]:
    return values[0] if values else None


def _first_after(values: List[int], base: Optional[int]) -> Optional[int]:
    """返回严格大于 base 的最小值；base 为 None 时返回最小值。"""
    ordered = sorted(values)
    if base is None:
        return ordered[0] if ordered else None
    for v in ordered:
        if v > base:
            return v
    return None


# ---------------------------------------------------------------------------
# /latency_event 解析（宽容）
# ---------------------------------------------------------------------------
def parse_latency_event(raw: str, arrival_ns: int) -> Optional[Dict[str, Any]]:
    """把 /latency_event 的 JSON 字符串解析成统一结构；无法解析返回 None。"""
    try:
        obj = json.loads(raw)
    except (ValueError, TypeError):
        return None
    if not isinstance(obj, dict):
        return None
    node = obj.get("node")
    stage = obj.get("stage")
    if not isinstance(node, str):
        node = "unknown"
    if not isinstance(stage, str):
        stage = "unknown"
    t_raw = obj.get("t_ns")
    t_ns: Optional[int]
    if isinstance(t_raw, bool):
        t_ns = None
    elif isinstance(t_raw, int):
        t_ns = t_raw
    elif isinstance(t_raw, float):
        t_ns = int(t_raw)
    elif isinstance(t_raw, str):
        try:
            t_ns = int(float(t_raw))
        except ValueError:
            t_ns = None
    else:
        t_ns = None
    detail = obj.get("detail")
    if not isinstance(detail, dict):
        detail = {} if detail is None else {"value": detail}
    return {
        "node": node,
        "stage": stage,
        "t_ns": t_ns,
        "arrival_ns": arrival_ns,
        "detail": detail,
        "raw": raw,
    }


# ---------------------------------------------------------------------------
# 单轮采集状态
# ---------------------------------------------------------------------------
class Round:
    def __init__(self, index: int, mode: str, args: argparse.Namespace):
        self.index = index
        self.mode = mode
        self.args = args
        self.start_ns = now_ns()
        self.end_ns: Optional[int] = None
        self.status = "running"
        self.notes: List[str] = []
        # t0 / 触发
        self.t0_ns: Optional[int] = None
        self.t0_source: Optional[str] = None
        self.t_publish_question_ns: Optional[int] = None
        self.t_start_asr_pub_ns: Optional[int] = None
        self.plays: List[Dict[str, Any]] = []
        self._lock = threading.RLock()
        # probe 侧到达（time.time_ns）
        self.question_asr_rx: List[int] = []
        self.router_openai_rx: List[int] = []
        self.router_dashapp_rx: List[int] = []
        self.llm_start_rx: Optional[int] = None
        self.llm_first_text_rx: Optional[int] = None
        self.llm_done_rx: Optional[int] = None
        self.llm_chunks: List[Dict[str, Any]] = []
        self.llm_chars = 0
        self.tts_start_rx: Optional[int] = None
        self.tts_first_text_rx: Optional[int] = None
        self.tts_done_rx: Optional[int] = None
        self.tts_texts: List[Dict[str, Any]] = []
        self.session_finished_rx: Optional[int] = None
        # /latency_event 埋点
        self.events: List[Dict[str, Any]] = []
        self.event_index: Dict[str, Dict[str, List[Dict[str, Any]]]] = {}
        self.ignored_counts: Dict[str, int] = {}

    # ---- 记录工具 ----
    def add_event(self, ev: Dict[str, Any]) -> None:
        with self._lock:
            self.events.append(ev)
            by_node = self.event_index.setdefault(ev["node"], {})
            by_node.setdefault(ev["stage"], []).append(ev)

    def events_of(self, node: str, stage: str) -> List[Dict[str, Any]]:
        return self.event_index.get(node, {}).get(stage, [])

    def play_of(self, role: str) -> Optional[Dict[str, Any]]:
        for p in self.plays:
            if p.get("role") == role:
                return p
        return None

    def question_play(self) -> Optional[Dict[str, Any]]:
        return self.play_of("question")


# ---------------------------------------------------------------------------
# 探针节点
# ---------------------------------------------------------------------------
class LatencyProbe(Node):
    def __init__(self, args: argparse.Namespace):
        node_name = f"latency_probe_{os.getpid()}"
        super().__init__(node_name)
        self.args = args
        self._round: Optional[Round] = None
        self._round_lock = threading.RLock()
        self._kws_evt = threading.Event()
        self._play_done_evt = threading.Event()
        # 常驻事件缓存：所有 /latency_event 都入缓存，与当前是否有活动轮次无关。
        # 原因：ASR 事件（endpoint/asr_callback/nurse_*/publish_question）发生在
        # /question_asr 到达之前，必须能在轮次结束后回溯关联。
        self._event_cache: List[Dict[str, Any]] = []
        self._event_cache_lock = threading.RLock()

        qos = QoSProfile(
            depth=500,
            reliability=ReliabilityPolicy.RELIABLE,
            history=HistoryPolicy.KEEP_LAST,
        )
        self.create_subscription(String, TOPIC_QUESTION, self._on_question_asr, qos)
        self.create_subscription(String, TOPIC_LLM_OPENAI, self._on_router_openai, qos)
        self.create_subscription(String, TOPIC_LLM_DASHAPP, self._on_router_dashapp, qos)
        self.create_subscription(String, TOPIC_LLM_OUTPUT, self._on_llm_output, qos)
        self.create_subscription(String, TOPIC_TTS_DATA, self._on_tts_data, qos)
        self.create_subscription(String, TOPIC_TTS_FINISHED, self._on_tts_finished, qos)
        self.create_subscription(String, TOPIC_LATENCY_EVENT, self._on_latency_event, qos)
        self.pub_question = self.create_publisher(String, TOPIC_QUESTION, 10)
        self.pub_start_asr = self.create_publisher(Bool, TOPIC_START_ASR, 10)
        self.pub_marker = self.create_publisher(String, TOPIC_ROUND_MARKER, 10)
        self.get_logger().info(f"latency probe ready, node={node_name}, mode={args.mode}")
        self.get_logger().info(
            f"record_dir={os.path.abspath(args.record_dir)} tag={args.tag} repeat={args.repeat}"
        )

    # ---------- 当前轮次 ----------
    def active_round(self) -> Optional[Round]:
        with self._round_lock:
            return self._round

    def _begin_round(self, index: int) -> Round:
        rnd = Round(index, self.args.mode, self.args)
        with self._round_lock:
            self._round = rnd
        self._kws_evt.clear()
        self._play_done_evt.clear()
        marker = {
            "event": "round_start",
            "tag": self.args.tag,
            "round": index,
            "mode": self.args.mode,
            "t_ns": rnd.start_ns,
        }
        self.pub_marker.publish(String(data=json.dumps(marker, ensure_ascii=False)))
        self.get_logger().info(
            f"===== round {index} start (mode={self.args.mode}) ====="
        )
        return rnd

    def _end_round(self) -> Optional[Round]:
        with self._round_lock:
            rnd = self._round
            self._round = None
        return rnd

    def _note_ignored(self, topic: str) -> None:
        with self._round_lock:
            rnd = self._round
        if rnd is None:
            return
        rnd.ignored_counts[topic] = rnd.ignored_counts.get(topic, 0) + 1

    # ---------- 订阅回调（全部在 spin 线程执行） ----------
    def _on_question_asr(self, msg: String) -> None:
        rnd = self.active_round()
        if rnd is None:
            self._note_ignored(TOPIC_QUESTION)
            return
        t = now_ns()
        rnd.question_asr_rx.append(t)
        if rnd.mode in ("question-wav", "wake-wav") and rnd.t0_ns is None:
            rnd.t0_ns = t
            rnd.t0_source = "question_asr_rx"
            self.get_logger().info(f"round {rnd.index}: t0 = /question_asr 到达 {t}")

    def _on_router_openai(self, msg: String) -> None:
        rnd = self.active_round()
        if rnd is None:
            self._note_ignored(TOPIC_LLM_OPENAI)
            return
        rnd.router_openai_rx.append(now_ns())

    def _on_router_dashapp(self, msg: String) -> None:
        rnd = self.active_round()
        if rnd is None:
            self._note_ignored(TOPIC_LLM_DASHAPP)
            return
        rnd.router_dashapp_rx.append(now_ns())

    def _on_llm_output(self, msg: String) -> None:
        rnd = self.active_round()
        if rnd is None:
            self._note_ignored(TOPIC_LLM_OUTPUT)
            return
        t = now_ns()
        text = (msg.data or "").strip()
        if text == CTRL_START:
            if rnd.llm_start_rx is None:
                rnd.llm_start_rx = t
            return
        if text == CTRL_DONE:
            if rnd.llm_done_rx is None:
                rnd.llm_done_rx = t
            return
        if text == "":
            return
        if rnd.llm_first_text_rx is None:
            rnd.llm_first_text_rx = t
        rnd.llm_chars += len(text)
        if len(rnd.llm_chunks) < 2000:
            rnd.llm_chunks.append({"t_ns": t, "text": text})

    def _on_tts_data(self, msg: String) -> None:
        rnd = self.active_round()
        if rnd is None:
            self._note_ignored(TOPIC_TTS_DATA)
            return
        t = now_ns()
        text = (msg.data or "").strip()
        if text == CTRL_START:
            if rnd.tts_start_rx is None:
                rnd.tts_start_rx = t
            return
        if text == CTRL_DONE:
            if rnd.tts_done_rx is None:
                rnd.tts_done_rx = t
            return
        if text == "":
            return
        if rnd.tts_first_text_rx is None:
            rnd.tts_first_text_rx = t
        if len(rnd.tts_texts) < 2000:
            rnd.tts_texts.append({"t_ns": t, "text": text})

    def _on_tts_finished(self, msg: String) -> None:
        rnd = self.active_round()
        if rnd is None:
            self._note_ignored(TOPIC_TTS_FINISHED)
            return
        if rnd.session_finished_rx is None:
            rnd.session_finished_rx = now_ns()

    def _on_latency_event(self, msg: String) -> None:
        t = now_ns()
        ev = parse_latency_event(msg.data, t)
        if ev is None:
            self._note_ignored(TOPIC_LATENCY_EVENT + ":unparseable")
            return
        # 1) 常驻缓存（与活动轮次无关）——ASR 事件早于 /question_asr 到达
        with self._event_cache_lock:
            self._event_cache.append(ev)
            if len(self._event_cache) > 5000:
                del self._event_cache[: len(self._event_cache) - 5000]
        # 2) 当前轮次（tts_local 事件的“向后收集”）
        rnd = self.active_round()
        if rnd is None:
            return
        rnd.add_event(ev)
        if ev["node"] == "asr" and ev["stage"] == "kws_detected":
            self._kws_evt.set()

    def snapshot_event_cache(self) -> List[Dict[str, Any]]:
        with self._event_cache_lock:
            return list(self._event_cache)

    # ---------- 播放 ----------
    def play_async(self, rnd: Round, role: str, wav: str) -> Dict[str, Any]:
        template = self.args.play_cmd
        if "{wav}" in template:
            cmd = template.replace("{wav}", shlex.quote(wav))
        else:
            cmd = template
        play: Dict[str, Any] = {
            "role": role,
            "wav": wav,
            "cmd": cmd,
            "start_ns": None,
            "end_ns": None,
            "ok": None,
            "error": None,
        }
        rnd.plays.append(play)

        def _run() -> None:
            play["start_ns"] = now_ns()
            try:
                argv = shlex.split(cmd)
                proc = subprocess.Popen(
                    argv,
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.PIPE,
                )
                try:
                    _, stderr = proc.communicate(timeout=self.args.play_timeout)
                except subprocess.TimeoutExpired:
                    proc.kill()
                    _, stderr = proc.communicate()
                    play["error"] = "playback timeout"
                play["ok"] = proc.returncode == 0
                if not play["ok"] and play["error"] is None:
                    play["error"] = (stderr or b"").decode("utf-8", "replace")[-500:]
            except Exception as exc:  # noqa: BLE001 - 播放失败不应导致探针崩溃
                play["ok"] = False
                play["error"] = repr(exc)
            finally:
                play["end_ns"] = now_ns()
                self._play_done_evt.set()
                self.get_logger().info(
                    f"playback[{role}] done ok={play['ok']} "
                    f"dur_ms={delta_ms(play['start_ns'], play['end_ns'])}"
                )

        threading.Thread(target=_run, name=f"play-{role}", daemon=True).start()
        return play

    # ---------- 主循环工具 ----------
    def spin_until(self, predicate, deadline_ns: int) -> bool:
        """自旋直到 predicate() 为真或超过 deadline；返回是否满足条件。"""
        while rclpy.ok():
            rclpy.spin_once(self, timeout_sec=SPIN_TIMEOUT)
            if predicate():
                return True
            if now_ns() >= deadline_ns:
                return False
        return False

    def spin_for(self, duration_s: float) -> None:
        deadline = now_ns() + int(duration_s * 1e9)
        while rclpy.ok() and now_ns() < deadline:
            rclpy.spin_once(self, timeout_sec=SPIN_TIMEOUT)


# ---------------------------------------------------------------------------
# 记录构造：把一轮原始采集整理成 JSON dict
# ---------------------------------------------------------------------------
ASR_CHAIN_STAGES = [
    "init_done", "kws_detected", "start_asr", "endpoint",
    "asr_callback", "nurse_req", "nurse_resp", "publish_question",
]


def select_asr_chain(rnd: Round, event_cache: List[Dict[str, Any]],
                     lookback_s: float):
    """从常驻事件缓存中为本轮关联最近一条 ASR 事件链。

    ASR 埋点（endpoint → asr_callback → nurse_req → nurse_resp → publish_question）
    发生在 /question_asr 到达（即本轮 t0）**之前**，所以不能只取“轮次开始后收到
    的事件”。策略（与 Lead 约定）：
      1) 在缓存里找本轮触发时刻之前、最近的一条 asr.publish_question；
      2) 以它为锚点向前回溯 --event-lookback（默认 60s），每个 stage 取窗口内
         最近的一次，避免把上一轮的 nurse 事件算进来；
      3) inject-text 模式不关联 ASR 链（问题由探针直接注入，不经过 ASR）。
    返回 (chain, meta)，chain = {stage: event}。
    """
    meta: Dict[str, Any] = {
        "source": "event_cache",
        "lookback_s": lookback_s,
        "anchor_ns": None,
        "publish_question_found": False,
    }
    if rnd.mode == "inject-text":
        meta["note"] = "inject-text 模式不关联 ASR 事件链"
        return {}, meta
    ref = _first(sorted(rnd.question_asr_rx)) or rnd.t0_ns or rnd.end_ns or now_ns()
    lookback_ns = int(lookback_s * 1e9)
    window_start = rnd.start_ns - lookback_ns
    asr_events = [
        e for e in event_cache
        if e.get("node") == "asr" and isinstance(e.get("t_ns"), int)
    ]
    pq = [
        e for e in asr_events
        if e["stage"] == "publish_question"
        and window_start <= e["t_ns"] <= ref + int(0.5e9)
    ]
    publish_question: Optional[Dict[str, Any]] = None
    if pq:
        publish_question = max(pq, key=lambda e: e["t_ns"])
        anchor = publish_question["t_ns"]
        meta["publish_question_found"] = True
    else:
        anchor = ref
    meta["anchor_ns"] = anchor
    chain: Dict[str, Dict[str, Any]] = {}
    for stage in ASR_CHAIN_STAGES:
        if stage == "publish_question" and publish_question is not None:
            chain[stage] = publish_question
            continue
        cands = [
            e for e in asr_events
            if e["stage"] == stage
            and anchor - lookback_ns <= e["t_ns"] <= anchor + int(0.1e9)
        ]
        if cands:
            chain[stage] = max(cands, key=lambda e: e["t_ns"])
    return chain, meta


def build_round_record(rnd: Round, args: argparse.Namespace, node_name: str,
                       hostname: str,
                       event_cache: Optional[List[Dict[str, Any]]] = None) -> Dict[str, Any]:
    def ev(node: str, stage: str) -> List[int]:
        out = []
        for e in rnd.events_of(node, stage):
            t = e.get("t_ns")
            out.append(int(t) if isinstance(t, int) else int(e["arrival_ns"]))
        return out

    def ev_first(node: str, stage: str) -> Optional[int]:
        vals = sorted(ev(node, stage))
        return vals[0] if vals else None

    def ev_last(node: str, stage: str) -> Optional[int]:
        vals = sorted(ev(node, stage))
        return vals[-1] if vals else None

    # ---- probe 侧到达 ----
    question_rx = _first(sorted(rnd.question_asr_rx))
    router_rx = _first(sorted(rnd.router_openai_rx + rnd.router_dashapp_rx))
    llm_first = rnd.llm_first_text_rx
    llm_done = rnd.llm_done_rx
    tts_first_text = rnd.tts_first_text_rx
    session_fin = rnd.session_finished_rx

    # ---- 埋点：ASR（从常驻缓存回溯关联，事件早于 /question_asr 到达） ----
    asr_chain, asr_chain_meta = select_asr_chain(
        rnd, event_cache or [], float(getattr(args, "event_lookback", 60.0))
    )

    def asr_t(stage: str) -> Optional[int]:
        e = asr_chain.get(stage)
        return int(e["t_ns"]) if e and isinstance(e.get("t_ns"), int) else None

    asr_kws = asr_t("kws_detected")
    asr_start_asr = asr_t("start_asr")
    asr_endpoint = asr_t("endpoint")
    asr_callback = asr_t("asr_callback")
    asr_nurse_req = asr_t("nurse_req")
    asr_nurse_resp = asr_t("nurse_resp")
    asr_publish_question = asr_t("publish_question")

    # ---- 埋点：TTS（同一 stage 可能每句一次，保留 list / 取首末） ----
    tts_rx_evts = sorted(ev("tts_local", "rx"))
    tts_synth_starts = sorted(ev("tts_local", "synth_start"))
    tts_synth_ends = sorted(ev("tts_local", "synth_end"))
    tts_play_starts = sorted(ev("tts_local", "play_start"))
    tts_play_ends = sorted(ev("tts_local", "play_end"))
    tts_session_evt = ev_first("tts_local", "session_finished")

    # TTS 首句 rx：优先埋点，退化用 probe 收到的首个 /tts_realtime_data 文本
    tts_first_rx = tts_rx_evts[0] if tts_rx_evts else tts_first_text
    tts_first_synth_end = _first_after(tts_synth_ends, tts_first_rx if tts_rx_evts else None)
    if tts_first_synth_end is None and tts_synth_ends:
        tts_first_synth_end = tts_synth_ends[0]
    tts_first_play_start = _first_after(tts_play_starts, tts_first_rx)
    if tts_first_play_start is None and tts_play_starts:
        tts_first_play_start = tts_play_starts[0]
    tts_last_play_end = tts_play_ends[-1] if tts_play_ends else None

    # 音频段配对：play_start -> 其后最近的 play_end
    play_pairs: List[Dict[str, int]] = []
    used_ends = set()
    for ps in tts_play_starts:
        cand = [pe for idx, pe in enumerate(tts_play_ends)
                if pe > ps and idx not in used_ends]
        if cand:
            pe = min(cand)
            used_ends.add(tts_play_ends.index(pe))
            play_pairs.append({"play_start_ns": ps, "play_end_ns": pe})
    audio_sum_ms = (
        round(sum((p["play_end_ns"] - p["play_start_ns"]) for p in play_pairs) / 1e6, 3)
        if play_pairs else None
    )
    audio_first_to_last_ms = (
        round((tts_last_play_end - tts_play_starts[0]) / 1e6, 3)
        if tts_last_play_end is not None and tts_play_starts else None
    )

    # ---- t0 ----
    t0 = rnd.t0_ns
    t0_source = rnd.t0_source
    if t0 is None:
        # 兜底：inject 模式理论上一定有；其余模式记录告警，用轮次开始时刻但不作为正式指标
        t0 = rnd.t_publish_question_ns or rnd.start_ns
        t0_source = t0_source or "fallback_round_start"

    question_play = rnd.question_play()
    question_play_start = question_play.get("start_ns") if question_play else None
    question_play_end = question_play.get("end_ns") if question_play else None

    # ---- 派生指标（ms；同时给出 ns 与 Lead 指定命名的 derived 视图） ----
    first_tts_text_to_first_play_start = delta_ms(tts_first_text, tts_first_play_start)
    q_to_first_audio = delta_ms(t0, tts_first_play_start)
    # ASR 端点事件现在带实际尾部静音（detail.silence_ms）：优先用它还原“说话结束”，
    # 这样端点阈值调整（含语义提前判停）也能被如实计入，而不是靠命令行假设值。
    _ep_evt = asr_chain.get("endpoint") or {}
    _ep_detail = _ep_evt.get("detail") if isinstance(_ep_evt.get("detail"), dict) else {}
    endpoint_silence_used_s = float(args.endpoint_silence)
    if _ep_detail.get("silence_ms") is not None:
        endpoint_silence_used_s = float(_ep_detail["silence_ms"]) / 1000.0
    speech_end_ns = (
        asr_endpoint - int(round(endpoint_silence_used_s * 1e9))
        if asr_endpoint is not None else None
    )
    segments: Dict[str, Optional[float]] = {
        # ASR（埋点 t_ns 之间）
        "asr_kws_to_start_asr_ms": delta_ms(asr_kws, asr_start_asr),
        "asr_endpoint_silence_used_ms": (
            round(endpoint_silence_used_s * 1000.0, 1) if asr_endpoint is not None else None
        ),
        "asr_start_asr_to_endpoint_ms": delta_ms(asr_start_asr, asr_endpoint),
        "asr_endpoint_to_nurse_req_ms": delta_ms(asr_endpoint, asr_nurse_req),
        "asr_nurse_req_to_nurse_resp_ms": delta_ms(asr_nurse_req, asr_nurse_resp),
        "asr_nurse_resp_to_publish_question_ms": delta_ms(asr_nurse_resp, asr_publish_question),
        "asr_endpoint_to_question_rx_ms": delta_ms(asr_endpoint, question_rx),
        "asr_publish_question_to_rx_ms": delta_ms(asr_publish_question, question_rx),
        # 路由 / LLM / 断句（probe 到达时刻）
        "question_to_router_ms": delta_ms(t0, router_rx),
        "router_to_llm_first_ms": delta_ms(router_rx, llm_first),
        "question_to_llm_first_ms": delta_ms(t0, llm_first),
        "llm_first_to_done_ms": delta_ms(llm_first, llm_done),
        "question_to_llm_done_ms": delta_ms(t0, llm_done),
        "llm_first_to_tts_first_text_ms": delta_ms(llm_first, tts_first_text),
        "question_to_tts_first_text_ms": delta_ms(t0, tts_first_text),
        "first_tts_text_to_first_play_start_ms": first_tts_text_to_first_play_start,
        # TTS（埋点 t_ns；首个 rx 可取 probe 到达作退化）
        "tts_rx_to_synth_end_ms": delta_ms(tts_first_rx, tts_first_synth_end),
        "tts_rx_to_play_start_ms": delta_ms(tts_first_rx, tts_first_play_start),
        "tts_synth_end_to_play_start_ms": delta_ms(tts_first_synth_end, tts_first_play_start),
        # 端到端 / 会话
        "question_to_first_audio_ms": q_to_first_audio,
        "speech_end_to_first_audio_ms": delta_ms(speech_end_ns, tts_first_play_start),
        "playback_end_to_first_audio_ms": delta_ms(question_play_end, tts_first_play_start),
        "question_to_session_finished_ms": delta_ms(t0, session_fin),
        "audio_total_sum_ms": audio_sum_ms,
        "audio_first_to_last_ms": audio_first_to_last_ms,
    }

    def pair_ns(a: Optional[int], b: Optional[int]) -> Optional[int]:
        if a is None or b is None:
            return None
        return int(b - a)

    audio_total_sum_ns = (
        sum(p["play_end_ns"] - p["play_start_ns"] for p in play_pairs)
        if play_pairs else None
    )
    audio_first_to_last_ns = (
        tts_last_play_end - tts_play_starts[0]
        if tts_last_play_end is not None and tts_play_starts else None
    )
    segments_ns: Dict[str, Optional[int]] = {
        "asr_kws_to_start_asr_ns": pair_ns(asr_kws, asr_start_asr),
        "asr_start_asr_to_endpoint_ns": pair_ns(asr_start_asr, asr_endpoint),
        "asr_endpoint_to_nurse_req_ns": pair_ns(asr_endpoint, asr_nurse_req),
        "asr_nurse_req_to_nurse_resp_ns": pair_ns(asr_nurse_req, asr_nurse_resp),
        "asr_nurse_resp_to_publish_question_ns": pair_ns(asr_nurse_resp, asr_publish_question),
        "asr_endpoint_to_question_rx_ns": pair_ns(asr_endpoint, question_rx),
        "asr_publish_question_to_rx_ns": pair_ns(asr_publish_question, question_rx),
        "question_to_router_ns": pair_ns(t0, router_rx),
        "router_to_llm_first_ns": pair_ns(router_rx, llm_first),
        "question_to_llm_first_ns": pair_ns(t0, llm_first),
        "llm_first_to_done_ns": pair_ns(llm_first, llm_done),
        "question_to_llm_done_ns": pair_ns(t0, llm_done),
        "llm_first_to_tts_first_text_ns": pair_ns(llm_first, tts_first_text),
        "question_to_tts_first_text_ns": pair_ns(t0, tts_first_text),
        "first_tts_text_to_first_play_start_ns": pair_ns(tts_first_text, tts_first_play_start),
        "tts_rx_to_synth_end_ns": pair_ns(tts_first_rx, tts_first_synth_end),
        "tts_rx_to_play_start_ns": pair_ns(tts_first_rx, tts_first_play_start),
        "tts_synth_end_to_play_start_ns": pair_ns(tts_first_synth_end, tts_first_play_start),
        "question_to_first_audio_ns": pair_ns(t0, tts_first_play_start),
        "speech_end_to_first_audio_ns": pair_ns(speech_end_ns, tts_first_play_start),
        "playback_end_to_first_audio_ns": pair_ns(question_play_end, tts_first_play_start),
        "question_to_session_finished_ns": pair_ns(t0, session_fin),
        "audio_total_sum_ns": audio_total_sum_ns,
        "audio_first_to_last_ns": audio_first_to_last_ns,
    }

    # Lead 指定命名的派生指标：每个指标同时给 ns / ms（audio_total_sec 额外给 sec）
    derived_ns: Dict[str, Optional[int]] = {
        "asr_endpoint_to_nurse_req": segments_ns["asr_endpoint_to_nurse_req_ns"],
        "nurse_dur": segments_ns["asr_nurse_req_to_nurse_resp_ns"],
        "nurse_resp_to_question": pair_ns(asr_nurse_resp, question_rx),
        "question_to_first_llm_chunk": segments_ns["question_to_llm_first_ns"],
        "first_llm_chunk_to_done": segments_ns["llm_first_to_done_ns"],
        "first_llm_chunk_to_first_tts_text": segments_ns["llm_first_to_tts_first_text_ns"],
        "first_tts_text_to_first_play_start": segments_ns["first_tts_text_to_first_play_start_ns"],
        "question_to_first_play_start": segments_ns["question_to_first_audio_ns"],
        "speech_end_to_first_play_start": segments_ns["speech_end_to_first_audio_ns"],
        "question_to_session_finished": segments_ns["question_to_session_finished_ns"],
    }
    derived: Dict[str, Dict[str, Optional[float]]] = {}
    for name, ns_val in derived_ns.items():
        derived[name] = {
            "ns": ns_val,
            "ms": None if ns_val is None else round(ns_val / 1e6, 3),
        }
    derived["audio_total_sec"] = {
        "ns": audio_total_sum_ns,
        "ms": None if audio_total_sum_ns is None else round(audio_total_sum_ns / 1e6, 3),
        "sec": None if audio_total_sum_ns is None else round(audio_total_sum_ns / 1e9, 3),
    }

    # ---- 时间戳明细 ----
    node_event_timestamps: Dict[str, Any] = {
        "asr.kws_detected": asr_kws,
        "asr.start_asr": asr_start_asr,
        "asr.endpoint": asr_endpoint,
        "asr.asr_callback": asr_callback,
        "asr.nurse_req": asr_nurse_req,
        "asr.nurse_resp": asr_nurse_resp,
        "asr.publish_question": asr_publish_question,
        "tts_local.rx_first": tts_rx_evts[0] if tts_rx_evts else None,
        "tts_local.synth_start_first": tts_synth_starts[0] if tts_synth_starts else None,
        "tts_local.synth_end_first": tts_synth_ends[0] if tts_synth_ends else None,
        "tts_local.play_start_first": tts_play_starts[0] if tts_play_starts else None,
        "tts_local.play_end_last": tts_last_play_end,
        "tts_local.session_finished": tts_session_evt,
    }
    timestamps = {
        "t0_ns": t0,
        "t0_source": t0_source,
        "t_trigger_begin_ns": rnd.start_ns,
        "t_publish_question_ns": rnd.t_publish_question_ns,
        "t_start_asr_pub_ns": rnd.t_start_asr_pub_ns,
        "t_question_play_start_ns": question_play_start,
        "t_question_play_end_ns": question_play_end,
        "probe_rx_ns": {
            "question_asr": question_rx,
            "llm_to_openai": _first(sorted(rnd.router_openai_rx)),
            "llm_to_dashapp": _first(sorted(rnd.router_dashapp_rx)),
            "ali_llm_output_start": rnd.llm_start_rx,
            "ali_llm_output_first_text": llm_first,
            "ali_llm_output_done": llm_done,
            "tts_realtime_data_start": rnd.tts_start_rx,
            "tts_realtime_data_first_text": tts_first_text,
            "tts_realtime_data_done": rnd.tts_done_rx,
            "tts_session_finished": session_fin,
        },
        "node_event_ns": node_event_timestamps,
    }

    # 事件明细：轮次内收到的 + 从常驻缓存回溯到的 ASR 链事件（source=cache）
    events_out: List[Dict[str, Any]] = []
    seen_event_keys = set()

    def _append_event(e: Dict[str, Any], source: str) -> None:
        key = (e.get("node"), e.get("stage"), e.get("t_ns"), e.get("arrival_ns"))
        if key in seen_event_keys:
            return
        seen_event_keys.add(key)
        events_out.append({
            "source": source,
            "arrival_ns": e.get("arrival_ns"),
            "node": e.get("node"),
            "stage": e.get("stage"),
            "t_ns": e.get("t_ns"),
            "detail": e.get("detail"),
            "raw": e.get("raw"),
        })

    for e in rnd.events:
        _append_event(e, "round")
    for stage, e in asr_chain.items():
        _append_event(e, "event_cache")
    events_by_stage: Dict[str, Dict[str, List[Dict[str, Any]]]] = {}
    for e in events_out:
        events_by_stage.setdefault(e["node"], {}).setdefault(e["stage"], []).append(
            {"t_ns": e.get("t_ns"), "arrival_ns": e.get("arrival_ns"),
             "source": e.get("source"), "detail": e.get("detail")}
        )
    for stages in events_by_stage.values():
        for evs in stages.values():
            evs.sort(key=lambda x: (x.get("t_ns") is None, x.get("t_ns") or 0))

    record = {
        "schema_version": SCHEMA_VERSION,
        "tool": TOOL_NAME,
        "node_name": node_name,
        "host": hostname,
        "tag": args.tag,
        "round_index": rnd.index,
        "mode": rnd.mode,
        "status": rnd.status,
        "notes": rnd.notes,
        "started_at": iso_str(rnd.start_ns),
        "finished_at": iso_str(rnd.end_ns),
        "duration_s": round((rnd.end_ns - rnd.start_ns) / 1e9, 3) if rnd.end_ns else None,
        "trigger": {
            "mode": rnd.mode,
            "t0_source": t0_source,
            "inject_text": args.inject_text or None,
            "question_wav": args.question_wav or None,
            "wake_wav": args.wake_wav or None,
            "root_play_cmd": args.play_cmd,
            "plays": copy.deepcopy(rnd.plays),
        },
        "timestamps_ns": timestamps,
        "segments_ms": segments,
        "segments_ns": segments_ns,
        "derived": derived,           # Lead 指定命名的派生指标（ns+ms+sec）
        "asr_chain": {
            stage: {"t_ns": e.get("t_ns"), "arrival_ns": e.get("arrival_ns"),
                    "detail": e.get("detail")}
            for stage, e in asr_chain.items()
        },
        "asr_chain_meta": asr_chain_meta,
        "metrics": segments,  # 与 segments_ms 相同，便于统计脚本直接读取
        "streams": {
            "llm": {
                "start_rx_ns": rnd.llm_start_rx,
                "first_text_rx_ns": llm_first,
                "done_rx_ns": llm_done,
                "chunk_count": len(rnd.llm_chunks),
                "char_count": rnd.llm_chars,
                "chunks": rnd.llm_chunks,
            },
            "tts": {
                "first_text_rx_ns": tts_first_text,
                "done_rx_ns": rnd.tts_done_rx,
                "text_count": len(rnd.tts_texts),
                "texts": rnd.tts_texts,
            },
        },
        "counts": {
            "latency_events_round": len(rnd.events),
            "latency_events_total": len(events_out),
            "asr_events": sum(len(v) for v in events_by_stage.get("asr", {}).values()),
            "tts_events": sum(len(v) for v in events_by_stage.get("tts_local", {}).values()),
            "tts_play_pairs": len(play_pairs),
            "question_asr_rx": len(rnd.question_asr_rx),
            "ignored": dict(rnd.ignored_counts),
        },
        "events": events_out,
        "events_by_stage": events_by_stage,
        "config": {
            "endpoint_silence_s": args.endpoint_silence,
            "event_lookback_s": getattr(args, "event_lookback", 60.0),
            "timeout_s": args.timeout,
            "kws_timeout_s": args.kws_timeout,
            "pre_delay_s": args.pre_delay,
            "play_cmd": args.play_cmd,
            "repeat": args.repeat,
            "interval_s": args.interval,
        },
    }
    return record


# ---------------------------------------------------------------------------
# 轮次执行
# ---------------------------------------------------------------------------
def _fail_round(rnd: Round, status: str, note: str, node: LatencyProbe) -> None:
    rnd.status = status
    rnd.notes.append(note)
    rnd.end_ns = now_ns()
    node._end_round()
    node.get_logger().warning(f"round {rnd.index} finished with status={status}: {note}")


def run_round(node: LatencyProbe, args: argparse.Namespace, index: int) -> Optional[Round]:
    rnd = node._begin_round(index)
    deadline = rnd.start_ns + int(args.timeout * 1e9)

    if args.mode == "inject-text":
        msg = String()
        msg.data = args.inject_text
        rnd.t_publish_question_ns = now_ns()
        node.pub_question.publish(msg)
        rnd.t0_ns = rnd.t_publish_question_ns
        rnd.t0_source = "publish_question"
        node.get_logger().info(
            f"round {index}: injected /question_asr at {rnd.t0_ns}"
        )
        # 关键：发布后必须继续自旋等待链路完成；否则轮次会立刻判 timeout
        node.spin_until(lambda: rnd.session_finished_rx is not None, deadline)

    elif args.mode == "wake-wav":
        if args.wake_wav:
            node.play_async(rnd, "wake", args.wake_wav)
        else:
            node.get_logger().warning("wake-wav mode without --wake-wav; skip wake playback")
        kws_deadline = now_ns() + int(args.kws_timeout * 1e9)
        got_kws = node.spin_until(lambda: node._kws_evt.is_set(), kws_deadline)
        if not got_kws:
            _fail_round(rnd, "kws_timeout",
                        f"未在 {args.kws_timeout}s 内收到 kws_detected 埋点", node)
            return rnd
        msg = Bool()
        msg.data = True
        rnd.t_start_asr_pub_ns = now_ns()
        node.pub_start_asr.publish(msg)
        if args.pre_delay > 0:
            node.spin_for(args.pre_delay)
        if args.question_wav:
            node.play_async(rnd, "question", args.question_wav)
        node.spin_until(lambda: rnd.session_finished_rx is not None, deadline)

    else:  # question-wav
        if args.start_asr:
            msg = Bool()
            msg.data = True
            rnd.t_start_asr_pub_ns = now_ns()
            node.pub_start_asr.publish(msg)
            if args.pre_delay > 0:
                node.spin_for(args.pre_delay)
        node.play_async(rnd, "question", args.question_wav)
        node.spin_until(lambda: rnd.session_finished_rx is not None, deadline)

    # 收尾：session_finished 后留 settle 时间收尾随埋点；无 session_finished 则等到超时
    if rnd.session_finished_rx is not None:
        settle_deadline = now_ns() + SETTLE_NS
        while rclpy.ok() and now_ns() < settle_deadline:
            rclpy.spin_once(node, timeout_sec=SPIN_TIMEOUT)
        if rnd.t0_ns is None:
            rnd.status = "no_t0"
            rnd.notes.append("未收到触发信号（/question_asr 无到达）")
        else:
            rnd.status = "ok"
    else:
        rnd.status = "timeout"
        rnd.notes.append(f"超时 {args.timeout}s 未收到 /tts_session_finished")
    rnd.end_ns = now_ns()
    node._end_round()
    node.get_logger().info(
        f"===== round {index} done status={rnd.status}, "
        f"duration={delta_ms(rnd.start_ns, rnd.end_ns)}ms ====="
    )
    return rnd


# ---------------------------------------------------------------------------
# 输出
# ---------------------------------------------------------------------------
def save_round(record: Dict[str, Any], record_dir: str) -> str:
    os.makedirs(record_dir, exist_ok=True)
    path = os.path.join(
        record_dir, f"{record['tag']}_round{record['round_index']:02d}.json"
    )
    with open(path, "w", encoding="utf-8") as f:
        json.dump(record, f, ensure_ascii=False, indent=2)
    return path


def write_outputs(args: argparse.Namespace, records: List[Dict[str, Any]],
                  node_name: str) -> Dict[str, str]:
    record_dir = os.path.abspath(args.record_dir)
    os.makedirs(record_dir, exist_ok=True)
    round_metrics = [r.get("metrics") or {} for r in records]
    stats = build_stats(round_metrics)

    round_files = []
    for r in records:
        path = save_round(r, record_dir)
        r["_round_file"] = path
        round_files.append(os.path.basename(path))

    summary = {
        "schema_version": SCHEMA_VERSION,
        "tool": TOOL_NAME,
        "node_name": node_name,
        "host": socket.gethostname(),
        "tag": args.tag,
        "generated_at": datetime.now().isoformat(timespec="milliseconds"),
        "mode": args.mode,
        "run_count": len(records),
        "ok_count": sum(1 for r in records if r.get("status") == "ok"),
        "non_ok": [
            {"round_index": r["round_index"], "status": r["status"],
             "notes": r.get("notes")}
            for r in records if r.get("status") != "ok"
        ],
        "config": {
            "record_dir": record_dir,
            "tag": args.tag,
            "mode": args.mode,
            "repeat": args.repeat,
            "interval_s": args.interval,
            "inject_text": args.inject_text or None,
            "question_wav": args.question_wav or None,
            "wake_wav": args.wake_wav or None,
            "play_cmd": args.play_cmd,
            "timeout_s": args.timeout,
            "kws_timeout_s": args.kws_timeout,
            "pre_delay_s": args.pre_delay,
            "endpoint_silence_s": args.endpoint_silence,
            "event_lookback_s": getattr(args, "event_lookback", 60.0),
            "start_asr": bool(args.start_asr),
        },
        "metric_order": METRIC_KEYS,
        "metrics": stats,
        "rounds": [
            {
                "round_index": r["round_index"],
                "round_file": r.get("_round_file"),
                "status": r["status"],
                "t0_source": (r.get("timestamps_ns") or {}).get("t0_source"),
                "trigger": {
                    "t0_ns": (r.get("timestamps_ns") or {}).get("t0_ns"),
                    "play_start_ns": (r.get("timestamps_ns") or {}).get("t_question_play_start_ns"),
                    "play_end_ns": (r.get("timestamps_ns") or {}).get("t_question_play_end_ns"),
                },
                "metrics": r.get("metrics") or {},
            }
            for r in records
        ],
        "round_files": round_files,
    }
    summary_path = os.path.join(record_dir, f"{args.tag}_summary.json")
    with open(summary_path, "w", encoding="utf-8") as f:
        json.dump(summary, f, ensure_ascii=False, indent=2)

    stats_csv = os.path.join(record_dir, f"{args.tag}_stats.csv")
    rounds_csv = os.path.join(record_dir, f"{args.tag}_rounds.csv")
    write_stats_csv(stats_csv, stats)
    write_rounds_csv(rounds_csv, summary["rounds"])
    table = markdown_stats_table(stats)
    table_path = os.path.join(record_dir, f"{args.tag}_stats.md")
    with open(table_path, "w", encoding="utf-8") as f:
        f.write(f"## 延迟统计 tag={args.tag} mode={args.mode} "
                f"(n={len(records)}, 单位 ms)\n\n{table}\n\n")
        note = missing_metric_note(stats)
        if note:
            f.write(note)

    print()
    print(f"## 延迟统计 tag={args.tag} mode={args.mode} "
          f"(n={len(records)}, 单位 ms)")
    print()
    print(table)
    print()
    note = missing_metric_note(stats)
    if note:
        print(note)
    print("输出文件：")
    for p in (summary_path, stats_csv, rounds_csv, table_path):
        print(f"  - {p}")
    for rf in round_files:
        print(f"  - {os.path.join(record_dir, rf)}")
    return {
        "summary": summary_path,
        "stats_csv": stats_csv,
        "rounds_csv": rounds_csv,
        "stats_md": table_path,
    }


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------
def parse_args(argv: Optional[List[str]] = None) -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description="ROS2 语音全链路延迟测量探针（单进程同钟 + /latency_event 埋点拼接）",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    p.add_argument("--record-dir", default=os.path.join(_HERE, "records"),
                   help="输出目录（run JSON / summary / CSV）")
    p.add_argument("--tag", default="",
                   help="本次测量标签，默认自动生成 run_<时间戳>；同 tag 会覆盖同名输出")
    p.add_argument("--repeat", type=int, default=1,
                   help="重复轮数；0 表示一直测到 Ctrl-C")
    p.add_argument("--interval", type=float, default=1.0, help="轮次间隔（秒）")
    p.add_argument("--inject-text", default="",
                   help="向 /question_asr 注入问题文本（触发方式一）")
    p.add_argument("--inject-file", default="",
                   help="问题文本文件（一行一句，# 开头忽略）；每轮依次注入一句，"
                        "配合 --repeat 0 时自动按文件行数跑满")
    p.add_argument("--question-wav", default="",
                   help="问题音频 wav（触发方式二：播放后等 ASR 发 /question_asr）")
    p.add_argument("--wake-wav", default="",
                   help="唤醒词音频 wav（触发方式三：播完等 kws_detected → /start_asr → 问题音频）")
    p.add_argument("--start-asr", action="store_true",
                   help="question-wav 模式下先向 /start_asr 发 Bool true（让 ASR 进入识别）")
    p.add_argument("--play-cmd", default="pw-play --target latency_sink {wav}",
                   help="播放命令模板；{wav} 会被替换为 wav 路径（无 {wav} 则原样执行，"
                        "便于用 'sleep 1' 之类命令做无声自测）")
    p.add_argument("--timeout", type=float, default=60.0, help="单轮超时（秒）")
    p.add_argument("--kws-timeout", type=float, default=10.0,
                   help="唤醒模式等待 kws_detected 的超时（秒）")
    p.add_argument("--pre-delay", type=float, default=0.3,
                   help="发 /start_asr 后、播问题音频前的等待（秒）")
    p.add_argument("--endpoint-silence", type=float, default=2.4,
                   help="ASR 端点静音阈值（秒），用于 speech_end=endpoint-该值")
    p.add_argument("--event-lookback", type=float, default=60.0,
                   help="ASR 事件链回溯窗口（秒）：ASR 埋点发生在 /question_asr 到达之前，"
                        "每轮在常驻缓存中回溯该窗口关联最近一条 publish_question 事件链")
    p.add_argument("--play-timeout", type=float, default=120.0,
                   help="单次播放子进程超时（秒）")
    p.add_argument("--warmup", type=float, default=1.0,
                   help="开始第一轮前的预热等待（秒），用于 DDS 发现/连接建立，"
                        "避免首条注入消息在订阅匹配前丢失")
    args = p.parse_args(argv)

    # 触发方式判定
    if args.inject_text or args.inject_file:
        args.mode = "inject-text"
    elif args.wake_wav:
        args.mode = "wake-wav"
    elif args.question_wav:
        args.mode = "question-wav"
    else:
        p.error("必须指定 --inject-text / --question-wav / --wake-wav 之一作为触发方式")
    if args.wake_wav and (args.inject_text or args.inject_file):
        p.error("--wake-wav 与 --inject-text/--inject-file 不能同时使用")
    if args.mode == "wake-wav" and not args.question_wav:
        print("[warn] wake-wav 模式未提供 --question-wav：发 /start_asr 后将等待真实用户/外部音频触发 /question_asr")
    if args.mode == "question-wav" and not args.question_wav:
        p.error("question-wav 模式必须提供 --question-wav")
    if args.repeat < 0:
        p.error("--repeat 不能为负数（0 表示无限）")
    if args.inject_file:
        with open(args.inject_file, encoding="utf-8") as f:
            args.inject_questions = [
                ln.strip() for ln in f
                if ln.strip() and not ln.strip().startswith("#")
            ]
        if not args.inject_questions:
            p.error(f"--inject-file {args.inject_file} 里没有有效问句")
        if args.repeat in (0, 1):
            args.repeat = len(args.inject_questions)
    else:
        args.inject_questions = []
    if not args.tag:
        args.tag = "run_" + time.strftime("%Y%m%d_%H%M%S")
    return args


def main(argv: Optional[List[str]] = None) -> int:
    args = parse_args(argv)
    os.makedirs(args.record_dir, exist_ok=True)
    rclpy.init(args=None)
    node = LatencyProbe(args)
    node_name = node.get_name()
    hostname = socket.gethostname()
    records: List[Dict[str, Any]] = []
    interrupted = False

    try:
        node.spin_for(max(0.0, float(args.warmup)))  # 等待 DDS 发现/连接建立
        index = 0
        while rclpy.ok():
            if args.repeat > 0 and index >= args.repeat:
                break
            if args.inject_questions:
                args.inject_text = args.inject_questions[index % len(args.inject_questions)]
            rnd = run_round(node, args, index)
            if rnd is not None:
                record = build_round_record(
                    rnd, args, node_name, hostname,
                    event_cache=node.snapshot_event_cache(),
                )
                records.append(record)
                path = save_round(record, args.record_dir)
                node.get_logger().info(
                    f"round {index} JSON saved: {path} "
                    f"(status={record['status']}, "
                    f"e2e={record['metrics'].get('question_to_first_audio_ms')}ms)"
                )
            index += 1
            if args.repeat > 0 and index >= args.repeat:
                break
            if args.interval > 0:
                node.spin_for(args.interval)
    except KeyboardInterrupt:
        interrupted = True
        node.get_logger().warning("Ctrl-C: 结束采集，写出已完成轮次")
    finally:
        try:
            node._end_round()
            node.destroy_node()
        except Exception:  # noqa: BLE001
            pass
        try:
            rclpy.shutdown()
        except Exception:  # noqa: BLE001
            pass

    if not records:
        print("[error] 没有完成任何轮次，未生成统计", file=sys.stderr)
        return 1
    write_outputs(args, records, node_name)
    if interrupted:
        return 130
    non_ok = sum(1 for r in records if r.get("status") != "ok")
    return 0 if non_ok == 0 else 2


if __name__ == "__main__":
    sys.exit(main())

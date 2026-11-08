#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""fake_chain_test.py —— 假语音链路发布器 + latency_probe 自测验证。

两个用途
--------
1. **publish 模式**（模拟生产节点）：
   在 ROS_DOMAIN_ID=42（自测隔离域）里发布与真实链路一致的话题时序：
   ``/question_asr → /llm_to_dashapp → /ali_llm_output([START]/分块/[DONE])
   → /tts_realtime_data(分句/[DONE]) → /tts_session_finished``，
   并按埋点契约发 ``/latency_event``（asr / tts_local 事件，t_ns 为 epoch ns）。

2. **selftest 模式**（默认）：
   自动拉起 publish 子进程 + 运行 ``latency_probe.py``，读取 summary JSON，
   把实测值与理论值逐项比对，打印 PASS/FAIL 表；退出码 0=全部通过。

自测命令（推荐用 run_selftest.sh，也可手动）::

    source /opt/ros/jazzy/setup.bash
    source /home/artrc/medical_ws/setup_env.sh
    export ROS_DOMAIN_ID=42
    conda run -n qwen_tts_online --no-capture-output \
        python fake_chain_test.py --suite all -v

注意：自测强制使用 ROS_DOMAIN_ID=42，避免与生产域 0 的机器人仿真节点互相干扰。
"""

from __future__ import annotations

import argparse
import json
import os
import signal
import subprocess
import sys
import tempfile
import threading
import time
from datetime import datetime
from typing import Any, Dict, List, Optional, Tuple

_HERE = os.path.dirname(os.path.abspath(__file__))
PROBE_PATH = os.path.join(_HERE, "latency_probe.py")
FAKE_PATH = os.path.abspath(__file__)

# ---------------------------------------------------------------- 默认时序参数
DEFAULTS = {
    "router_delay_ms": 30.0,          # /question_asr -> 路由话题
    "first_chunk_delay_ms": 150.0,    # /question_asr -> 首个 LLM 文本块（TTFT）
    "chunk_interval_ms": 80.0,        # LLM 文本块间隔
    "llm_duration_ms": 600.0,         # 首块 -> [DONE]
    "tts_first_delay_ms": 200.0,      # 首个 LLM 块 -> 首个 /tts_realtime_data 文本
    "tts_text_gap_ms": 600.0,         # 相邻分句文本间隔
    "tts_sentences": 2,               # 分句数量
    "tts_rx_offset_ms": 5.0,          # 文本到达 -> TTS 节点 rx 埋点
    "tts_synth_start_offset_ms": 20.0,
    "tts_synth_ms": 800.0,            # rx -> synth_end（首句合成）
    "tts_play_start_ms": 1200.0,      # rx -> play_start（首音）
    "tts_play_dur_ms": 1200.0,        # 每段播放时长
    "session_finished_delay_ms": 50.0,  # 末段 play_end -> session_finished
    "asr_kws_delay_ms": 100.0,        # 唤醒模式：轮次开始 -> kws_detected
    "asr_endpoint_offset_ms": 800.0,  # start_asr -> endpoint（问句说完）
    "asr_callback_offset_ms": 20.0,   # endpoint -> asr_callback
    "asr_nurse_req_offset_ms": 50.0,  # endpoint -> nurse_req
    "asr_nurse_resp_offset_ms": 300.0,  # endpoint -> nurse_resp
    "asr_publish_question_offset_ms": 305.0,  # endpoint -> publish_question
    "asr_question_publish_offset_ms": 2.0,    # publish_question -> /question_asr 发布
}


# ===========================================================================
# publish 模式：假链路节点
# ===========================================================================
def run_publisher(args: argparse.Namespace) -> int:
    import rclpy
    from rclpy.node import Node
    from rclpy.qos import QoSProfile, ReliabilityPolicy, HistoryPolicy
    from std_msgs.msg import Bool, String

    cfg = vars(args).copy()

    class FakeChain(Node):
        def __init__(self):
            super().__init__(f"fake_chain_{os.getpid()}")
            qos = QoSProfile(depth=500, reliability=ReliabilityPolicy.RELIABLE,
                             history=HistoryPolicy.KEEP_LAST)
            self._busy = threading.Lock()
            self._threads: List[threading.Thread] = []
            self._stopping = False
            # 发布者
            self.pub_llm_routed = self.create_publisher(String, "/llm_to_dashapp", 10)
            self.pub_llm = self.create_publisher(String, "/ali_llm_output", 100)
            self.pub_tts_text = self.create_publisher(String, "/tts_realtime_data", 100)
            self.pub_session = self.create_publisher(String, "/tts_session_finished", 10)
            self.pub_question = self.create_publisher(String, "/question_asr", 10)
            self.pub_event = self.create_publisher(String, "/latency_event", 200)
            # 订阅者（注意：--mode 是 publish/selftest，触发方式看 --trigger）
            if args.trigger == "inject":
                self.create_subscription(String, "/question_asr", self._on_question, qos)
            else:  # asr：由探针轮次标记 / start_asr 驱动
                self.create_subscription(String, "/latency_probe_round", self._on_marker, qos)
                if args.wake_kws:
                    self.create_subscription(Bool, "/start_asr", self._on_start_asr, qos)
            self.get_logger().info(
                f"fake chain ready mode={args.mode} domain={os.environ.get('ROS_DOMAIN_ID')}"
            )
            # 模仿真实节点启动时的埋点
            now = time.time_ns()
            self._emit_event("asr", "init_done", now, {})
            self._emit_event("tts_local", "model_warmup", now + 1, {"dur_ms": 1.0})
            if args.ready_file:
                os.makedirs(os.path.dirname(os.path.abspath(args.ready_file)) or ".", exist_ok=True)
                with open(args.ready_file, "w", encoding="utf-8") as f:
                    json.dump({"pid": os.getpid(), "node": self.get_name(),
                               "mode": args.mode, "ready_ns": time.time_ns(),
                               "config": cfg}, f, ensure_ascii=False)

        # ---------- 埋点 / 话题发布 ----------
        def _emit_event(self, node: str, stage: str, t_ns: int,
                        detail: Optional[Dict[str, Any]] = None) -> None:
            payload = {"node": node, "stage": stage, "t_ns": int(t_ns),
                       "detail": detail or {}}
            self.pub_event.publish(String(data=json.dumps(payload, ensure_ascii=False)))

        def _publish(self, pub, data: str) -> None:
            pub.publish(String(data=data))

        # ---------- 调度器 ----------
        def _schedule(self, t_ns: int, fn, label: str) -> None:
            def _run() -> None:
                try:
                    delay = (t_ns - time.time_ns()) / 1e9
                    if delay > 0.002:
                        time.sleep(delay - 0.001)
                    while time.time_ns() < t_ns:
                        pass
                    if not self._stopping:
                        fn()
                except Exception as exc:  # noqa: BLE001
                    self.get_logger().error(f"schedule[{label}] failed: {exc!r}")

            th = threading.Thread(target=_run, name=f"sched-{label}", daemon=True)
            th.start()
            self._threads.append(th)

        def _chain_actions(self, t_ref_ns: int, tag: str) -> List[Tuple[int, str, Any]]:
            """构造从 /question_asr 时刻 t_ref 开始的下游动作（已按时间排序）。"""
            a: List[Tuple[int, str, Any]] = []
            ms = lambda v: int(round(v * 1e6))  # noqa: E731
            t_router = t_ref_ns + ms(args.router_delay_ms)
            t_first = t_ref_ns + ms(args.first_chunk_delay_ms)
            t_llm_done = t_first + ms(args.llm_duration_ms)

            a.append((t_router, "router", lambda: self._publish(
                self.pub_llm_routed, args.question_text)))
            a.append((t_first - ms(5), "llm_start", lambda: self._publish(
                self.pub_llm, "[START]")))
            # LLM 流式分块
            n_chunks = max(1, int(round(args.llm_duration_ms / args.chunk_interval_ms)))
            chunk_texts = ["病人", "血压", "偏高，", "建议", "先安静", "休息，", "不要", "紧张。"]
            for i in range(n_chunks):
                t = t_first + ms(i * args.chunk_interval_ms)
                txt = chunk_texts[i % len(chunk_texts)]
                a.append((t, f"llm_chunk{i}", lambda txt=txt: self._publish(
                    self.pub_llm, txt)))
            a.append((t_llm_done, "llm_done", lambda: self._publish(
                self.pub_llm, "[DONE]")))

            # TTS 分句文本 + tts_local 埋点
            play_ends: List[int] = []
            for i in range(args.tts_sentences):
                t_text = t_first + ms(args.tts_first_delay_ms + i * args.tts_text_gap_ms)
                t_rx = t_text + ms(args.tts_rx_offset_ms)
                t_synth_start = t_rx + ms(args.tts_synth_start_offset_ms)
                t_synth_end = t_rx + ms(args.tts_synth_ms)
                t_play_start = t_rx + ms(args.tts_play_start_ms)
                t_play_end = t_play_start + ms(args.tts_play_dur_ms)
                play_ends.append(t_play_end)
                txt = f"测试分句{i + 1}。"
                a.append((t_text, f"tts_text{i}", lambda txt=txt: self._publish(
                    self.pub_tts_text, txt)))
                a.append((t_rx, f"tts_rx{i}",
                          lambda t_rx=t_rx, i=i, txt=txt: self._emit_event(
                              "tts_local", "rx", t_rx,
                              {"text_len": len(txt), "head": txt})))
                a.append((t_synth_start, f"tts_synth_start{i}",
                          lambda t_synth_start=t_synth_start, i=i: self._emit_event(
                              "tts_local", "synth_start", t_synth_start,
                              {"text_len": 5, "sentence": i})))
                a.append((t_synth_end, f"tts_synth_end{i}",
                          lambda t_synth_end=t_synth_end, i=i, txt=txt: self._emit_event(
                              "tts_local", "synth_end", t_synth_end,
                              {"text_len": len(txt), "dur_ms": args.tts_synth_ms,
                               "audio_sec": args.tts_play_dur_ms / 1000.0, "sentence": i})))
                a.append((t_play_start, f"tts_play_start{i}",
                          lambda t_play_start=t_play_start, i=i, txt=txt: self._emit_event(
                              "tts_local", "play_start", t_play_start,
                              {"text_len": len(txt), "head": txt, "sentence": i})))
                a.append((t_play_end, f"tts_play_end{i}",
                          lambda t_play_end=t_play_end, i=i: self._emit_event(
                              "tts_local", "play_end", t_play_end,
                              {"dur_ms": args.tts_play_dur_ms, "sentence": i})))

            t_tts_done = t_llm_done + ms(5)
            a.append((t_tts_done, "tts_done", lambda: self._publish(
                self.pub_tts_text, "[DONE]")))
            t_rx_control = t_tts_done + ms(1)
            a.append((t_rx_control, "tts_rx_control_done",
                      lambda: self._emit_event("tts_local", "rx_control", t_rx_control,
                                               {"control": ["DONE"]})))
            t_session = max(play_ends) + ms(args.session_finished_delay_ms)
            a.append((t_session, "session_finished_topic", lambda: self._publish(
                self.pub_session, "finished")))
            a.append((t_session + ms(1), "session_finished_event",
                      lambda: self._emit_event("tts_local", "session_finished",
                                               t_session, {})))
            a.sort(key=lambda x: (x[0], x[1]))
            return a

        def _start_chain(self, t_ref_ns: int, tag: str) -> None:
            if not self._busy.acquire(blocking=False):
                self.get_logger().warning("fake chain busy, ignore overlapping question")
                return
            actions = self._chain_actions(t_ref_ns, tag)

            def _runner() -> None:
                try:
                    for t_ns, label, fn in actions:
                        delay = (t_ns - time.time_ns()) / 1e9
                        if delay > 0.002:
                            time.sleep(delay - 0.001)
                        while time.time_ns() < t_ns:
                            pass
                        if self._stopping:
                            return
                        fn()
                finally:
                    self._busy.release()

            th = threading.Thread(target=_runner, name=f"chain-{tag}", daemon=True)
            th.start()
            self._threads.append(th)
            self.get_logger().info(f"fake chain started ({tag})")

        # ---------- 回调 ----------
        def _on_question(self, msg: String) -> None:
            t = time.time_ns()
            self.get_logger().info(f"fake chain <- /question_asr: {msg.data!r}")
            self._start_chain(t, "inject")

        def _on_marker(self, msg: String) -> None:
            try:
                obj = json.loads(msg.data)
            except Exception:  # noqa: BLE001
                return
            if obj.get("event") != "round_start":
                return
            # 清除上一轮 start_asr 状态
            self._marker_ns = time.time_ns()
            self._round_index = obj.get("round")
            self.get_logger().info(f"fake chain <- round marker {obj.get('round')}")
            if args.wake_kws:
                t_kws = self._marker_ns + int(args.asr_kws_delay_ms * 1e6)
                self._schedule(t_kws, lambda: self._emit_event(
                    "asr", "kws_detected", t_kws, {"keyword": "小医"}), "kws")
            else:
                # question-wav 无唤醒：直接按固定延迟模拟 endpoint→question_asr
                self._schedule_question_chain(self._marker_ns, "question-only")

        def _on_start_asr(self, msg: Bool) -> None:
            if not args.wake_kws:
                return
            t = time.time_ns()
            self.get_logger().info("fake chain <- /start_asr, simulate ASR chain")
            self._schedule(t, lambda: self._emit_event("asr", "start_asr", t, {}), "start_asr")
            self._schedule_question_chain(t, "wake")

        def _schedule_question_chain(self, base_ns: int, tag: str) -> None:
            base = base_ns
            ms = lambda v: int(round(v * 1e6))  # noqa: E731
            t_endpoint = base + ms(args.asr_endpoint_offset_ms)
            t_cb = t_endpoint + ms(args.asr_callback_offset_ms)
            t_nreq = t_endpoint + ms(args.asr_nurse_req_offset_ms)
            t_nresp = t_endpoint + ms(args.asr_nurse_resp_offset_ms)
            t_pubq = t_endpoint + ms(args.asr_publish_question_offset_ms)
            t_qpub = t_pubq + ms(args.asr_question_publish_offset_ms)
            actions: List[Tuple[int, str, Any]] = [
                (t_endpoint, "asr_endpoint", lambda: self._emit_event(
                    "asr", "endpoint", t_endpoint, {"text_len": 6})),
                (t_cb, "asr_callback", lambda: self._emit_event(
                    "asr", "asr_callback", t_cb, {"text_len": 6})),
                (t_nreq, "asr_nurse_req", lambda: self._emit_event(
                    "asr", "nurse_req", t_nreq, {"text_len": 6})),
                (t_nresp, "asr_nurse_resp", lambda: self._emit_event(
                    "asr", "nurse_resp", t_nresp, {"need_call": False, "need_end": False})),
                (t_pubq, "asr_publish_question", lambda: self._emit_event(
                    "asr", "publish_question", t_pubq, {"text_len": 6, "head": "血压偏高"})),
                (t_qpub, "question_asr", lambda: self._publish(
                    self.pub_question, args.question_text)),
            ]
            actions.sort(key=lambda x: (x[0], x[1]))
            for t_ns, label, fn in actions:
                self._schedule(t_ns, fn, label)
            self._schedule(t_qpub + ms(1),
                           lambda: self._start_chain(t_qpub, f"asr-{tag}"), "start_chain")

    rclpy.init(args=None)
    node = FakeChain()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node._stopping = True
        node.destroy_node()
        try:
            rclpy.shutdown()
        except Exception:  # noqa: BLE001
            pass
    return 0


# ===========================================================================
# selftest 模式：拉起假链路 + 探针，比对理论值
# ===========================================================================
def _child_env() -> Dict[str, str]:
    env = dict(os.environ)
    domain = env.get("ROS_DOMAIN_ID", "")
    if domain in ("", "0"):
        env["ROS_DOMAIN_ID"] = "42"
        print(f"[selftest] 强制子进程 ROS_DOMAIN_ID=42（原值 {domain!r}），自测与生产域 0 隔离")
    env["PYTHONUNBUFFERED"] = "1"
    return env


def _wait_ready(path: str, proc: subprocess.Popen, timeout_s: float) -> bool:
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        if os.path.exists(path) and os.path.getsize(path) > 0:
            return True
        if proc.poll() is not None:
            return False
        time.sleep(0.05)
    return False


def _run_cmd(cmd: List[str], env: Dict[str, str], log_path: str,
             timeout_s: float) -> Tuple[int, str]:
    with open(log_path, "w", encoding="utf-8") as log:
        log.write("$ " + " ".join(cmd) + "\n")
        log.flush()
        try:
            proc = subprocess.run(cmd, env=env, stdout=log, stderr=subprocess.STDOUT,
                                  timeout=timeout_s)
            code = proc.returncode
        except subprocess.TimeoutExpired:
            code = -9
            log.write(f"\n[selftest] timeout after {timeout_s}s\n")
    with open(log_path, "r", encoding="utf-8", errors="replace") as f:
        return code, f.read()


def _expected_inject(cfg: Dict[str, Any]) -> Dict[str, Tuple[float, float]]:
    """返回 {metric: (expected_ms, tol_ms)}。"""
    rx = cfg["tts_rx_offset_ms"]
    first = cfg["first_chunk_delay_ms"]
    tts0 = cfg["tts_first_delay_ms"]
    n = cfg["tts_sentences"]
    last_text = first + tts0 + (n - 1) * cfg["tts_text_gap_ms"]
    return {
        "question_to_router_ms": (cfg["router_delay_ms"], 80),
        "question_to_llm_first_ms": (first, 80),
        "llm_first_to_done_ms": (cfg["llm_duration_ms"], 30),
        "llm_first_to_tts_first_text_ms": (tts0, 80),
        "tts_rx_to_synth_end_ms": (cfg["tts_synth_ms"], 30),
        "tts_rx_to_play_start_ms": (cfg["tts_play_start_ms"], 30),
        "first_tts_text_to_first_play_start_ms": (rx + cfg["tts_play_start_ms"], 80),
        "question_to_first_audio_ms": (first + tts0 + rx + cfg["tts_play_start_ms"], 100),
        "question_to_session_finished_ms": (
            last_text + rx + cfg["tts_play_start_ms"] + cfg["tts_play_dur_ms"]
            + cfg["session_finished_delay_ms"], 150),
        "audio_total_sum_ms": (n * cfg["tts_play_dur_ms"], 20),
        "audio_first_to_last_ms": (
            (n - 1) * cfg["tts_text_gap_ms"] + cfg["tts_play_dur_ms"], 100),
    }


def _expected_asr(cfg: Dict[str, Any]) -> Dict[str, Tuple[float, float]]:
    rx = cfg["tts_rx_offset_ms"]
    first = cfg["first_chunk_delay_ms"]
    tts0 = cfg["tts_first_delay_ms"]
    n = cfg["tts_sentences"]
    last_text = first + tts0 + (n - 1) * cfg["tts_text_gap_ms"]
    endpoint_to_q = (cfg["asr_publish_question_offset_ms"]
                     + cfg["asr_question_publish_offset_ms"])
    q_to_first_audio = first + tts0 + rx + cfg["tts_play_start_ms"]
    return {
        "asr_endpoint_to_nurse_req_ms": (
            cfg["asr_nurse_req_offset_ms"], 30),
        "asr_nurse_req_to_nurse_resp_ms": (
            cfg["asr_nurse_resp_offset_ms"] - cfg["asr_nurse_req_offset_ms"], 30),
        "asr_start_asr_to_endpoint_ms": (cfg["asr_endpoint_offset_ms"], 80),
        "asr_endpoint_to_question_rx_ms": (endpoint_to_q, 80),
        "question_to_first_audio_ms": (q_to_first_audio, 150),
        "speech_end_to_first_audio_ms": (
            endpoint_to_q + q_to_first_audio + 2400.0, 200),
        "question_to_session_finished_ms": (
            last_text + rx + cfg["tts_play_start_ms"] + cfg["tts_play_dur_ms"]
            + cfg["session_finished_delay_ms"], 200),
        "audio_total_sum_ms": (n * cfg["tts_play_dur_ms"], 20),
    }


def _compare_table(name: str, expected: Dict[str, Tuple[float, float]],
                   summary: Dict[str, Any], verbose: bool) -> bool:
    stats = summary.get("metrics") or {}
    print(f"\n### {name} 校验（实测 mean vs 理论值，单位 ms）")
    print("| 指标 | 理论值 | 实测 mean | 偏差 | 容差 | 结果 |")
    print("| --- | ---: | ---: | ---: | ---: | :---: |")
    ok_all = True
    for metric, (exp, tol) in expected.items():
        s = stats.get(metric)
        if not s:
            print(f"| {metric} | {exp:.1f} | — | — | {tol:.0f} | **FAIL(缺失)** |")
            ok_all = False
            continue
        got = float(s["mean_ms"])
        diff = got - exp
        ok = abs(diff) <= tol
        ok_all = ok_all and ok
        print(f"| {metric} | {exp:.1f} | {got:.1f} | {diff:+.1f} | {tol:.0f} | "
              f"{'PASS' if ok else '**FAIL**'} |")
    if verbose:
        print(f"\n全部实测统计（{name}）：")
        for k, s in stats.items():
            print(f"  {k}: n={s['count']} mean={s['mean_ms']} "
                  f"median={s['median_ms']} p95={s['p95_ms']}")
    return ok_all


def run_selftest(args: argparse.Namespace) -> int:
    env = _child_env()
    tmpdir = tempfile.mkdtemp(prefix="latency_probe_selftest_")
    print(f"[selftest] workdir: {tmpdir}")
    print(f"[selftest] python: {sys.executable}")
    print(f"[selftest] ROS_DOMAIN_ID={env.get('ROS_DOMAIN_ID')}")
    cfg = {k: getattr(args, k) for k in DEFAULTS}
    all_ok = True

    suites = []
    if args.suite in ("inject", "all"):
        suites.append("inject")
    if args.suite in ("asr", "all"):
        suites.append("asr")

    for suite in suites:
        ready = os.path.join(tmpdir, f"ready_{suite}.json")
        fake_log = os.path.join(tmpdir, f"fake_{suite}.log")
        probe_log = os.path.join(tmpdir, f"probe_{suite}.log")
        record_dir = os.path.join(tmpdir, "records")
        tag = f"selftest_{suite}"

        fake_cmd = [sys.executable, FAKE_PATH, "--mode", "publish",
                    "--ready-file", ready,
                    "--trigger", ("asr" if suite == "asr" else "inject")]
        if suite == "asr":
            fake_cmd += ["--wake-kws"]
        for k, v in cfg.items():
            fake_cmd += [f"--{k.replace('_', '-')}", str(v)]
        print(f"\n[selftest] 启动假链路: {' '.join(fake_cmd)}")
        fake_out = open(fake_log, "w", encoding="utf-8")
        fake_proc = subprocess.Popen(fake_cmd, env=env, stdout=fake_out,
                                     stderr=subprocess.STDOUT, text=True,
                                     start_new_session=True)
        try:
            if not _wait_ready(ready, fake_proc, args.startup_timeout):
                fake_out.flush()
                try:
                    with open(fake_log, "r", encoding="utf-8", errors="replace") as f:
                        out = f.read()
                except OSError:
                    out = ""
                print("[selftest] FAIL: 假链路未就绪\n" + out[-4000:], file=sys.stderr)
                return 1
            time.sleep(1.0)  # 等 DDS 发现完成
            if suite == "inject":
                probe_cmd = [
                    sys.executable, PROBE_PATH,
                    "--record-dir", record_dir, "--tag", tag,
                    "--repeat", str(args.repeat), "--interval", str(args.interval),
                    "--inject-text", args.question_text,
                    "--timeout", "30", "--endpoint-silence", "2.4",
                ]
            else:
                probe_cmd = [
                    sys.executable, PROBE_PATH,
                    "--record-dir", record_dir, "--tag", tag,
                    "--repeat", str(args.repeat), "--interval", str(args.interval),
                    "--wake-wav", "/tmp/fake_wake.wav",
                    "--question-wav", "/tmp/fake_question.wav",
                    "--play-cmd", f"sleep {args.asr_fake_play_s}",
                    "--pre-delay", "0.2", "--kws-timeout", "8",
                    "--timeout", "30", "--endpoint-silence", "2.4",
                ]
            print(f"[selftest] 运行探针: {' '.join(probe_cmd)}")
            code, log = _run_cmd(probe_cmd, env, probe_log,
                                 timeout_s=args.probe_timeout)
            if fake_proc.poll() is not None:
                print(f"[selftest] FAIL: 假链路中途退出 rc={fake_proc.returncode}",
                      file=sys.stderr)
                fake_out.flush()
                try:
                    with open(fake_log, "r", encoding="utf-8", errors="replace") as f:
                        tail = f.read()
                except OSError:
                    tail = ""
                print("\n".join(tail.splitlines()[-40:]), file=sys.stderr)
                all_ok = False
                continue
            if code != 0:
                print(f"[selftest] FAIL: 探针退出码 {code}，日志尾部：", file=sys.stderr)
                print("\n".join(log.splitlines()[-60:]), file=sys.stderr)
                all_ok = False
                continue
            summary_path = os.path.join(record_dir, f"{tag}_summary.json")
            if not os.path.exists(summary_path):
                print(f"[selftest] FAIL: 缺少 summary {summary_path}", file=sys.stderr)
                all_ok = False
                continue
            with open(summary_path, "r", encoding="utf-8") as f:
                summary = json.load(f)
            # 基础校验
            checks = [
                (summary.get("run_count") == args.repeat, "run_count == repeat"),
                (summary.get("ok_count") == args.repeat, "ok_count == repeat"),
                (os.path.exists(os.path.join(record_dir, f"{tag}_stats.csv")),
                 "stats.csv 存在"),
                (os.path.exists(os.path.join(record_dir, f"{tag}_rounds.csv")),
                 "rounds.csv 存在"),
                (os.path.exists(os.path.join(record_dir, f"{tag}_stats.md")),
                 "stats.md 存在"),
            ]
            for ok, desc in checks:
                print(f"[selftest] {'PASS' if ok else 'FAIL'}: {desc}")
                all_ok = all_ok and ok
            # 圆整 JSON 存在
            for i in range(args.repeat):
                p = os.path.join(record_dir, f"{tag}_round{i:02d}.json")
                ok = os.path.exists(p)
                print(f"[selftest] {'PASS' if ok else 'FAIL'}: {os.path.basename(p)} 存在")
                all_ok = all_ok and ok
            expected = (_expected_inject(cfg) if suite == "inject"
                        else _expected_asr(cfg))
            all_ok = _compare_table(suite, expected, summary, args.verbose) and all_ok

            # 额外契约校验：asr 链回溯 / inject 模式无 asr 链
            for rnd in summary.get("rounds", []):
                rpath = rnd.get("round_file")
                if not rpath or not os.path.exists(rpath):
                    continue
                with open(rpath, "r", encoding="utf-8") as f:
                    rec = json.load(f)
                meta = rec.get("asr_chain_meta") or {}
                if suite == "asr":
                    ok = meta.get("publish_question_found") is True and \
                        len(rec.get("asr_chain") or {}) >= 3
                    print(f"[selftest] {'PASS' if ok else 'FAIL'}: round{rnd['round_index']} "
                          f"ASR 链回溯 publish_question_found={meta.get('publish_question_found')}")
                    all_ok = all_ok and ok
                else:
                    ok = not rec.get("asr_chain")
                    print(f"[selftest] {'PASS' if ok else 'FAIL'}: round{rnd['round_index']} "
                          f"inject 模式不关联 ASR 链")
                    all_ok = all_ok and ok
            print(f"[selftest] deliverable: {summary_path}")
        finally:
            # 整组结束：conda run 只是包装层，必须 kill 进程组才能带走真实节点进程
            try:
                os.killpg(os.getpgid(fake_proc.pid), signal.SIGTERM)
            except (ProcessLookupError, PermissionError):
                fake_proc.terminate()
            try:
                fake_proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                try:
                    os.killpg(os.getpgid(fake_proc.pid), signal.SIGKILL)
                except (ProcessLookupError, PermissionError):
                    fake_proc.kill()
            try:
                fake_out.close()
            except Exception:  # noqa: BLE001
                pass

    print()
    print("=" * 60)
    print(f"[selftest] 总结果: {'PASS' if all_ok else 'FAIL'}")
    print(f"[selftest] 产物目录: {tmpdir}")
    print("=" * 60)
    return 0 if all_ok else 1


# ===========================================================================
# CLI
# ===========================================================================
def parse_args(argv: Optional[List[str]] = None) -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description="假语音链路发布器 + latency_probe 自测验证（ROS_DOMAIN_ID=42 隔离）",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    p.add_argument("--mode", choices=["publish", "selftest"], default="selftest",
                   help="publish=只跑假链路节点；selftest=自动拉起假链路+探针并校验")
    p.add_argument("--suite", choices=["inject", "asr", "all"], default="all",
                   help="selftest 覆盖的触发方式")
    p.add_argument("--repeat", type=int, default=3, help="selftest 轮数")
    p.add_argument("--interval", type=float, default=0.8, help="selftest 轮间隔（秒）")
    p.add_argument("--question-text", default="病人血压偏高怎么办？")
    p.add_argument("--trigger", choices=["inject", "asr"], default="inject",
                   help="publish 模式的触发来源（inject=/question_asr；asr=轮次标记/start_asr）")
    p.add_argument("--wake-kws", action="store_true",
                   help="asr 触发模式下模拟唤醒：轮次标记后发 kws_detected，等 /start_asr")
    p.add_argument("--ready-file", default="", help="publish 模式就绪标记文件")
    p.add_argument("--startup-timeout", type=float, default=20.0)
    p.add_argument("--probe-timeout", type=float, default=180.0)
    p.add_argument("--asr-fake-play-s", type=float, default=0.25,
                   help="asr 自测时 --play-cmd 用 sleep 的时长（无需真实声卡）")
    p.add_argument("-v", "--verbose", action="store_true")
    for k, v in DEFAULTS.items():
        arg = "--" + k.replace("_", "-")
        typ = int if isinstance(v, int) else float
        p.add_argument(arg, type=typ, default=v)
    return p.parse_args(argv)


def main(argv: Optional[List[str]] = None) -> int:
    args = parse_args(argv)
    if args.mode == "publish":
        return run_publisher(args)
    return run_selftest(args)


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""latency_stats.py —— 延迟探针的统计/报告公共函数（不依赖 ROS）。

所有延迟指标的单位统一为 **毫秒 (ms)**，字段名以 ``_ms`` 结尾；
时间戳统一为 **epoch 纳秒 (ns)**，字段名以 ``_ns`` 结尾。

统计口径：
  count   有效轮次数（None / NaN 不计入）
  mean    算术平均
  median  中位数（偶数个取中间两数均值）
  p95     95 分位（nearest-rank：排序后第 ceil(0.95*n) 个）
  min/max 最小/最大
  stdev   样本标准差（n>1；n==1 记 0.0）
"""

from __future__ import annotations

import csv
import math
import os
from typing import Dict, Iterable, List, Optional, Sequence, Tuple

# ---------------------------------------------------------------------------
# 指标定义：(字段名, 中文标签, 所在链路阶段)
# 顺序即报告中的显示顺序；新增指标只需在此登记。
# ---------------------------------------------------------------------------
METRIC_ORDER: List[Tuple[str, str]] = [
    # ---- ASR 段（来自 /latency_event node=asr 的节点内 t_ns） ----
    ("asr_kws_to_start_asr_ms", "ASR: kws_detected→start_asr"),
    ("asr_start_asr_to_endpoint_ms", "ASR: start_asr→endpoint"),
    ("asr_endpoint_to_nurse_req_ms", "ASR: endpoint→nurse_req"),
    ("asr_nurse_req_to_nurse_resp_ms", "ASR: nurse_req→nurse_resp"),
    ("asr_nurse_resp_to_publish_question_ms", "ASR: nurse_resp→publish_question"),
    ("asr_endpoint_to_question_rx_ms", "ASR: endpoint→probe收到question"),
    ("asr_publish_question_to_rx_ms", "ASR: publish_question→probe收到"),
    # ---- 路由 / LLM / 断句 ----
    ("question_to_router_ms", "Router: question→路由话题到达"),
    ("router_to_llm_first_ms", "LLM: 路由话题→首块"),
    ("question_to_llm_first_ms", "LLM TTFT: question→首块"),
    ("llm_first_to_done_ms", "LLM 总时长: 首块→[DONE]"),
    ("question_to_llm_done_ms", "LLM: question→[DONE]"),
    ("llm_first_to_tts_first_text_ms", "断句: LLM首块→首个TTS文本"),
    ("question_to_tts_first_text_ms", "断句: question→首个TTS文本"),
    ("first_tts_text_to_first_play_start_ms", "断句: 首个TTS文本→首音"),
    # ---- TTS ----
    ("tts_rx_to_synth_end_ms", "TTS 首句合成: rx→synth_end"),
    ("tts_rx_to_play_start_ms", "TTS 首音: rx→play_start"),
    ("tts_synth_end_to_play_start_ms", "TTS 首句: synth_end→play_start"),
    # ---- 端到端 / 会话 ----
    ("question_to_first_audio_ms", "端到端: question→首个play_start"),
    ("speech_end_to_first_audio_ms", "语音结束→首音 (endpoint-静音阈值)"),
    ("playback_end_to_first_audio_ms", "问句播放结束→首音 (wav触发)"),
    ("question_to_session_finished_ms", "会话总时长: question→session_finished"),
    ("audio_total_sum_ms", "音频总时长(各段play相加)"),
    ("audio_first_to_last_ms", "音频跨距: 首play_start→末play_end"),
]

METRIC_LABEL: Dict[str, str] = dict(METRIC_ORDER)
METRIC_KEYS: List[str] = [k for k, _ in METRIC_ORDER]


# ---------------------------------------------------------------------------
# 基础统计
# ---------------------------------------------------------------------------
def _clean(values: Iterable[Optional[float]]) -> List[float]:
    out: List[float] = []
    for v in values:
        if v is None:
            continue
        try:
            f = float(v)
        except (TypeError, ValueError):
            continue
        if math.isnan(f) or math.isinf(f):
            continue
        out.append(f)
    return out


def percentile(sorted_values: Sequence[float], pct: float) -> float:
    """nearest-rank 分位数；sorted_values 必须已升序且非空。"""
    n = len(sorted_values)
    if n == 0:
        raise ValueError("empty sequence")
    rank = int(math.ceil(pct / 100.0 * n))
    rank = max(1, min(n, rank))
    return float(sorted_values[rank - 1])


def summarize(values: Iterable[Optional[float]]) -> Optional[Dict[str, float]]:
    """对一个指标的若干轮测量值做统计；无有效值返回 None。"""
    vals = _clean(values)
    n = len(vals)
    if n == 0:
        return None
    s = sorted(vals)
    mean = sum(vals) / n
    if n % 2 == 1:
        median = s[n // 2]
    else:
        median = (s[n // 2 - 1] + s[n // 2]) / 2.0
    if n > 1:
        var = sum((x - mean) ** 2 for x in vals) / (n - 1)
        stdev = math.sqrt(var)
    else:
        stdev = 0.0
    return {
        "count": n,
        "mean_ms": round(mean, 3),
        "median_ms": round(median, 3),
        "p95_ms": round(percentile(s, 95.0), 3),
        "min_ms": round(s[0], 3),
        "max_ms": round(s[-1], 3),
        "stdev_ms": round(stdev, 3),
    }


def build_stats(round_metrics: Sequence[Dict[str, Optional[float]]],
                extra_keys: Optional[Iterable[str]] = None) -> Dict[str, Dict[str, float]]:
    """round_metrics: 每轮一个 {metric_name: ms_value_or_None}。"""
    keys: List[str] = []
    for k in METRIC_KEYS:
        keys.append(k)
    for k in extra_keys or []:
        if k not in keys:
            keys.append(k)
    stats: Dict[str, Dict[str, float]] = {}
    for key in keys:
        values = [rm.get(key) for rm in round_metrics]
        s = summarize(values)
        if s is not None:
            stats[key] = s
    return stats


# ---------------------------------------------------------------------------
# 输出：Markdown 表 / CSV
# ---------------------------------------------------------------------------
def _fmt(v: Optional[float]) -> str:
    if v is None:
        return "—"
    return f"{v:.1f}"


def markdown_stats_table(stats: Dict[str, Dict[str, float]],
                         keys: Optional[Sequence[str]] = None) -> str:
    """生成 `stage,n,mean,median,p95,min,max,stdev` 的 markdown 表（ms）。"""
    if keys is None:
        keys = [k for k in METRIC_KEYS if k in stats]
        keys += [k for k in stats if k not in keys]
    lines = [
        "| 阶段(stage) | n | mean | median | p95 | min | max | stdev |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |",
    ]
    for key in keys:
        s = stats.get(key)
        if not s:
            lines.append(f"| {METRIC_LABEL.get(key, key)} | 0 | — | — | — | — | — | — |")
            continue
        lines.append(
            "| {label} | {n} | {mean} | {median} | {p95} | {min} | {max} | {stdev} |".format(
                label=METRIC_LABEL.get(key, key),
                n=s["count"],
                mean=_fmt(s["mean_ms"]),
                median=_fmt(s["median_ms"]),
                p95=_fmt(s["p95_ms"]),
                min=_fmt(s["min_ms"]),
                max=_fmt(s["max_ms"]),
                stdev=_fmt(s["stdev_ms"]),
            )
        )
    return "\n".join(lines)


def missing_metric_note(stats: Dict[str, Dict[str, float]]) -> str:
    missing = [k for k in METRIC_KEYS if k not in stats]
    if not missing:
        return ""
    labels = "、".join(METRIC_LABEL.get(k, k) for k in missing)
    return f"> 未采集到数据的指标（通常表示该链路环节未带埋点/未触发）：{labels}\n"


def write_stats_csv(path: str, stats: Dict[str, Dict[str, float]],
                    keys: Optional[Sequence[str]] = None) -> None:
    if keys is None:
        keys = [k for k in METRIC_KEYS if k in stats]
        keys += [k for k in stats if k not in keys]
    os.makedirs(os.path.dirname(os.path.abspath(path)) or ".", exist_ok=True)
    with open(path, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["metric", "label", "count", "mean_ms", "median_ms",
                    "p95_ms", "min_ms", "max_ms", "stdev_ms"])
        for key in keys:
            s = stats.get(key)
            if not s:
                continue
            w.writerow([key, METRIC_LABEL.get(key, key), s["count"], s["mean_ms"],
                        s["median_ms"], s["p95_ms"], s["min_ms"], s["max_ms"],
                        s["stdev_ms"]])


def write_rounds_csv(path: str, rounds: Sequence[Dict],
                     keys: Optional[Sequence[str]] = None) -> None:
    """每轮一行、每指标一列（宽表），便于 Excel/pandas 直接分析。"""
    if keys is None:
        keys = list(METRIC_KEYS)
    os.makedirs(os.path.dirname(os.path.abspath(path)) or ".", exist_ok=True)
    with open(path, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["round", "status", "t0_source"] + list(keys))
        for r in rounds:
            metrics = r.get("metrics") or {}
            row = [r.get("round_index"), r.get("status"), r.get("t0_source")]
            for k in keys:
                v = metrics.get(k)
                row.append("" if v is None else f"{float(v):.3f}")
            w.writerow(row)

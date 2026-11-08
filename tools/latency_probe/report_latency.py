#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""report_latency.py —— 读取 latency_probe 的 summary JSON，生成 Markdown 报告。

用法::

    # 读目录下所有 <tag>_summary.json
    python report_latency.py --record-dir ./records

    # 只读指定 tag，并写到文件
    python report_latency.py --record-dir ./records --tags demo,prod --out report_latency.md

    # 直接指定 summary JSON 文件
    python report_latency.py records/demo_summary.json records/prod_summary.json
"""

from __future__ import annotations

import argparse
import glob
import json
import os
import sys
from datetime import datetime
from typing import Any, Dict, List, Optional

_HERE = os.path.dirname(os.path.abspath(__file__))
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)

from latency_stats import (  # noqa: E402
    METRIC_LABEL,
    markdown_stats_table,
    missing_metric_note,
)

# 端到端透视表的列（字段名, 表头）
PERSPECTIVE = [
    ("question_to_llm_first_ms", "LLM TTFT"),
    ("llm_first_to_done_ms", "LLM 总时长"),
    ("first_llm_chunk_to_first_tts_text", None),  # 由 derived 取，见 _derived_mean
    ("first_tts_text_to_first_play_start_ms", "断句→首音"),
    ("question_to_first_audio_ms", "端到端(首音)"),
    ("speech_end_to_first_audio_ms", "语音结束→首音"),
    ("question_to_session_finished_ms", "会话总时长"),
    ("audio_total_sum_ms", "音频总时长"),
]


def load_summaries(record_dir: str, tags: List[str],
                   explicit: List[str]) -> List[Dict[str, Any]]:
    paths: List[str] = []
    for p in explicit:
        paths.extend(glob.glob(p))
    if record_dir:
        if tags:
            for tag in tags:
                paths.append(os.path.join(record_dir, f"{tag}_summary.json"))
        else:
            paths.extend(sorted(glob.glob(os.path.join(record_dir, "*_summary.json"))))
    out: List[Dict[str, Any]] = []
    seen = set()
    for p in paths:
        ab = os.path.abspath(p)
        if ab in seen:
            continue
        seen.add(ab)
        if not os.path.exists(ab):
            print(f"[warn] 找不到 summary: {p}", file=sys.stderr)
            continue
        with open(ab, "r", encoding="utf-8") as f:
            summary = json.load(f)
        summary["_path"] = ab
        out.append(summary)
    return out


def _fmt_pair(stats: Dict[str, Any], key: str, digits: int = 1) -> str:
    s = stats.get(key)
    if not s:
        return "—"
    return f"{s['mean_ms']:.{digits}f} ({s['p95_ms']:.{digits}f})"


def _mean_of_derived(summary: Dict[str, Any], rounds: List[Dict[str, Any]],
                     name: str) -> Optional[float]:
    """从每轮 JSON 的 derived 字段取指定指标 mean（需回落读取 round 文件）。"""
    vals: List[float] = []
    for r in rounds:
        path = r.get("round_file")
        if not path or not os.path.exists(path):
            continue
        try:
            with open(path, "r", encoding="utf-8") as f:
                rec = json.load(f)
        except (OSError, ValueError):
            continue
        v = ((rec.get("derived") or {}).get(name) or {}).get("ms")
        if v is not None:
            vals.append(float(v))
    if not vals:
        return None
    return sum(vals) / len(vals)


def _perspective_value(summary: Dict[str, Any], rounds: List[Dict[str, Any]],
                       key: str) -> str:
    stats = summary.get("metrics") or {}
    if key == "first_llm_chunk_to_first_tts_text":
        mean = _mean_of_derived(summary, rounds, "first_llm_chunk_to_first_tts_text")
        return "—" if mean is None else f"{mean:.1f}"
    return _fmt_pair(stats, key)


def build_report(summaries: List[Dict[str, Any]]) -> str:
    lines: List[str] = []
    lines.append("# 语音全链路延迟测量报告")
    lines.append("")
    lines.append(f"- 生成时间：{datetime.now().isoformat(timespec='seconds')}")
    lines.append(f"- 测量工具：latency_probe（单进程同钟 + /latency_event 埋点拼接）")
    lines.append(f"- 报告 tag 数：{len(summaries)}")
    lines.append("")
    lines.append("> 表中数值单位均为 **毫秒(ms)**；`mean (p95)` 表示均值与 95 分位。"
                 "端到端透视按 tag 汇总每轮 mean/p95。")
    lines.append("")

    # ---- 端到端透视 ----
    lines.append("## 端到端透视")
    lines.append("")
    header = "| tag | mode | n | " + " | ".join(
        (label or key) for key, label in PERSPECTIVE) + " |"
    sep = "| --- | --- | ---: | " + " | ".join("---:" for _ in PERSPECTIVE) + " |"
    lines.append(header)
    lines.append(sep)
    for s in summaries:
        rounds = s.get("rounds") or []
        row = [s.get("tag", "?"), s.get("mode", "?"), str(s.get("run_count", 0))]
        for key, _label in PERSPECTIVE:
            row.append(_perspective_value(s, rounds, key))
        lines.append("| " + " | ".join(row) + " |")
    lines.append("")

    # ---- 各 tag 明细 ----
    for s in summaries:
        tag = s.get("tag", "?")
        mode = s.get("mode", "?")
        cfg = s.get("config") or {}
        lines.append(f"## tag: `{tag}`（mode={mode}, n={s.get('run_count')}, "
                     f"ok={s.get('ok_count')}）")
        lines.append("")
        lines.append(f"- summary: `{s.get('_path')}`")
        lines.append(f"- 触发参数：inject_text={cfg.get('inject_text')!r}, "
                     f"question_wav={cfg.get('question_wav')!r}, "
                     f"wake_wav={cfg.get('wake_wav')!r}")
        lines.append(f"- endpoint 静音阈值：{cfg.get('endpoint_silence_s')}s；"
                     f"事件回溯窗口：{cfg.get('event_lookback_s')}s；"
                     f"轮间隔：{cfg.get('interval_s')}s")
        if s.get("non_ok"):
            lines.append(f"- ⚠️ 非 ok 轮次：{json.dumps(s['non_ok'], ensure_ascii=False)}")
        lines.append("")
        stats = s.get("metrics") or {}
        if stats:
            lines.append(markdown_stats_table(stats))
            lines.append("")
            note = missing_metric_note(stats)
            if note:
                lines.append(note)
                lines.append("")
        else:
            lines.append("（无有效指标）")
            lines.append("")
        # 每轮端到端
        rounds = s.get("rounds") or []
        if rounds:
            lines.append("<details><summary>每轮端到端（question→首音 / 会话总时长 / 状态）"
                         "</summary>")
            lines.append("")
            lines.append("| round | t0_source | question→首音(ms) | 语音结束→首音(ms) | "
                         "会话总时长(ms) | status |")
            lines.append("| ---: | --- | ---: | ---: | ---: | --- |")
            for r in rounds:
                m = r.get("metrics") or {}
                def f(v):
                    return "—" if v is None else f"{float(v):.1f}"
                lines.append("| {i} | {t0} | {e2e} | {spe} | {ses} | {st} |".format(
                    i=r.get("round_index"), t0=r.get("t0_source"),
                    e2e=f(m.get("question_to_first_audio_ms")),
                    spe=f(m.get("speech_end_to_first_audio_ms")),
                    ses=f(m.get("question_to_session_finished_ms")),
                    st=r.get("status")))
            lines.append("")
            lines.append("</details>")
            lines.append("")
    lines.append("")
    lines.append("---")
    lines.append("")
    lines.append("### 指标说明")
    lines.append("")
    for key, label in METRIC_LABEL.items():
        lines.append(f"- `{key}`：{label}")
    lines.append("")
    return "\n".join(lines)


def parse_args(argv: Optional[List[str]] = None) -> argparse.Namespace:
    p = argparse.ArgumentParser(description="把 latency_probe summary JSON 渲染成 Markdown 报告")
    p.add_argument("summary_json", nargs="*", help="直接指定 summary JSON 文件（可选）")
    p.add_argument("--record-dir", default="",
                   help="记录目录（读取 <tag>_summary.json / 全部 *_summary.json）")
    p.add_argument("--tags", default="",
                   help="逗号分隔的 tag 列表；缺省读取 record-dir 下全部 summary")
    p.add_argument("--out", default="", help="输出 Markdown 文件路径；缺省打印到 stdout")
    return p.parse_args(argv)


def main(argv: Optional[List[str]] = None) -> int:
    args = parse_args(argv)
    tags = [t.strip() for t in args.tags.split(",") if t.strip()]
    if not args.record_dir and not args.summary_json:
        args.record_dir = os.path.join(_HERE, "records")
    summaries = load_summaries(args.record_dir, tags, args.summary_json)
    if not summaries:
        print("[error] 没有找到任何 summary JSON", file=sys.stderr)
        return 1
    report = build_report(summaries)
    if args.out:
        out = os.path.abspath(args.out)
        os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
        with open(out, "w", encoding="utf-8") as f:
            f.write(report + "\n")
        print(f"[report] 已写入 {out}（{len(summaries)} 个 tag）")
    else:
        print(report)
    return 0


if __name__ == "__main__":
    sys.exit(main())

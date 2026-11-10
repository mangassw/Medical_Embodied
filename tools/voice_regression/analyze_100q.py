#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""分析 measure_100q 的产物：每题分段耗时 CSV + 全局分位数统计。

产物：
  out/measure_rows.json   每题行（驱动产出的总体时间点）
  out/measure_events.jsonl  原始事件（llm/lat 埋点）
输出：
  out/measure_detail.csv  每题分段明细（CSV，可直接给 Excel）
  out/measure_summary.md  分位数汇总（中位/p90/最大）
"""
import json
import os
import statistics as st

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "out")


def pct(vals, p):
    vals = sorted(v for v in vals if v is not None)
    if not vals:
        return None
    k = int(round((len(vals) - 1) * p / 100))
    return vals[k]


def main():
    rows = json.load(open(os.path.join(OUT, "measure_rows.json"), encoding="utf-8"))
    events = [json.loads(l) for l in open(os.path.join(OUT, "measure_events.jsonl"),
                                          encoding="utf-8")]

    # 事件按题切分：以每次 llm_start（[START]）为界
    # 收集每题的 lat 事件：intent/rag/llm_first_chunk/synth/play 等
    per_q = []
    cur = None
    for e in events:
        if e["kind"] == "llm_start":
            if cur is not None:
                per_q.append(cur)
            cur = {"t_start_llm": e["t"], "lats": [], "first_delta": None,
                   "done": None, "finished": None}
        elif cur is not None:
            if e["kind"] == "llm_first_delta" and cur["first_delta"] is None:
                cur["first_delta"] = e["t"]
            elif e["kind"] == "llm_done":
                cur["done"] = e["t"]
            elif e["kind"] == "tts_finished":
                cur["finished"] = e["t"]
            elif e["kind"] == "lat":
                cur["lats"].append(e)
    if cur is not None:
        per_q.append(cur)

    detail = []
    for i, row in enumerate(rows):
        q = per_q[i] if i < len(per_q) else {}
        lats = q.get("lats", [])

        def stage(name):
            vals = [x for x in lats if x["data"].get("stage") == name]
            return vals

        rag_s = stage("rag_start")
        rag_e = stage("rag_end")
        rag_ms = None
        if rag_s and rag_e:
            rag_ms = round((rag_e[0]["t"] - rag_s[0]["t"]) * 1000)

        synth_e = stage("synth_end")
        synth_ms = [round((x["data"].get("detail") or {}).get("dur_ms") or 0)
                    for x in synth_e]
        play_segs = len(stage("play_end"))

        first_audio = None
        play_start = stage("play_start")
        filler = stage("filler_enqueued")
        # TTS 首个回答音频起播（filler 之后的第一个 play_start）
        if play_start:
            t0 = row.get("t_pub")
            marks = [x for x in play_start if t0 and x["t"] > t0]
            if len(marks) >= 2:  # 第 1 个是 filler
                first_audio = round((marks[1]["t"] - t0) * 1000)
            elif marks:
                first_audio = round((marks[0]["t"] - t0) * 1000)

        detail.append({
            "id": row["id"],
            "ok": row["ok"],
            "intent_ms": row["question_to_start_ms"],
            "rag_ms": rag_ms,
            "ttft_ms": row["question_to_first_delta_ms"],  # 含网关处理+云端建连
            "gen_ms": row["delta_to_done_ms"],
            "tts_first_audio_ms": first_audio,
            "synth_units": len(synth_ms),
            "synth_total_ms": sum(synth_ms) if synth_ms else None,
            "play_segments": play_segs,
            "total_ms": row["question_to_finished_ms"],
            "question": row["question"],
        })

    # CSV
    cols = ["id", "ok", "intent_ms", "rag_ms", "ttft_ms", "gen_ms",
            "tts_first_audio_ms", "synth_units", "synth_total_ms",
            "play_segments", "total_ms", "question"]
    with open(os.path.join(OUT, "measure_detail.csv"), "w", encoding="utf-8-sig") as f:
        f.write(",".join(cols) + "\n")
        for d in detail:
            f.write(",".join('""' + str(d[c]).replace('"', "'") + '""'
                             if c == "question" else str(d[c]) for c in cols) + "\n")

    # 汇总
    ok = [d for d in detail if d["ok"]]
    def line(name, key):
        vals = [d[key] for d in ok if d[key] is not None]
        if not vals:
            return f"| {name} | - | - | - | - |"
        return (f"| {name} | {st.median(vals):.0f} | {pct(vals, 90):.0f} | "
                f"{max(vals):.0f} | {min(vals):.0f} |")

    md = ["# 100 题全链路耗时测量（单位 ms）", "",
          f"- 完成 {len(ok)}/{len(detail)} 题（超时/失败 {len(detail)-len(ok)}）", "",
          "| 阶段 | 中位 | P90 | 最大 | 最小 |", "| --- | ---: | ---: | ---: | ---: |",
          line("网关接收→开始生成(intent)", "intent_ms"),
          line("RAG 检索", "rag_ms"),
          line("LLM 首包(注入→首delta，含建连)", "ttft_ms"),
          line("LLM 生成(first→DONE)", "gen_ms"),
          line("首回答音频起播(注入起算)", "tts_first_audio_ms"),
          line("合成总耗时(每单元合计)", "synth_total_ms"),
          line("端到端(注入→播报完成)", "total_ms"), ""]
    units = [d["synth_units"] for d in ok if d["synth_units"]]
    if units:
        md.append(f"- 每题合成单元数：中位 {st.median(units):.0f}，最大 {max(units)}")
    with open(os.path.join(OUT, "measure_summary.md"), "w", encoding="utf-8") as f:
        f.write("\n".join(md) + "\n")
    print("\n".join(md))


if __name__ == "__main__":
    main()

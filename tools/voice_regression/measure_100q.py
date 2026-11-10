#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""100 题全链路耗时测量驱动。

逐题串行：发布 /question_asr → 等 /tts_session_finished（播报完成）→ 下一题。
同时订阅 /ali_llm_output（[START]/首delta/[DONE]）与 /latency_event（节点内部阶段），
全部事件带 wall 时间戳落 JSONL，供离线分段分析。

用法（栈以 LATENCY_PROBE=1 启动）：
  python3 measure_100q.py [--limit N]
输出：
  tools/voice_regression/out/measure_events.jsonl   原始事件流
  tools/voice_regression/out/measure_rows.json      每题行（t_pub/START/首delta/DONE/finished）
"""
import argparse
import json
import os
import time

import rclpy
from rclpy.node import Node
from std_msgs.msg import String

HERE = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = os.path.join(HERE, "out")
QFILE = os.path.join(HERE, "questions_100.json")


class Measure(Node):
    def __init__(self, ev_fp):
        super().__init__("measure_100q")
        self.ev_fp = ev_fp
        self.t_pub = None
        self.t_start = None
        self.t_first_delta = None
        self.t_done = None
        self.t_finished = None
        self._first_delta_seen = False
        self.pub_q = self.create_publisher(String, "/question_asr", 10)
        self.create_subscription(String, "/ali_llm_output", self.on_llm, 200)
        self.create_subscription(String, "/tts_session_finished", self.on_fin, 10)
        self.create_subscription(String, "/latency_event", self.on_lat, 500)

    def _ev(self, kind, data=None):
        self.ev_fp.write(json.dumps(
            {"t": time.time(), "kind": kind, "data": data},
            ensure_ascii=False) + "\n")
        self.ev_fp.flush()

    def on_llm(self, msg):
        d = msg.data
        now = time.time()
        if d == "[START]":
            if self.t_start is None:
                self.t_start = now
            self._ev("llm_start", d)
        elif d == "[DONE]":
            self.t_done = now
            self._ev("llm_done", d)
        else:
            if self.t_pub and not self._first_delta_seen:
                self._first_delta_seen = True
                self.t_first_delta = now
                self._ev("llm_first_delta", d[:30])

    def on_fin(self, msg):
        if self.t_pub is not None and self.t_finished is None:
            self.t_finished = time.time()
            self._ev("tts_finished", msg.data)

    def on_lat(self, msg):
        try:
            payload = json.loads(msg.data)
        except Exception:
            return
        payload["t_wall"] = time.time()
        self._ev("lat", payload)

    def reset(self):
        self.t_pub = self.t_start = self.t_first_delta = None
        self.t_done = self.t_finished = None
        self._first_delta_seen = False


def spin_until(node, cond, timeout):
    deadline = time.time() + timeout
    while time.time() < deadline and not cond():
        rclpy.spin_once(node, timeout_sec=0.1)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--limit", type=int, default=0)
    args = ap.parse_args()
    os.makedirs(OUT_DIR, exist_ok=True)
    questions = json.load(open(QFILE, encoding="utf-8"))
    if args.limit:
        questions = questions[:args.limit]

    rclpy.init()
    ev_fp = open(os.path.join(OUT_DIR, "measure_events.jsonl"), "w",
                 encoding="utf-8")
    node = Measure(ev_fp)
    # 等 DDS 匹配完成（否则首题可能在发现前发出而丢失）
    deadline = time.time() + 20
    while time.time() < deadline:
        rclpy.spin_once(node, timeout_sec=0.1)
        if node.pub_q.get_subscription_count() >= 1:
            break
    print(f"[measure] DDS 匹配完成: q_subs={node.pub_q.get_subscription_count()}", flush=True)
    rows = []
    t_all = time.time()
    for i, item in enumerate(questions, 1):
        node.reset()
        msg = String()
        msg.data = item["question"]
        node.t_pub = time.time()
        node.pub_q.publish(msg)
        spin_until(node, lambda: node.t_finished is not None, 60.0)
        row = {
            "id": item["id"],
            "question": item["question"],
            "ok": node.t_finished is not None,
            "t_pub": node.t_pub,
            "question_to_start_ms": _ms(node.t_start, node.t_pub),
            "start_to_first_delta_ms": _ms(node.t_first_delta, node.t_start),
            "question_to_first_delta_ms": _ms(node.t_first_delta, node.t_pub),
            "delta_to_done_ms": _ms(node.t_done, node.t_first_delta),
            "question_to_done_ms": _ms(node.t_done, node.t_pub),
            "question_to_finished_ms": _ms(node.t_finished, node.t_pub),
        }
        rows.append(row)
        print(f"[{i}/{len(questions)}] q2firstdelta={row['question_to_first_delta_ms']}ms "
              f"q2done={row['question_to_done_ms']}ms q2finish={row['question_to_finished_ms']}ms "
              f"{'OK' if row['ok'] else 'TIMEOUT'}", flush=True)
    json.dump(rows, open(os.path.join(OUT_DIR, "measure_rows.json"), "w",
                         encoding="utf-8"), ensure_ascii=False, indent=1)
    print(f"总耗时 {time.time()-t_all:.0f}s，完成 {sum(r['ok'] for r in rows)}/{len(rows)}")
    ev_fp.close()
    node.destroy_node()
    rclpy.shutdown()


def _ms(t1, t0):
    if t1 is None or t0 is None:
        return None
    return round((t1 - t0) * 1000)


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""语音链暴力测试驱动（比 ros2 topic pub CLI 快，毫秒级时序）。

测试项：
  flood     /ali_llm_output 洪水：多轮快速 [START]+10单元+[DONE]
  storm     [START]/[DONE] 会话风暴 ×10（空会话）
  longtext  超长文本（600字单单元 + 多句）
  abnormal  异常消息：空串、无[START]的[DONE]、[START]连发
  rapidq    真实问题快速连发 /question_asr ×3

用法（栈运行时）：
  python3 stress_test_voice.py <case>
"""
import sys
import time

import rclpy
from rclpy.node import Node
from std_msgs.msg import String


class Stress(Node):
    def __init__(self):
        super().__init__("voice_stress_driver")
        self.llm = self.create_publisher(String, "/ali_llm_output", 100)
        self.q = self.create_publisher(String, "/question_asr", 10)

    def p(self, topic, data, gap=0.0):
        msg = String()
        msg.data = data
        (self.llm if topic == "llm" else self.q).publish(msg)
        if gap:
            time.sleep(gap)

    # ---------------- 用例 ----------------
    def flood(self):
        units = ["这是洪水测试的第%d句话。" % i for i in range(10)]
        for r in range(3):
            self.p("llm", "[START]")
            for u in units:
                self.p("llm", u, 0.03)
            self.p("llm", "[DONE]")
            print(f"[flood] 第{r+1}轮已发（{len(units)}单元）", flush=True)
            time.sleep(0.8)
        time.sleep(8)

    def storm(self):
        for r in range(10):
            self.p("llm", "[START]")
            self.p("llm", "[DONE]")
            time.sleep(0.15)
        print("[storm] 10 轮空会话已发（间隔150ms）", flush=True)
        time.sleep(6)

    def longtext(self):
        self.p("llm", "[START]")
        long_unit = ("透析期间的饮水管理非常关键，" * 20 + "请务必遵守医嘱。")
        self.p("llm", long_unit)
        for i in range(5):
            self.p("llm", f"补充说明第{i+1}条，注意控制水分。", 0.02)
        self.p("llm", "[DONE]")
        print(f"[longtext] 已发 {len(long_unit)} 字单单元 + 5 短句", flush=True)
        time.sleep(14)

    def abnormal(self):
        self.p("llm", "")                       # 空串
        self.p("llm", "   ", 0.1)               # 空白
        self.p("llm", "[DONE]", 0.1)            # 无 START 的 DONE
        self.p("llm", "[START]", 0.1)
        self.p("llm", "[START]", 0.1)           # START 连发
        self.p("llm", "异常消息后的正常句子。", 0.1)
        self.p("llm", "[DONE]", 0.1)
        print("[abnormal] 异常消息组合已发", flush=True)
        time.sleep(8)

    def rapidq(self):
        qs = [
            "小医你好，内瘘震颤摸不到了怎么办？",
            "透析患者能喝多少水？",
            "第一次透析要准备什么？",
        ]
        for q in qs:
            self.p("q", q, 0.4)
        print("[rapidq] 3 个问题已连发（间隔400ms）", flush=True)
        time.sleep(25)


def main():
    case = sys.argv[1] if len(sys.argv) > 1 else "flood"
    rclpy.init()
    node = Stress()
    time.sleep(2.0)  # 等 DDS 发现完成（否则首批发出的消息可能未被订阅）
    getattr(node, case)()
    node.destroy_node()
    rclpy.shutdown()


if __name__ == "__main__":
    main()

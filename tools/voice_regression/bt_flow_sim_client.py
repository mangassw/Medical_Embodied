#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""仿真行为树客户端：驱动 BT 完整流程，只测 ASR→LLM→TTS 真实链路。

替代真机主行为树发送 LLMInteraction goal：
  goal(PASSIVE) → bt_hci 播问候 → start_asr → 真人说话 → ASR → 网关 → TTS
  → /tts_session_finished → dialog_manager → dialog_session_finished
  → action 返回 need_call_nurse/summary → 本客户端打印结果。

用法（需 source ROS 与 install，栈已运行）：
  PYTHONNOUSERSITE=1 python3 bt_flow_sim_client.py [--mode passive|alert|interrupt] [--person 1]
"""
import argparse
import sys

import rclpy
from rclpy.action import ActionClient
from rclpy.node import Node

ROOT = "/home/artrc/medical_ws/Medical_Embodied_dev/install"
sys.path.insert(0, f"{ROOT}/interfaces/lib/python3.12/site-packages")
sys.path.insert(0, f"{ROOT}/llm_node_comm/lib/python3.12/site-packages")

from interfaces.action import LLMInteraction  # noqa: E402
from interfaces.msg import ActionStatus  # noqa: E402

MODES = {"alert": 0, "passive": 1, "interrupt": 2}
STATUS_NAMES = {getattr(ActionStatus, n): n for n in dir(ActionStatus)
                if n.isupper() and isinstance(getattr(ActionStatus, n), int)}


class SimBTClient(Node):
    def __init__(self):
        super().__init__("bt_flow_sim_client")
        self._client = ActionClient(self, LLMInteraction, "llm_interaction")

    def run(self, mode: int, person_id: int) -> int:
        goal = LLMInteraction.Goal()
        goal.mode = mode
        goal.person_id = person_id
        goal.context = ""
        self.get_logger().info(f"发送 goal: mode={mode} person={person_id}，等待 BT 服务器...")
        self._client.wait_for_server()
        future = self._client.send_goal_async(goal, feedback_callback=self.on_feedback)
        rclpy.spin_until_future_complete(self, future)
        handle = future.result()
        if not handle.accepted:
            self.get_logger().error("goal 被 BT 拒绝")
            return 1
        self.get_logger().info("goal 已接受 → 问候音后请直接对麦克风说问题（无需唤醒词）")
        result_future = handle.get_result_async()
        rclpy.spin_until_future_complete(self, result_future)
        result = result_future.result().result
        status = STATUS_NAMES.get(result.status.status, str(result.status.status))
        print("\n===== 行为树流程结果 =====")
        print(f"  status        : {status}")
        print(f"  need_call_nurse: {result.need_call_nurse}")
        print(f"  summary       : {result.summary or '(空)'}")
        print("==========================")
        return 0

    def on_feedback(self, fb):
        self.get_logger().info(f"[feedback] {fb.feedback.partial}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mode", default="passive", choices=MODES.keys())
    ap.add_argument("--person", type=int, default=1)
    args = ap.parse_args()
    rclpy.init()
    node = SimBTClient()
    try:
        code = node.run(MODES[args.mode], args.person)
    finally:
        node.destroy_node()
        rclpy.shutdown()
    sys.exit(code)


if __name__ == "__main__":
    main()

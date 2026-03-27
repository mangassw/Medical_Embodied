#!/usr/bin/env python3
import argparse
import rospy
import actionlib

from interfaces.msg import NavigateAction, NavigateGoal


STATUS_NAMES = {
    0: "OK",
    1: "TIMEOUT",
    2: "PREEMPTED",
    3: "NO_PATH",
    4: "ABORTED",
}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--target", default="p0")
    parser.add_argument("--nav-type", default="goal")
    parser.add_argument("--timeout", type=float, default=5.0)
    args = parser.parse_args()

    rospy.init_node("navigate_mock_client")
    client = actionlib.SimpleActionClient("navigate", NavigateAction)
    if not client.wait_for_server(rospy.Duration(3.0)):
        rospy.logerr("navigate action server not available")
        return 1

    goal = NavigateGoal(target=args.target, nav_type=args.nav_type)
    client.send_goal(goal)
    client.wait_for_result(rospy.Duration(args.timeout))
    result = client.get_result()
    if result is None:
        rospy.logerr("no result")
        return 2

    status = result.status.status
    rospy.loginfo("navigate result=%s message=%s", STATUS_NAMES.get(status, str(status)), result.message)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

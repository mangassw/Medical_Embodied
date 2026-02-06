#!/usr/bin/env python3
import rospy
from interfaces.srv import Dock, DockResponse, ChargeUntil, ChargeUntilResponse


def handle_dock(req):
    rospy.loginfo("dock requested start=%s", str(req.start).lower())
    return DockResponse(ok=True)


def handle_charge(req):
    rospy.loginfo("charge_until requested soc_target=%.1f", req.soc_target)
    return ChargeUntilResponse(ok=True)


if __name__ == "__main__":
    rospy.init_node("charge_services")
    rospy.Service("dock", Dock, handle_dock)
    rospy.Service("charge_until", ChargeUntil, handle_charge)
    rospy.loginfo("charge_services started")
    rospy.spin()

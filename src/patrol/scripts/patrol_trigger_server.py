#!/usr/bin/env python3
import rospy
from std_msgs.msg import Bool
from interfaces.srv import PatrolTrigger, PatrolTriggerResponse


class PatrolTriggerServer:
    def __init__(self):
        self._state = False
        self._pub = rospy.Publisher("patrol_triggered", Bool, queue_size=10, latch=True)
        self._srv = rospy.Service("patrol_trigger", PatrolTrigger, self._handle)
        rospy.loginfo("patrol_trigger_server started")

    def _handle(self, req):
        self._state = bool(req.enable)
        self._pub.publish(Bool(data=self._state))
        return PatrolTriggerResponse(ok=True)

    def spin(self):
        rate = rospy.Rate(1)
        while not rospy.is_shutdown():
            self._pub.publish(Bool(data=self._state))
            rate.sleep()


if __name__ == "__main__":
    rospy.init_node("patrol_trigger_server")
    server = PatrolTriggerServer()
    server.spin()

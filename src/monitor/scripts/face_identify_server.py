#!/usr/bin/env python3
import rospy
from interfaces.srv import FaceIdentify, FaceIdentifyResponse


def handle_face_identify(_req):
    person_id = rospy.get_param("~person_id", 1)
    confidence = rospy.get_param("~confidence", 0.86)
    message = rospy.get_param("~message", "mock face identify")
    return FaceIdentifyResponse(
        success=True,
        person_id=person_id,
        confidence=confidence,
        message=message,
    )


if __name__ == "__main__":
    rospy.init_node("face_identify_server")
    rospy.Service("face_identify", FaceIdentify, handle_face_identify)
    rospy.loginfo("face_identify_server started")
    rospy.spin()

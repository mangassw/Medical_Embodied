#!/usr/bin/env python3
import rospy
from std_msgs.msg import Bool
from interfaces.msg import Battery, Fault


def main():
    rospy.init_node("monitor_mock_pub")
    battery_pub = rospy.Publisher("battery", Battery, queue_size=10)
    fault_pub = rospy.Publisher("fault", Fault, queue_size=10)
    call_signal_pub = rospy.Publisher("call_signal", Bool, queue_size=10)

    rate_hz = rospy.get_param("~rate_hz", 1.0)
    rate = rospy.Rate(rate_hz)

    while not rospy.is_shutdown():
        battery = Battery()
        battery.soc = float(rospy.get_param("~battery_soc", 50.0))
        battery.charging = bool(rospy.get_param("~battery_charging", False))
        battery.voltage = float(rospy.get_param("~battery_voltage", 24.0))
        battery_pub.publish(battery)

        fault = Fault()
        fault.fault_type = str(rospy.get_param("~fault_type", ""))
        fault.severity = int(rospy.get_param("~fault_severity", 0))
        fault.details = str(rospy.get_param("~fault_details", ""))
        fault_pub.publish(fault)

        call_signal = bool(rospy.get_param("~call_signal", False))
        call_signal_pub.publish(Bool(data=call_signal))

        rate.sleep()


if __name__ == "__main__":
    main()

#pragma once

#include <ros/ros.h>
#include <std_msgs/String.h>
#include <geometry_msgs/Twist.h>
#include <std_msgs/Float32MultiArray.h>
#include "can_msgs/Frame.h"
#include "controlcan.h"
#include "printf_utils.h"
#include "std_msgs/Int16MultiArray.h"
#include <sensor_msgs/Joy.h>
#include "wheel_motor_driver.h"

namespace car{
constexpr static const char* NODE_NAME = "car_canalyst";
class CarCanalyst
{
	public:

		// uint16_t wheel_left_v,wheel_right_v;
		// float stop_val,climb_val;
		CarCanalyst() = default;

		~CarCanalyst();

		void init();
		void init_can1();
		void init_can2();
		void receive_func();
		void dev_close();
		
	private:
		VCI_BOARD_INFO pInfo;//用来获取设备信息。
		VCI_BOARD_INFO pInfo1 [50];
		
		//接受其他话题发布的速度值
		void real_ctrl_cb(const interfaces::RealCMD::ConstPtr& real_cmd);

		ros::NodeHandle nh;
		ros::Subscriber real_ctrl_sub_;
		ros::Publisher can_msg_fb_pub_;
};
}



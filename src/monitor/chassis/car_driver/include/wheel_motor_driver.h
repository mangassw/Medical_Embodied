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
#include "interfaces/RealCMD.h"

namespace car {
class WheelMotorCanCtrl
{
	public:
		void init();
		void print_send_info(const VCI_CAN_OBJ* can_obj);
		uint8_t RPDO0_Config(uint8_t ID);
		uint8_t RPDO1_Config(uint8_t ID);
		uint8_t RPDO2_Config(uint8_t ID);
		uint8_t TPDO0_Config(uint8_t ID);
		uint8_t TPDO1_Config(uint8_t ID);
		uint8_t Profile_Velocity_Init(uint8_t ID);
		uint8_t NMT_Control(uint8_t Data0, uint8_t ID);
		uint8_t Driver_Enable(uint8_t ID);
		void Profile_Velocity_Test(uint8_t ID);
		void ZLAC8015D_Init_Velocity_Mode(void);
		bool Velocity_Joy_Control(uint8_t ID,uint16_t left_v,uint16_t right_v);
		void Clear_Error_Code(uint8_t ID);
		bool Quick_Stop(uint8_t ID);
		bool Re_Enabled(uint8_t ID);
		bool Driver_Disabled(uint8_t ID);
		void Set_max_current(uint8_t ID);
		void Enable_Error_PWM(uint8_t ID);
		void Set_Overload_param(uint8_t ID);
		void Query_Wheel_Param(uint8_t ID);
		void Set_Overload_Time(uint8_t ID);
	private:
		int count_num;
		ros::NodeHandle nh;
};

}



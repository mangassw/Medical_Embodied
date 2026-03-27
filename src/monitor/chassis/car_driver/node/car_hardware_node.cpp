#include "car_canalyst.h"
// #include "car_joy.h"
#include "wheel_motor_driver.h"
#include "std_msgs/Int32.h"



int main (int argc, char** argv) 
{
	// 1.初始化ros节点
	ros::init(argc, argv, "car_hardware_node");
	ros::NodeHandle nh;
	car::CarCanalyst canalyst_node_;
	car::WheelMotorCanCtrl motorctrl_node_;
	// car::hardware::JoyControl _joyctrl_node;

	// 2.初始化canalyst硬件端并延时
	canalyst_node_.init_can1();
	// canalyst_node_.init_can2();
	usleep(500000); // delay 500ms

	// 3.初始化电机并设置参数
	motorctrl_node_.ZLAC8015D_Init_Velocity_Mode();

	// 4.初始化遥控器接收
	// _joyctrl_node.init();

	// 5.初始化canalyst软件
	canalyst_node_.init();
	
	ros::Rate loop_rate(500);
	// _motorctrl_node.Driver_Disabled(1);	
	static bool flag;	
	ros::Time last_time_ = ros::Time::now();
	while(ros::ok())
	{
		canalyst_node_.receive_func();

		loop_rate.sleep();
		ros::spinOnce();
	}
	// 关闭can设备
	canalyst_node_.dev_close();
	return 0;
}
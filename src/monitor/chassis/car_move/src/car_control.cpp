#include "car_control.h"


namespace car{


void CarMove::init()
{
  ros::NodeHandle nh;
  nh.param<int>("Joy_Liner_K", Joy_Liner_K, 40);
  nh.param<int>("Joy_Angular_K", Joy_Angular_K, 17);

  real_cmd_pub_ = nh.advertise<interfaces::RealCMD>("/realcmd_ctrl",1);

  nav_vel_sub_ = nh.subscribe<geometry_msgs::Twist>("/cmd_vel",1,&CarMove::nav_vel_cb,this);
  joy_ctrl_sub_ = nh.subscribe<sensor_msgs::Joy>("/joy_node_bl/joy",10,&CarMove::joy_ctrl_cb,this);
  joy_usb_ctrl_sub_ = nh.subscribe<sensor_msgs::Joy>("/joy_node_usb/joy",10,&CarMove::joy_usb_ctrl_cb,this);
  
}
void CarMove::joy_usb_ctrl_cb(const sensor_msgs::Joy::ConstPtr& joy)
{
  	float left_rocker   = joy->axes[1];  // 0-axis[1]-->左摇杆上下移动-->控制轮毂电机前进后退
	float right_rocker  = joy->axes[3];  // 1-axis[2]-->右摇杆左右移动-->控制轮毂电机左右转
	float A_button = joy->buttons[0];       // 3-buttons[0]-->A键-->循迹以及一级爬升到位
	float B_button = joy->buttons[1];       // 4-buttons[1]-->B键-->一级爬升到位至二级爬升到位
	float X_button = joy->buttons[2];       // 5-buttons[3]-->X键-->任意时刻退出至原点
	float Y_button = joy->buttons[3];       // 6-buttons[4]-->Y键-->任意时刻暂停后恢复运动，且若已经二级爬升到位则一键撤离至原点
	float left_bumper = joy->axes[6];    // 7-axis[5]-->左上扳机键-->按住将控制后电机作为驱动轮，默认为前电机驱动
	float M_button1 = joy->buttons[7];      // 8-buttons[5]-->中间，从左往右第一个View按钮-->测试用
	float M_button3 = joy->buttons[8];      // 10-buttons[7]-->中间，从左往右第三个菜单按钮-->测试用
	float L_button = joy->buttons[4];      // 11-buttons[8]-->左上的按键-->测试用
	float R_button = joy->buttons[5];      // 12-buttons[9]-->右上的按键-->测试用
	float dir_down = joy->axes[11];      // 12-方向键下键 ---> 用于齿轮电机的归零
	float right_bumper = joy->axes[12];  // 13-axis[4]-->右上扳机键
	float left_roc_mid  = joy->buttons[9]; // 14-button[13]-->左摇杆中键
	float right_roc_mid = joy->buttons[10]; // 15-button[14]-->右摇杆中键
	// ROS_INFO_STREAM("joy_ctrl_cb: "<< left_rocker << " " << right_rocker << " " << A_button << " " << B_button << " " << X_button << " " << Y_button << " " << left_bumper << " " << M_button1 << " " << M_button3 << " " << L_button << " " << R_button << " " << dir_down << " " << right_bumper << " " << left_roc_mid << " " << right_roc_mid);	
	// 避免按键弹起带来影响
	static float last_button[4];
	if(last_button[0] != 0 || last_button[1] != 0 || last_button[2] != 0 || last_button[3] != 0) { 
		last_button[0] = 0;  last_button[1] = 0;  last_button[2] = 0;  last_button[3] = 0;
		return ; 
	}  
	last_button[0] = A_button*1000+B_button*100+X_button*10+Y_button;
	last_button[1] = M_button1*10+M_button3;
	// last_button[2] = L_button*10+R_button;
	last_button[2] = L_button*10;
	last_button[3] = left_roc_mid*10+right_roc_mid;
	// ----------OVER------------------

	// 轮毂电机断使能以及重新使能控制
	if(left_roc_mid)  {car_state = CarState::MotorDisable;    return;}
	if(right_roc_mid) {car_state = CarState::MotorReEnable;   return;}
	if(L_button) {car_state = CarState::MotorStop;   return;}
	if(car_state == CarState::MotorDisable || car_state == CarState::MotorStop) return;

	if(A_button) {car_state = CarState::AutoNavigating; return;}
	// 手动遥控
	uint16_t joy_wheel_left_v_quick,joy_wheel_right_v_quick,joy_wheel_left_v_normal,joy_wheel_right_v_normal;
	joy_wheel_left_v_normal  =   left_rocker*Joy_Liner_K - right_rocker*Joy_Angular_K;		// 正常手动遥控调试机器人速度
	joy_wheel_right_v_normal = -(left_rocker*Joy_Liner_K + right_rocker*Joy_Angular_K);
	// 遥控轮毂电机和齿轮电机
	if((left_rocker != 0||right_rocker != 0))
	{
		car_state = CarState::Manual;
		if(left_bumper != 0 && left_bumper != 1)
		{
			float speed_k = (-left_bumper + 1) * 2.5 + 1; // [1-6]
			// ROS_INFO_STREAM("speed_k: "<< speed_k);
			// joy_wheel_left_v_quick  =   left_rocker*Joy_Liner_K*5 - right_rocker*Joy_Angular_K*5;		// 快速遥控调试机器人速度
			// joy_wheel_right_v_quick = -(left_rocker*Joy_Liner_K*5 + right_rocker*Joy_Angular_K*5);
			manual_left_rpm_  =  left_rocker*Joy_Liner_K* speed_k - right_rocker*Joy_Angular_K*speed_k/1.5;
			manual_right_rpm_ = -(left_rocker*Joy_Liner_K*speed_k + right_rocker*Joy_Angular_K*speed_k/1.5);
		}else
		{
			manual_left_rpm_  = joy_wheel_left_v_normal;
			manual_right_rpm_ = joy_wheel_right_v_normal;
		}
	}
	else if(left_rocker == 0&&right_rocker == 0)
		car_state = CarState::Pause;
	// ROS_INFO("joy_ctrl_cb: left_rpm_ = %d, right_rpm_ = %d",manual_left_rpm_,manual_right_rpm_);
}
void CarMove::joy_ctrl_cb(const sensor_msgs::Joy::ConstPtr& joy)
{
  	float left_rocker   = joy->axes[1];  // 0-axis[1]-->左摇杆上下移动-->控制轮毂电机前进后退
	float right_rocker  = joy->axes[2];  // 1-axis[2]-->右摇杆左右移动-->控制轮毂电机左右转
	float A_button = joy->buttons[0];       // 3-buttons[0]-->A键-->循迹以及一级爬升到位
	float B_button = joy->buttons[1];       // 4-buttons[1]-->B键-->一级爬升到位至二级爬升到位
	float X_button = joy->buttons[3];       // 5-buttons[3]-->X键-->任意时刻退出至原点
	float Y_button = joy->buttons[4];       // 6-buttons[4]-->Y键-->任意时刻暂停后恢复运动，且若已经二级爬升到位则一键撤离至原点
	float left_bumper = joy->axes[6];    // 7-axis[5]-->左上扳机键-->按住将控制后电机作为驱动轮，默认为前电机驱动
	float M_button1 = joy->buttons[7];      // 8-buttons[5]-->中间，从左往右第一个View按钮-->测试用
	float M_button3 = joy->buttons[8];      // 10-buttons[7]-->中间，从左往右第三个菜单按钮-->测试用
	float L_button = joy->buttons[9];      // 11-buttons[8]-->左上的按键-->测试用
	float R_button = joy->buttons[10];      // 12-buttons[9]-->右上的按键-->测试用
	float dir_down = joy->axes[11];      // 12-方向键下键 ---> 用于齿轮电机的归零
	float right_bumper = joy->axes[12];  // 13-axis[4]-->右上扳机键
	float left_roc_mid  = joy->buttons[13]; // 14-button[13]-->左摇杆中键
	float right_roc_mid = joy->buttons[14]; // 15-button[14]-->右摇杆中键
	// ROS_INFO_STREAM("joy_ctrl_cb: "<< left_rocker << " " << right_rocker << " " << A_button << " " << B_button << " " << X_button << " " << Y_button << " " << left_bumper << " " << M_button1 << " " << M_button3 << " " << L_button << " " << R_button << " " << dir_down << " " << right_bumper << " " << left_roc_mid << " " << right_roc_mid);	
  // 避免按键弹起带来影响
	static float last_button[4];
	if(last_button[0] != 0 || last_button[1] != 0 || last_button[2] != 0 || last_button[3] != 0) { 
		last_button[0] = 0;  last_button[1] = 0;  last_button[2] = 0;  last_button[3] = 0;
		return ; 
	}  
	last_button[0] = A_button*1000+B_button*100+X_button*10+Y_button;
	last_button[1] = M_button1*10+M_button3;
	// last_button[2] = L_button*10+R_button;
	last_button[2] = L_button*10;
	last_button[3] = left_roc_mid*10+right_roc_mid;
  // ----------OVER------------------
  
  // 轮毂电机断使能以及重新使能控制
	if(left_roc_mid)  {car_state = CarState::MotorDisable;    return;}
	if(right_roc_mid) {car_state = CarState::MotorReEnable;   return;}
	if(L_button) {car_state = CarState::MotorStop;   return;}
  	if(car_state == CarState::MotorDisable || car_state == CarState::MotorStop) return;

	if(A_button) {car_state = CarState::AutoNavigating; return;}
  // 手动遥控
  uint16_t joy_wheel_left_v_quick,joy_wheel_right_v_quick,joy_wheel_left_v_normal,joy_wheel_right_v_normal;
  joy_wheel_left_v_normal  =   left_rocker*Joy_Liner_K - right_rocker*Joy_Angular_K;		// 正常手动遥控调试机器人速度
	joy_wheel_right_v_normal = -(left_rocker*Joy_Liner_K + right_rocker*Joy_Angular_K);
  // 遥控轮毂电机和齿轮电机
	if((left_rocker != 0||right_rocker != 0))
	{
		car_state = CarState::Manual;
		if(left_bumper != 0 && left_bumper != 1)
		{
			float speed_k = (-left_bumper + 1) * 2.5 + 1; // [1-6]
			// ROS_INFO_STREAM("speed_k: "<< speed_k);
			// joy_wheel_left_v_quick  =   left_rocker*Joy_Liner_K*5 - right_rocker*Joy_Angular_K*5;		// 快速遥控调试机器人速度
			// joy_wheel_right_v_quick = -(left_rocker*Joy_Liner_K*5 + right_rocker*Joy_Angular_K*5);
			manual_left_rpm_  =  left_rocker*Joy_Liner_K* speed_k - right_rocker*Joy_Angular_K*speed_k/1.5;
			manual_right_rpm_ = -(left_rocker*Joy_Liner_K*speed_k + right_rocker*Joy_Angular_K*speed_k/1.5);
		}else
		{
			manual_left_rpm_  = joy_wheel_left_v_normal;
			manual_right_rpm_ = joy_wheel_right_v_normal;
		}
	}
	else if(left_rocker == 0&&right_rocker == 0)
		car_state = CarState::Pause;
	// ROS_INFO("joy_ctrl_cb: left_rpm_ = %d, right_rpm_ = %d",manual_left_rpm_,manual_right_rpm_);
}

void CarMove::nav_vel_cb(const geometry_msgs::Twist::ConstPtr& vel_msg)
{
	float wheel_base_ = 0.475; // 55.5cm
  float wheel_radius_ = 0.2032; // 8英寸转换为米为0.2032米
	float linear_vel = vel_msg->linear.x;
	float angular_vel = vel_msg->angular.z;
	nav_to_left_rpm_  = (linear_vel - angular_vel*wheel_base_/2) / wheel_radius_ * 60 / M_PI;
	nav_to_right_rpm_ = (linear_vel + angular_vel*wheel_base_/2) / wheel_radius_ * 60 / M_PI;
	// nav_to_right_rpm_= (linear_vel - angular_vel*wheel_base_/2) / wheel_radius_ * 60 / M_PI;
	// nav_to_left_rpm_= (linear_vel + angular_vel*wheel_base_/2) / wheel_radius_ * 60 / M_PI;
	nav_to_right_rpm_ = -nav_to_right_rpm_;
}


void CarMove::wheel_speed_choose(CarState car_state_,uint16_t &left_rpm_,uint16_t &right_rpm_)
{
	if(car_state_ == CarState::Init || car_state_ == CarState::Pause 
  || car_state_ == CarState::Origin)
	{ 
    left_rpm_  = right_rpm_ = 0;
	}
	else if(car_state_ == CarState::AutoNavigating)
  {
    left_rpm_  = nav_to_left_rpm_;
    right_rpm_ = nav_to_right_rpm_;
  }
  else if(car_state_ == CarState::Manual)
  {
    left_rpm_  = manual_left_rpm_;
    right_rpm_ = manual_right_rpm_;
  }else
  {
    left_rpm_  = right_rpm_ = 0;
  }

}


void CarMove::run()
{
	car_state = CarState::Init;
	
	ros::Rate r_100hz(loop_hz);

	static ros::Time last_time_;

  interfaces::RealCMD real_cmd_;
  uint16_t left_rpm_ = 0, right_rpm_ = 0;

	while(ros::ok())
	{
		// 速度选择
		wheel_speed_choose(car_state,left_rpm_,right_rpm_);
		switch (car_state)
		{
			case CarState::Origin:
				real_cmd_.stop_flag = 0;
				real_cmd_.re_enabled = 0;
				real_cmd_.disabled = 0;
				break;
			case CarState::Init:
			case CarState::Pause:
			
			case CarState::AutoNavigating:
			case CarState::Manual:
				real_cmd_.wheel_left_v  = left_rpm_;
				real_cmd_.wheel_right_v = right_rpm_;
				break;
			case CarState::MotorDisable:
				real_cmd_.disabled = 1;
				break;
			case CarState::MotorReEnable:
				real_cmd_.stop_flag = 0;
				real_cmd_.re_enabled = 1;
				real_cmd_.disabled = 0;
				car_state = CarState::Origin;
				break;
			case CarState::MotorStop:
				real_cmd_.stop_flag = 1;
				real_cmd_.disabled = 0;
				break;
			default:
				break;
		}
    
    real_cmd_pub_.publish(real_cmd_);
    r_100hz.sleep();
    ros::spinOnce();
	}
}

}





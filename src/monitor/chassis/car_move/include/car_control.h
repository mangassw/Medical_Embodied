#pragma once 

#include <ros/ros.h>
#include <std_msgs/Float32MultiArray.h>
#include <std_msgs/Float32.h>
#include <std_msgs/String.h>
#include <geometry_msgs/Twist.h>
#include <nav_msgs/Odometry.h>
#include <std_msgs/Int32MultiArray.h>
#include <std_msgs/Int32.h>
#include <geometry_msgs/PoseWithCovarianceStamped.h>
#include <sensor_msgs/Imu.h>
#include "geometry_msgs/PointStamped.h"

#include "printf_utils.h"
#include "can_msgs/Frame.h"
#include "interfaces/RealCMD.h"


#include "sensor_msgs/Joy.h"

namespace car {


enum class CarState : uint8_t {
  Init = 0,          // 初始化
  Origin,            // 遥控后或任务完成后的默认状态，原点
  Manual,            // 手动控制状态
  AutoNavigating,    // 自动导航状态
  Pause,             // 暂停
  MotorDisable,     // 轮毂电机失能
  MotorReEnable,    // 轮毂电机重新使能
  MotorStop,         // 轮毂电机停止

};



class CarMove{
  public:
    CarMove() = default;
    // ~CarMove();
    void init();
    void run();
  
  private:

    #define loop_hz 100

    void nav_vel_cb(const geometry_msgs::Twist::ConstPtr& vel_msg);
    void wheel_speed_choose(CarState car_state_,uint16_t &left_rpm_,uint16_t &right_rpm_);
    void joy_ctrl_cb(const sensor_msgs::Joy::ConstPtr& joy);
    void joy_usb_ctrl_cb(const sensor_msgs::Joy::ConstPtr& joy);


    ros::Publisher real_cmd_pub_ ;
    ros::Subscriber nav_vel_sub_,joy_ctrl_sub_,joy_usb_ctrl_sub_;
  
    CarState car_state, last_car_state;
    

    int32_t Joy_Liner_K,Joy_Angular_K;
    uint16_t nav_to_left_rpm_ = 0, nav_to_right_rpm_ = 0;
    uint16_t manual_left_rpm_ = 0, manual_right_rpm_ = 0;
    
};









}
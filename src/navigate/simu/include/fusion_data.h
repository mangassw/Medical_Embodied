#pragma once

#include <actionlib/client/simple_action_client.h>
#include <geometry_msgs/Twist.h>
#include <nav_msgs/Path.h>
#include <ros/ros.h>
#include <tf2/utils.h>
#include <tf2_ros/transform_listener.h>
#include <tf2_ros/transform_broadcaster.h>
#include <geometry_msgs/TransformStamped.h>
#include <nav_msgs/Odometry.h>
#include <vector>
#include <std_msgs/Int32.h>
#include "sensor_msgs/Joy.h"

#include "costmap_2d/costmap_2d.h"
#include "costmap_2d/costmap_2d_ros.h"
#include "mbf_msgs/MoveBaseAction.h"
#include "mbf_msgs/ExePathAction.h"
#include "xjrobot_simu/fusion_analysis.h"
#include <iostream>
#include <fstream>
#include <ctime>
#include <iomanip>
#include <thread>
#include <mutex>
#include "gazebo_msgs/ModelStates.h"


namespace fusion_data {
  constexpr static const char* NODE_NAME = "xjrobot_bridge";
  constexpr static const char* CommandTopic = "/cmd_vel";
  constexpr static const char* VelFeedbackTopic = "/odom_diff";
  constexpr static const char* FeedbackTopic = "/odom";
  constexpr static const char* FusionTopic = "/fusion_analysis";
  constexpr static const char* OdomTransform = "/odom_transform";
  constexpr static const char* OdomCalculation = "/odom_calc";
  constexpr static const double WheelSeparation = 0.56;
  constexpr static const double WheelRadius = 0.20;
  constexpr static const double TimerDuration = 0.2;
  constexpr static const double OdomTimerDur = 0.02;
  
class bridge {
  public:
    bridge();
  
    ~bridge();
  
    void init();
  
  private:
    void control_callback(const geometry_msgs::Twist::ConstPtr& msg);
  
    // void feedback_callback(const nav_msgs::Odometry::ConstPtr& msg);
  
    void fusionAnalysisTimerCB(const ros::TimerEvent& e);
    
    void odomTimerCB(const ros::TimerEvent& e);

    void velCB(const nav_msgs::Odometry::ConstPtr& msg);

  
  private:
    ros::Publisher fusion_analysis_pub_;
    ros::Publisher odom_transform_pub_;
    ros::Publisher odom_calc_pub_;
  
    ros::Subscriber control_sub_;
    ros::Subscriber feedback_sub_;
    ros::Subscriber vel_sub_;
  
    ros::Timer fusion_analysis_timer_;
    ros::Timer odom_calc_timer_;


    xjrobot_simu::fusion_analysis pub_msg_;

    ros::Time last_time_;
    double delta_time_ = 0;
    double linear_speed = 0,angular_speed = 0, accumulation_x_ = 0, accumulation_y_ = 0, accumulation_th_ = 0;

    tf2_ros::TransformBroadcaster br_;
    geometry_msgs::TransformStamped transformStamped_;
    nav_msgs::Odometry odom_;
    bool send_tf_flag_ = false;
  
    std::string odom_frame_ = "odom";
    std::string base_frame_ = "base_link";

    
  };
}


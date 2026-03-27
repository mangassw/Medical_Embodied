#include "fusion_data.h"

namespace fusion_data {

  bridge::bridge()
  {

  }

  bridge::~bridge() {
    fusion_analysis_timer_.stop();
  }

void bridge::init() {
  ros::NodeHandle nh;
  fusion_analysis_pub_ = nh.advertise<xjrobot_simu::fusion_analysis>(FusionTopic, 10);
  odom_transform_pub_ = nh.advertise<geometry_msgs::TransformStamped>(OdomTransform, 10);
  odom_calc_pub_ = nh.advertise<geometry_msgs::TransformStamped>(OdomCalculation, 10);

  vel_sub_ = nh.subscribe(VelFeedbackTopic, 1, &bridge::velCB, this);
  // feedback_sub_ = nh.subscribe(FeedbackTopic, 1, &bridge::feedback_callback, this, ros::TransportHints().tcpNoDelay());
  fusion_analysis_timer_ = nh.createTimer(ros::Duration(TimerDuration), &bridge::fusionAnalysisTimerCB, this);
  odom_calc_timer_ = nh.createTimer(ros::Duration(OdomTimerDur), &bridge::odomTimerCB, this);
  
  last_time_ = ros::Time::now();
  ros::param::get("send_tf_flag_", send_tf_flag_);

}

void bridge::velCB(const nav_msgs::Odometry::ConstPtr& msg) {
  
  pub_msg_.linear_feedback = msg->twist.twist.linear.x;
  pub_msg_.angular_feedback = msg->twist.twist.angular.z; 
  pub_msg_.lwheel_feedback = (pub_msg_.linear_feedback - 0.5 * pub_msg_.angular_feedback * WheelSeparation) / WheelRadius;
  pub_msg_.rwheel_feedback = (pub_msg_.linear_feedback + 0.5 * pub_msg_.angular_feedback * WheelSeparation) / WheelRadius;
}

void bridge::fusionAnalysisTimerCB(const ros::TimerEvent& e) {
  fusion_analysis_pub_.publish(pub_msg_);
}



void bridge::odomTimerCB(const ros::TimerEvent& e) {
  ros::Time now_ = ros::Time::now();
  delta_time_ = (now_ - last_time_).toSec();
  float control_rate_ = 20;
  // if (delta_time_ >= (0.5 / control_rate_)) {
    // linear_speed  = (right_speed + left_speed) / 2.0;   // 计算线速度
    // angular_speed = (right_speed - left_speed) / wheel_distance_;   // 计算角速度
    linear_speed  = pub_msg_.linear_feedback ;   // 计算线速度
    angular_speed = pub_msg_.angular_feedback;   // 计算角速度
    double delta_theta = (delta_time_ * angular_speed);
    double v_theta = delta_theta / delta_time_;
    
    double delta_dis = delta_time_ * linear_speed;
    double v_dis = delta_dis / delta_time_;
    double delta_x, delta_y;
    if (delta_theta == 0) {
      delta_x = delta_dis;
      delta_y = 0.0;
    } else {
      // delta_x = delta_dis * (sin(delta_theta) / delta_theta);
      // delta_y = delta_dis * ((1 - cos(delta_theta)) / delta_theta);
      delta_x = delta_dis * cos(delta_theta);
      delta_y = delta_dis * sin(delta_theta);
    }
    // accumulation_x_ += (cos(accumulation_th_) * delta_x - sin(accumulation_th_) * delta_y);
    // accumulation_y_ += (sin(accumulation_th_) * delta_x + cos(accumulation_th_) * delta_y);
    accumulation_x_ += (cos(accumulation_th_) * delta_x - sin(accumulation_th_) * delta_y);
    accumulation_y_ += (sin(accumulation_th_) * delta_x + cos(accumulation_th_) * delta_y);
    accumulation_th_ += delta_theta;
    if (accumulation_th_ <= -M_PI)    accumulation_th_ += 2 * M_PI;
    else if(accumulation_th_ >= M_PI)  accumulation_th_ -= 2 * M_PI;

    // std::cout <<"Calc Odom:";
    // std::cout <<"  [L_V]: " <<std::fixed <<std::setprecision(2)<< left_speed*100 ;
    // std::cout <<"  [L_RPM]: " <<std::fixed <<std::setprecision(2)<< left_rpm_fd*100 ;
    // std::cout <<"  [R_V]: " <<std::fixed <<std::setprecision(2)<< right_speed*100 ;
    // std::cout <<"  [R_RPM]: " <<std::fixed <<std::setprecision(2)<< right_rpm_fd*100 << std::endl ;
    geometry_msgs::Twist cmd_vel_fb;
    cmd_vel_fb.angular.z = angular_speed;
    cmd_vel_fb.linear.x = linear_speed;
    // real_vel_feedback_.publish(cmd_vel_fb);
      
    tf2::Quaternion q;
    q.setRPY(0, 0, accumulation_th_);

    if (send_tf_flag_){
      transformStamped_.header.stamp = ros::Time::now();
      transformStamped_.header.frame_id = odom_frame_;
      transformStamped_.child_frame_id = base_frame_;
      transformStamped_.transform.translation.x = accumulation_x_;
      transformStamped_.transform.translation.y = accumulation_y_;
      transformStamped_.transform.translation.z = 0.0;

      transformStamped_.transform.rotation.x = q.x();
      transformStamped_.transform.rotation.y = q.y();
      transformStamped_.transform.rotation.z = q.z();
      transformStamped_.transform.rotation.w = q.w();

      br_.sendTransform(transformStamped_);
    }
    
    nav_msgs::Odometry odom_;
    odom_.header.frame_id = odom_frame_;
    odom_.child_frame_id = base_frame_;
    odom_.header.stamp = now_;
    odom_.pose.pose.position.x = accumulation_x_;
    odom_.pose.pose.position.y = accumulation_y_;
    odom_.pose.pose.position.z = 0;
    odom_.pose.pose.orientation.x = q.getX();
    odom_.pose.pose.orientation.y = q.getY();
    odom_.pose.pose.orientation.z = q.getZ();
    odom_.pose.pose.orientation.w = q.getW();
    odom_.twist.twist.linear.x = v_dis;
    odom_.twist.twist.linear.y = 0;
    odom_.twist.twist.angular.z = v_theta;

    odom_calc_pub_.publish(odom_);

    last_time_ = now_;
  // }
}



}











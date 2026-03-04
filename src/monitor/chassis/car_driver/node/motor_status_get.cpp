#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include <geometry_msgs/TransformStamped.h>
#include <geometry_msgs/Twist.h>
#include <nav_msgs/Odometry.h>
#include <ros/ros.h>
#include <sensor_msgs/Imu.h>
#include <std_msgs/Float64.h>
#include <std_msgs/String.h>
#include <tf2/LinearMath/Quaternion.h>
#include <tf2_ros/transform_broadcaster.h>

#include "can_msgs/Frame.h"

class CalcOdom {
 public:
  CalcOdom();
  void init();
  void run();

 private:
  void recvVel(const can_msgs::Frame::ConstPtr& msg);
  void updateBotOdom();
  double convertRpmToMs(float inch, float rpm);
  static int16_t parseInt16LE(uint8_t low, uint8_t high);
  static uint16_t parseUInt16LE(uint8_t low, uint8_t high);
  std::string wheelFaultToString(uint16_t fault_code,
                                 const std::string& wheel_name) const;
  std::string composeFaultStatus(uint16_t left_fault, uint16_t right_fault) const;
  void navStartCb(const std_msgs::String::ConstPtr& nav_start_msg);
  void navOffsetYawCb(const std_msgs::Float64::ConstPtr& offset_yaw_msg);
  void odomCallback(const nav_msgs::Odometry::ConstPtr& msg);

  ros::Subscriber sub_wheel_vel_;
  ros::Subscriber nav_start_sub_;
  ros::Subscriber nav_offset_yaw_sub_;
  ros::Subscriber odom_sub_;
  ros::Publisher combined_odom_pub_;
  ros::Publisher wheel_fault_status_pub_;

  bool start_flag_;
  float left_rpm_fd_ = 0.0F;
  float right_rpm_fd_ = 0.0F;
  double wheel_distance_ = 0.562;
  double delta_time_ = 0.0;
  double accumulation_x_ = 0.0;
  double accumulation_y_ = 0.0;
  double accumulation_th_ = 0.0;
  int cur_left_;
  int cur_right_;
  int rev_left_;
  int rev_right_;
  int delta_left_;
  int delta_right_;
  int control_rate_ = 50;

  tf2_ros::TransformBroadcaster br_;
  geometry_msgs::TransformStamped transform_stamped_;
  ros::Time last_time_;
  ros::Time now_;
  nav_msgs::Odometry odom_;
  nav_msgs::Odometry received_odom_;

  double offset_yaw_ = 0.0;

  std::string odom_frame_ = "odom";
  std::string base_frame_ = "base_link";
};

CalcOdom::CalcOdom() : start_flag_(true) {}

void CalcOdom::init() {
  ros::NodeHandle nh;
  sub_wheel_vel_ =
      nh.subscribe<can_msgs::Frame>("/can_msg_fb", 10, &CalcOdom::recvVel, this);
  nav_start_sub_ =
      nh.subscribe<std_msgs::String>("workstatus", 1, &CalcOdom::navStartCb, this);
  nav_offset_yaw_sub_ = nh.subscribe<std_msgs::Float64>(
      "/offset_yaw", 10, &CalcOdom::navOffsetYawCb, this);
  odom_sub_ =
      nh.subscribe<nav_msgs::Odometry>("odom", 10, &CalcOdom::odomCallback, this);
  combined_odom_pub_ = nh.advertise<nav_msgs::Odometry>("combined_odom", 10);
  wheel_fault_status_pub_ =
      nh.advertise<std_msgs::String>("wheel_fault_status", 10);

  ros::param::set("/rosconsole/default_level", "debug");
}

void CalcOdom::odomCallback(const nav_msgs::Odometry::ConstPtr& msg) {
  received_odom_ = *msg;
}

void CalcOdom::navOffsetYawCb(
    const std_msgs::Float64::ConstPtr& offset_yaw_msg) {
  offset_yaw_ = offset_yaw_msg->data;
}

void CalcOdom::navStartCb(const std_msgs::String::ConstPtr& nav_start_msg) {
  (void)nav_start_msg;
}

void CalcOdom::recvVel(const can_msgs::Frame::ConstPtr& can_msg) {
  if ((can_msg->id & 0x180) != 0x180) {
    return;
  }
  if (can_msg->dlc < 8) {
    ROS_WARN_STREAM_THROTTLE(1.0, "[WheelFault] invalid DLC: " << static_cast<int>(can_msg->dlc));
    return;
  }

  const int16_t left_raw_rpm = parseInt16LE(can_msg->data[0], can_msg->data[1]);
  const int16_t right_raw_rpm = parseInt16LE(can_msg->data[2], can_msg->data[3]);
  left_rpm_fd_ = static_cast<float>(left_raw_rpm) / 10.0F;
  right_rpm_fd_ = static_cast<float>(right_raw_rpm) / 10.0F;

  // 故障码按小端格式组合：低字节在前，高字节在后。
  const uint16_t left_fault_code = parseUInt16LE(can_msg->data[4], can_msg->data[5]);
  const uint16_t right_fault_code = parseUInt16LE(can_msg->data[6], can_msg->data[7]);
  if (left_fault_code == 0x0000 && right_fault_code == 0x0000) {
    return;
  }

  const std::string status = composeFaultStatus(left_fault_code, right_fault_code);
  std_msgs::String status_msg;
  status_msg.data = status;
  wheel_fault_status_pub_.publish(status_msg);
  ROS_WARN_STREAM_THROTTLE(1.0, "[WheelFault] " << status);
}

int16_t CalcOdom::parseInt16LE(uint8_t low, uint8_t high) {
  const uint16_t raw = (static_cast<uint16_t>(high) << 8) | low;
  return static_cast<int16_t>(raw);
}

uint16_t CalcOdom::parseUInt16LE(uint8_t low, uint8_t high) {
  return (static_cast<uint16_t>(high) << 8) | low;
}

std::string CalcOdom::wheelFaultToString(uint16_t fault_code,
                                         const std::string& wheel_name) const {
  if (fault_code == 0x0000) {
    return wheel_name + ":none";
  }

  const std::vector<std::pair<uint16_t, std::string>> wheel_fault_bits = {
      {0x0004, "overcurrent"},  // 电机过流
      {0x0008, "overload"},     // 电机过载
      {0x0020, "deviation"},    // 电机编码器超差
      {0x0080, "reference"},    // 电机参考电压异常
      {0x0200, "hall"},         // 电机霍尔故障
      {0x0400, "overheat"},     // 电机超温
      {0x0800, "encoder"},      // 电机编码器错误
      {0x2000, "setpoint"}};    // 电机速度给定错误
  const std::vector<std::pair<uint16_t, std::string>> shared_fault_bits = {
      {0x0001, "overvoltage"},   // 过压
      {0x0002, "undervoltage"},  // 欠压
      {0x0100, "eeprom"}};       // EEPROM读写错误

  std::vector<std::string> faults;
  faults.reserve(wheel_fault_bits.size() + shared_fault_bits.size() + 1);

  for (const auto& entry : wheel_fault_bits) {
    if ((fault_code & entry.first) != 0) {
      faults.push_back(entry.second);
    }
  }
  for (const auto& entry : shared_fault_bits) {
    if ((fault_code & entry.first) != 0) {
      faults.push_back(entry.second);
    }
  }

  uint16_t known_mask = 0;
  for (const auto& entry : wheel_fault_bits) {
    known_mask |= entry.first;
  }
  for (const auto& entry : shared_fault_bits) {
    known_mask |= entry.first;
  }
  const uint16_t unknown_bits = fault_code & static_cast<uint16_t>(~known_mask);
  if (unknown_bits != 0) {
    std::ostringstream oss;
    oss << "unknown(0x" << std::hex << std::uppercase << unknown_bits << ")";
    faults.push_back(oss.str());
  }

  std::ostringstream result;
  result << wheel_name << ":";
  for (size_t i = 0; i < faults.size(); ++i) {
    if (i != 0) {
      result << "|";
    }
    result << faults[i];
  }
  return result.str();
}

std::string CalcOdom::composeFaultStatus(uint16_t left_fault,
                                         uint16_t right_fault) const {
  return wheelFaultToString(left_fault, "left") + "; " +
         wheelFaultToString(right_fault, "right");
}

// 如果是前进的话，左右两轮应均为正速度
void CalcOdom::updateBotOdom() {
  double left_speed = convertRpmToMs(8.0, left_rpm_fd_);
  double right_speed = convertRpmToMs(8.0, right_rpm_fd_);
  right_speed = -right_speed;
  now_ = ros::Time::now();

  if (start_flag_) {
    accumulation_x_ = accumulation_y_ = accumulation_th_ = 0.0;
    last_time_ = now_;
    start_flag_ = false;
    return;
  }

  delta_time_ = (now_ - last_time_).toSec();
  if (delta_time_ < (0.5 / control_rate_)) {
    return;
  }

  const double linear_speed = (right_speed + left_speed) / 2.0;  // 计算线速度
  const double angular_speed =
      (right_speed - left_speed) / wheel_distance_;  // 计算角速度
  const double delta_theta = delta_time_ * angular_speed;
  const double v_theta = delta_theta / delta_time_;

  const double delta_dis = delta_time_ * linear_speed;
  const double v_dis = delta_dis / delta_time_;
  double delta_x = 0.0;
  double delta_y = 0.0;
  if (delta_theta == 0) {
    delta_x = delta_dis;
  } else {
    delta_x = delta_dis * std::cos(delta_theta);
    delta_y = delta_dis * std::sin(delta_theta);
  }

  accumulation_x_ +=
      (std::cos(accumulation_th_) * delta_x - std::sin(accumulation_th_) * delta_y);
  accumulation_y_ +=
      (std::sin(accumulation_th_) * delta_x + std::cos(accumulation_th_) * delta_y);
  accumulation_th_ += delta_theta;
  if (accumulation_th_ <= -M_PI) {
    accumulation_th_ += 2 * M_PI;
  } else if (accumulation_th_ >= M_PI) {
    accumulation_th_ -= 2 * M_PI;
  }

  geometry_msgs::Twist cmd_vel_fb;
  cmd_vel_fb.angular.z = angular_speed;
  cmd_vel_fb.linear.x = linear_speed;

  tf2::Quaternion q;
  q.setRPY(0, 0, accumulation_th_);

  transform_stamped_.header.stamp = ros::Time::now();
  transform_stamped_.header.frame_id = odom_frame_;
  transform_stamped_.child_frame_id = base_frame_;
  transform_stamped_.transform.translation.x = accumulation_x_;
  transform_stamped_.transform.translation.y = accumulation_y_;
  transform_stamped_.transform.translation.z = 0.0;
  transform_stamped_.transform.rotation.x = q.x();
  transform_stamped_.transform.rotation.y = q.y();
  transform_stamped_.transform.rotation.z = q.z();
  transform_stamped_.transform.rotation.w = q.w();

  nav_msgs::Odometry combined_odom;
  combined_odom.header.stamp = ros::Time::now();
  combined_odom.header.frame_id = received_odom_.header.frame_id;
  combined_odom.pose = received_odom_.pose;
  combined_odom.twist.twist.linear.x = v_dis;
  combined_odom.twist.twist.linear.y = 0.0;
  combined_odom.twist.twist.angular.z = v_theta;
  combined_odom.child_frame_id = received_odom_.child_frame_id;
  combined_odom_pub_.publish(combined_odom);

  // 1. 使用里程计的yaw信息模拟imu的yaw
  // 2. 加上融合后的补偿值
  // 3. 即为真实坐标系下的yaw值，用于数字孪生
  tf2::Quaternion q_imu;
  double real_yaw = accumulation_th_ + offset_yaw_;
  if (real_yaw <= -M_PI) {
    real_yaw += 2 * M_PI;
  } else if (real_yaw >= M_PI) {
    real_yaw -= 2 * M_PI;
  }

  q_imu.setRPY(0, 0, real_yaw);
  sensor_msgs::Imu imu_msg;
  imu_msg.orientation.x = q_imu.getX();
  imu_msg.orientation.y = q_imu.getY();
  imu_msg.orientation.z = q_imu.getZ();
  imu_msg.orientation.w = q_imu.getW();

  last_time_ = now_;
  ROS_INFO_STREAM_THROTTLE(
      0.5, "[x]:" << std::fixed << std::setprecision(2) << accumulation_x_ * 100
                  << " [y]:" << std::fixed << std::setprecision(2)
                  << accumulation_y_ * 100 << " [th]:" << std::fixed
                  << std::setprecision(2) << accumulation_th_ * 57.3
                  << " [r_th]:" << std::fixed << std::setprecision(2)
                  << real_yaw * 57.3);
}

double CalcOdom::convertRpmToMs(float inch, float rpm) {
  const double circumference_inch = inch * 3.14159;               // 计算轮胎的周长，单位为英寸
  const double circumference_meter = circumference_inch * 0.0254;  // 将周长转换为米
  const double meters_per_minute = circumference_meter * rpm;      // 计算每分钟走过的米数
  double meters_per_second = meters_per_minute / 60.0;             // 将每分钟转换为每秒钟
  meters_per_second = meters_per_second * 100.0 / 105.0;           // 实际补偿值
  return meters_per_second;
}

void CalcOdom::run() {
  updateBotOdom();
}

int main(int argc, char** argv) {
  ros::init(argc, argv, "calc_odom");
  CalcOdom calc_odom;
  calc_odom.init();
  ros::Rate loop_rate(100);
  while (ros::ok()) {
    calc_odom.run();
    ros::spinOnce();
    loop_rate.sleep();
  }

  return 0;
}

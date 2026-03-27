#include <actionlib_msgs/GoalStatusArray.h>
#include <geometry_msgs/PoseStamped.h>
#include <nav_msgs/Odometry.h>
#include <ros/ros.h>
#include <tf2/utils.h>
#include <tf2_ros/buffer.h>
#include <tf2_ros/transform_listener.h>

#include <cmath>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace {
double normalizeAngle(double a) {
  constexpr double kPi = 3.14159265358979323846;
  while (a > kPi) a -= 2.0 * kPi;
  while (a < -kPi) a += 2.0 * kPi;
  return a;
}

std::string joinReasons(const std::vector<std::string>& reasons) {
  if (reasons.empty()) {
    return "none";
  }
  std::ostringstream oss;
  for (size_t i = 0; i < reasons.size(); ++i) {
    if (i != 0) oss << ",";
    oss << reasons[i];
  }
  return oss.str();
}
}  // namespace

class GoalReachDebug {
 public:
  GoalReachDebug() : nh_(), pnh_("~"), tf_listener_(tf_buffer_) {
    goal_topic_ = pnh_.param<std::string>("goal_topic", "/move_base_flex/controller_goal");
    odom_topic_ = pnh_.param<std::string>("odom_topic", "/odom_diff");
    status_topic_ = pnh_.param<std::string>("status_topic", "/move_base_flex/move_base/status");
    global_frame_ = pnh_.param<std::string>("global_frame", "map");
    odom_frame_ = pnh_.param<std::string>("odom_frame", "odom");
    base_frame_ = pnh_.param<std::string>("base_frame", "base_link");

    xy_goal_tolerance_ = pnh_.param("xy_goal_tolerance", 0.30);
    yaw_goal_tolerance_ = pnh_.param("yaw_goal_tolerance", 0.25);
    trans_stopped_vel_ = pnh_.param("trans_stopped_vel", 0.05);
    theta_stopped_vel_ = pnh_.param("theta_stopped_vel", 0.10);
    log_hz_ = std::max(1.0, pnh_.param("log_hz", 5.0));

    goal_sub_ = nh_.subscribe(goal_topic_, 5, &GoalReachDebug::goalCb, this);
    odom_sub_ = nh_.subscribe(odom_topic_, 20, &GoalReachDebug::odomCb, this);
    status_sub_ = nh_.subscribe(status_topic_, 20, &GoalReachDebug::statusCb, this);

    timer_ = nh_.createTimer(ros::Duration(1.0 / log_hz_), &GoalReachDebug::tick, this);

    ROS_INFO_STREAM("[GOAL_DEBUG] started goal=" << goal_topic_
                    << " odom=" << odom_topic_
                    << " status=" << status_topic_
                    << " frame(global,odom,base)=(" << global_frame_ << "," << odom_frame_ << "," << base_frame_ << ")"
                    << " tol(xy,yaw)=(" << xy_goal_tolerance_ << "," << yaw_goal_tolerance_
                    << ") stop(v,w)=(" << trans_stopped_vel_ << "," << theta_stopped_vel_ << ")");
  }

 private:
  void goalCb(const geometry_msgs::PoseStampedConstPtr& msg) {
    std::lock_guard<std::mutex> lock(mtx_);
    goal_ = *msg;
    has_goal_ = true;
  }

  void odomCb(const nav_msgs::OdometryConstPtr& msg) {
    std::lock_guard<std::mutex> lock(mtx_);
    lin_speed_ = std::hypot(msg->twist.twist.linear.x, msg->twist.twist.linear.y);
    ang_speed_ = std::fabs(msg->twist.twist.angular.z);
    has_speed_ = true;
    odom_x_ = msg->pose.pose.position.x;
    odom_y_ = msg->pose.pose.position.y;
    odom_yaw_ = tf2::getYaw(msg->pose.pose.orientation);
    has_odom_pose_ = true;
  }

  void statusCb(const actionlib_msgs::GoalStatusArrayConstPtr& msg) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (msg->status_list.empty()) {
      last_status_text_ = "EMPTY";
      last_status_code_ = 0;
      return;
    }
    const auto& s = msg->status_list.front();
    last_status_code_ = s.status;
    last_status_text_ = s.text;
  }

  void tick(const ros::TimerEvent&) {
    geometry_msgs::PoseStamped goal_local;
    double lin = 0.0;
    double ang = 0.0;
    bool has_goal = false;
    bool has_speed = false;
    bool has_odom_pose = false;
    uint8_t status_code = 0;
    std::string status_text;
    double ox = 0.0;
    double oy = 0.0;
    double oyaw = 0.0;

    {
      std::lock_guard<std::mutex> lock(mtx_);
      has_goal = has_goal_;
      if (has_goal) goal_local = goal_;
      lin = lin_speed_;
      ang = ang_speed_;
      has_speed = has_speed_;
      has_odom_pose = has_odom_pose_;
      ox = odom_x_;
      oy = odom_y_;
      oyaw = odom_yaw_;
      status_code = last_status_code_;
      status_text = last_status_text_;
    }

    if (!has_goal) {
      ROS_WARN_STREAM_THROTTLE(1.0, "[GOAL_DEBUG] waiting goal topic=" << goal_topic_);
      return;
    }

    geometry_msgs::TransformStamped tf_map_base;
    try {
      tf_map_base = tf_buffer_.lookupTransform(global_frame_, base_frame_, ros::Time(0), ros::Duration(0.05));
    } catch (const tf2::TransformException& ex) {
      ROS_WARN_STREAM_THROTTLE(1.0, "[GOAL_DEBUG] tf unavailable: " << ex.what());
      return;
    }

    const double rx = tf_map_base.transform.translation.x;
    const double ry = tf_map_base.transform.translation.y;
    const double ryaw = tf2::getYaw(tf_map_base.transform.rotation);
    const double gx = goal_local.pose.position.x;
    const double gy = goal_local.pose.position.y;
    const double gyaw = tf2::getYaw(goal_local.pose.orientation);

    const double dxy = std::hypot(rx - gx, ry - gy);
    const double dyaw = std::fabs(normalizeAngle(ryaw - gyaw));

    const bool xy_ok = dxy <= xy_goal_tolerance_;
    const bool yaw_ok = dyaw <= yaw_goal_tolerance_;
    const bool v_ok = has_speed && (lin <= trans_stopped_vel_);
    const bool w_ok = has_speed && (ang <= theta_stopped_vel_);

    std::vector<std::string> reasons;
    if (!xy_ok) reasons.emplace_back("xy_not_reached");
    if (!yaw_ok) reasons.emplace_back("yaw_not_reached");
    if (!v_ok) reasons.emplace_back("linear_not_stopped");
    if (!w_ok) reasons.emplace_back("angular_not_stopped");
    if (!has_speed) reasons.emplace_back("no_speed");

    // 并行计算 odom 系误差：将 goal(全局系) 转到 odom 系后，与 odom 里程计位姿对比。
    bool odom_eval_ok = false;
    double dxy_odom = -1.0;
    double dyaw_odom = -1.0;
    if (has_odom_pose) {
      try {
        const auto tf_odom_global =
            tf_buffer_.lookupTransform(odom_frame_, global_frame_, ros::Time(0), ros::Duration(0.05));
        const double tx = tf_odom_global.transform.translation.x;
        const double ty = tf_odom_global.transform.translation.y;
        const double tyaw = tf2::getYaw(tf_odom_global.transform.rotation);

        const double cg = std::cos(tyaw);
        const double sg = std::sin(tyaw);
        const double gx_odom = tx + cg * gx - sg * gy;
        const double gy_odom = ty + sg * gx + cg * gy;
        const double gyaw_odom = normalizeAngle(gyaw + tyaw);

        dxy_odom = std::hypot(ox - gx_odom, oy - gy_odom);
        dyaw_odom = std::fabs(normalizeAngle(oyaw - gyaw_odom));
        odom_eval_ok = true;
      } catch (const tf2::TransformException& ex) {
        ROS_WARN_STREAM_THROTTLE(1.0, "[GOAL_DEBUG] odom/global tf unavailable: " << ex.what());
      }
    }

    ROS_WARN_STREAM("[GOAL_DEBUG] status=" << static_cast<int>(status_code)
                    << " status_text=\"" << status_text << "\""
                    << " map(dxy,dyaw)=(" << dxy << "," << dyaw << ")/("
                    << xy_goal_tolerance_ << "," << yaw_goal_tolerance_ << ")"
                    << " odom(dxy,dyaw)=(" << dxy_odom << "," << dyaw_odom << ")"
                    << " lin=" << lin << "/" << trans_stopped_vel_
                    << " ang=" << ang << "/" << theta_stopped_vel_
                    << " all_ok=" << ((xy_ok && yaw_ok && v_ok && w_ok) ? "YES" : "NO")
                    << " odom_eval=" << (odom_eval_ok ? "OK" : "N/A")
                    << " frame_gap(dxy,dyaw)=(" << (odom_eval_ok ? std::fabs(dxy - dxy_odom) : -1.0)
                    << "," << (odom_eval_ok ? std::fabs(dyaw - dyaw_odom) : -1.0) << ")"
                    << " reasons=" << joinReasons(reasons));
  }

 private:
  ros::NodeHandle nh_;
  ros::NodeHandle pnh_;
  std::mutex mtx_;

  tf2_ros::Buffer tf_buffer_;
  tf2_ros::TransformListener tf_listener_;

  ros::Subscriber goal_sub_;
  ros::Subscriber odom_sub_;
  ros::Subscriber status_sub_;
  ros::Timer timer_;

  std::string goal_topic_;
  std::string odom_topic_;
  std::string status_topic_;
  std::string global_frame_;
  std::string odom_frame_;
  std::string base_frame_;

  double xy_goal_tolerance_ = 0.30;
  double yaw_goal_tolerance_ = 0.25;
  double trans_stopped_vel_ = 0.05;
  double theta_stopped_vel_ = 0.10;
  double log_hz_ = 5.0;

  bool has_goal_ = false;
  bool has_speed_ = false;
  bool has_odom_pose_ = false;
  geometry_msgs::PoseStamped goal_;
  double lin_speed_ = 0.0;
  double ang_speed_ = 0.0;
  double odom_x_ = 0.0;
  double odom_y_ = 0.0;
  double odom_yaw_ = 0.0;
  uint8_t last_status_code_ = 0;
  std::string last_status_text_;
};

int main(int argc, char** argv) {
  ros::init(argc, argv, "goal_reach_debug");
  GoalReachDebug node;
  ros::spin();
  return 0;
}

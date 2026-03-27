#include <actionlib_msgs/GoalID.h>
#include <geometry_msgs/Twist.h>
#include <hdl_localization/ScanMatchingStatus.h>
#include <nav_msgs/Odometry.h>
#include <ros/ros.h>
#include <std_msgs/Bool.h>
#include <std_msgs/String.h>
#include <std_srvs/Empty.h>
#include <tf2/utils.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <future>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string joinReasons(const std::vector<std::string>& reasons) {
  std::ostringstream oss;
  for (size_t i = 0; i < reasons.size(); ++i) {
    if (i) {
      oss << ",";
    }
    oss << reasons[i];
  }
  return oss.str();
}

bool hasReason(const std::vector<std::string>& reasons, const std::string& key) {
  for (const auto& reason : reasons) {
    if (reason == key) {
      return true;
    }
  }
  return false;
}

double normAngle(double rad) {
  while (rad > M_PI) {
    rad -= 2.0 * M_PI;
  }
  while (rad < -M_PI) {
    rad += 2.0 * M_PI;
  }
  return rad;
}

std::string toStr(double v, int precision = 3) {
  std::ostringstream oss;
  oss.setf(std::ios::fixed);
  oss.precision(precision);
  oss << v;
  return oss.str();
}

std::string expandHomePath(const std::string& path) {
  if (path.size() >= 2 && path[0] == '~' && path[1] == '/') {
    const char* home = std::getenv("HOME");
    if (home != nullptr) {
      return std::string(home) + path.substr(1);
    }
  }
  return path;
}

std::string nowString() {
  const auto now = std::time(nullptr);
  std::tm tm_buf{};
  localtime_r(&now, &tm_buf);

  std::ostringstream oss;
  oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
  return oss.str();
}

std::string red(const std::string& s) {
  return std::string("\033[31m") + s + "\033[0m";
}

}  // namespace

class LocalizationSupervisor {
 public:
  LocalizationSupervisor() : nh_(), pnh_("~") {
    status_topic_ = pnh_.param<std::string>("status_topic", "/status");
    global_odom_topic_ = pnh_.param<std::string>("global_odom_topic", "/odometry/global");

    startup_grace_period_ = pnh_.param("startup_grace_period", 5.0);
    status_timeout_ = pnh_.param("status_timeout", 0.8);
    global_odom_timeout_ = pnh_.param("global_odom_timeout", 1.0);
    min_inlier_fraction_ = pnh_.param("min_inlier_fraction", 0.30);
    max_matching_error_ = pnh_.param("max_matching_error", 0.30);

    jump_trans_thresh_ = pnh_.param("jump_trans_thresh", 0.30);
    jump_yaw_thresh_ = pnh_.param("jump_yaw_thresh", 0.35);
    jump_linear_speed_gate_ = pnh_.param("jump_linear_speed_gate", 0.10);
    jump_angular_speed_gate_ = pnh_.param("jump_angular_speed_gate", 0.20);
    jump_hold_time_ = pnh_.param("jump_hold_time", 2.0);
    jump_dt_min_ = pnh_.param("jump_dt_min", 0.01);
    jump_dt_max_ = pnh_.param("jump_dt_max", 0.30);
    jump_suppress_after_relocalize_ = pnh_.param("jump_suppress_after_relocalize", 2.5);
    jump_suppress_after_recover_ = pnh_.param("jump_suppress_after_recover", 2.0);

    enable_recovery_ = pnh_.param("enable_recovery", true);
    quality_bad_hold_time_ = pnh_.param("quality_bad_hold_time", 1.0);
    recovery_success_hold_time_ = pnh_.param("recovery_success_hold_time", 3.0);
    recovery_min_inlier_fraction_ = pnh_.param("recovery_min_inlier_fraction", 0.95);
    min_relocalize_calls_in_recovery_ = pnh_.param("min_relocalize_calls_in_recovery", 2);
    recovery_attempt_timeout_ = pnh_.param("recovery_attempt_timeout", 15.0);
    max_recovery_attempts_ = pnh_.param("max_recovery_attempts", 3);
    force_recovery_on_jump_ = pnh_.param("force_recovery_on_jump", true);

    cmd_vel_topic_ = pnh_.param<std::string>("cmd_vel_topic", "/cmd_vel");
    cancel_topic_ = pnh_.param<std::string>("cancel_topic", "/move_base_flex/move_base/cancel");
    relocalize_service_ = pnh_.param<std::string>("relocalize_service", "/relocalize");
    enable_rotate_recovery_ = pnh_.param("enable_rotate_recovery", true);
    rotate_speed_ = pnh_.param("rotate_speed", 0.18);  // lower speed for safer in-place recovery
    enable_cancel_navigation_ = pnh_.param("enable_cancel_navigation", true);

    enable_info_log_ = pnh_.param("enable_info_log", true);
    process_on_status_update_ = pnh_.param("process_on_status_update", true);
    log_follow_status_ = pnh_.param("log_follow_status", true);
    normal_info_log_throttle_ = pnh_.param("normal_info_log_throttle", 1.0);
    recovery_info_log_throttle_ = pnh_.param("recovery_info_log_throttle", 0.25);
    enable_periodic_fail_log_ = pnh_.param("enable_periodic_fail_log", false);
    periodic_fail_log_throttle_ = pnh_.param("periodic_fail_log_throttle", 2.0);
    if (normal_info_log_throttle_ <= 0.0) {
      normal_info_log_throttle_ = 1.0;
    }
    if (recovery_info_log_throttle_ <= 0.0) {
      recovery_info_log_throttle_ = 0.25;
    }
    if (periodic_fail_log_throttle_ <= 0.0) {
      periodic_fail_log_throttle_ = 2.0;
    }

    enable_file_log_ = pnh_.param("enable_file_log", true);
    event_log_file_ = expandHomePath(pnh_.param<std::string>("event_log_file", "~/.ros/localization_monitor.log"));

    start_time_ = ros::Time::now();
    jump_flag_until_ = ros::Time(0);
    jump_detection_suppress_until_ = ros::Time(0);

    status_sub_ = nh_.subscribe(status_topic_, 20, &LocalizationSupervisor::statusCb, this);
    global_odom_sub_ = nh_.subscribe(global_odom_topic_, 50, &LocalizationSupervisor::globalOdomCb, this);

    healthy_pub_ = pnh_.advertise<std_msgs::Bool>("healthy", 10);
    summary_pub_ = pnh_.advertise<std_msgs::String>("summary", 10);
    state_pub_ = pnh_.advertise<std_msgs::String>("state", 10);

    cmd_pub_ = nh_.advertise<geometry_msgs::Twist>(cmd_vel_topic_, 10);
    cancel_pub_ = nh_.advertise<actionlib_msgs::GoalID>(cancel_topic_, 5);
    relocalize_client_ = nh_.serviceClient<std_srvs::Empty>(relocalize_service_);

    // Fast internal loop; actual processing is status-driven when process_on_status_update_=true.
    timer_ = nh_.createTimer(ros::Duration(0.01), &LocalizationSupervisor::tick, this);

    ROS_INFO_STREAM("localization_supervisor started: status=" << status_topic_ << " global_odom=" << global_odom_topic_
                                                                << " enable_recovery=" << (enable_recovery_ ? "true" : "false")
                                                                << " relocalize=" << relocalize_service_
                                                                << " rotate_speed=" << rotate_speed_
                                                                << " log_file=" << event_log_file_);
  }

 private:
  enum class RecoverState { NORMAL = 0, RECOVERY = 1, FAILSAFE = 2 };

  std::string stateName(RecoverState s) const {
    switch (s) {
      case RecoverState::NORMAL:
        return "NORMAL";
      case RecoverState::RECOVERY:
        return "RECOVERY";
      case RecoverState::FAILSAFE:
        return "FAILSAFE";
      default:
        return "UNKNOWN";
    }
  }

  void statusCb(const hdl_localization::ScanMatchingStatusConstPtr& msg) {
    std::lock_guard<std::mutex> lock(mtx_);
    latest_status_ = *msg;
    latest_status_valid_ = true;
    last_status_time_ = msg->header.stamp.isZero() ? ros::Time::now() : msg->header.stamp;
    status_updated_ = true;
  }

  void globalOdomCb(const nav_msgs::OdometryConstPtr& msg) {
    const ros::Time now = ros::Time::now();
    const ros::Time stamp = msg->header.stamp.isZero() ? now : msg->header.stamp;

    std::lock_guard<std::mutex> lock(mtx_);
    last_global_odom_time_ = stamp;

    const double x = msg->pose.pose.position.x;
    const double y = msg->pose.pose.position.y;
    const double yaw = tf2::getYaw(msg->pose.pose.orientation);

    if (now < jump_detection_suppress_until_) {
      prev_global_x_ = x;
      prev_global_y_ = y;
      prev_global_yaw_ = yaw;
      prev_global_stamp_ = stamp;
      prev_global_valid_ = true;
      return;
    }

    if (prev_global_valid_) {
      const double dt = (stamp - prev_global_stamp_).toSec();
      if (dt >= jump_dt_min_ && dt <= jump_dt_max_) {
        const double dtrans = std::hypot(x - prev_global_x_, y - prev_global_y_);
        const double dyaw = std::fabs(normAngle(yaw - prev_global_yaw_));

        const double linear_speed = std::hypot(msg->twist.twist.linear.x, msg->twist.twist.linear.y);
        const double angular_speed = std::fabs(msg->twist.twist.angular.z);
        const bool near_static = linear_speed < jump_linear_speed_gate_ && angular_speed < jump_angular_speed_gate_;

        if (near_static && (dtrans > jump_trans_thresh_ || dyaw > jump_yaw_thresh_)) {
          jump_flag_until_ = now + ros::Duration(jump_hold_time_);
          ROS_ERROR_STREAM(red("[LOC_FAIL] jump detected dtrans=" + toStr(dtrans) + " dyaw=" + toStr(dyaw) +
                               " lin=" + toStr(linear_speed) + " ang=" + toStr(angular_speed)));
        }
      }
    }

    prev_global_x_ = x;
    prev_global_y_ = y;
    prev_global_yaw_ = yaw;
    prev_global_stamp_ = stamp;
    prev_global_valid_ = true;
  }

  void evaluateLocked(const ros::Time& now, bool& ok, std::vector<std::string>& reasons) {
    reasons.clear();

    // In simulation with /clock, node startup time can be zero before first clock arrives.
    // Anchor grace period at the first valid time to avoid false startup failures.
    if (now.isZero()) {
      ok = true;
      return;
    }
    if (start_time_.isZero() || now < start_time_) {
      start_time_ = now;
      ok = true;
      return;
    }

    if ((now - start_time_).toSec() < startup_grace_period_) {
      ok = true;
      return;
    }

    if (!latest_status_valid_) {
      reasons.emplace_back("no_status");
    } else {
      if ((now - last_status_time_).toSec() > status_timeout_) {
        reasons.emplace_back("status_timeout");
      }
      if (latest_status_.inlier_fraction < min_inlier_fraction_) {
        reasons.emplace_back("low_inlier");
      }
      if (latest_status_.matching_error > max_matching_error_) {
        reasons.emplace_back("high_error");
      }
    }

    if (last_global_odom_time_.isZero() || (now - last_global_odom_time_).toSec() > global_odom_timeout_) {
      reasons.emplace_back("global_odom_timeout");
    }

    if (now < jump_flag_until_) {
      reasons.emplace_back("pose_jump");
    }

    ok = reasons.empty();
  }

  std::string buildSummaryLocked(bool raw_ok, bool healthy, const std::vector<std::string>& reasons,
                                 const ros::Time& now) const {
    const double status_age = latest_status_valid_ ? (now - last_status_time_).toSec() : -1.0;
    const double odom_age = last_global_odom_time_.isZero() ? -1.0 : (now - last_global_odom_time_).toSec();

    std::ostringstream oss;
    oss.setf(std::ios::fixed);
    oss.precision(3);
    oss << "state=" << stateName(state_)
        << " health=" << (healthy ? "GOOD" : "BAD")
        << " raw=" << (raw_ok ? "GOOD" : "BAD")
        << " inlier=" << (latest_status_valid_ ? toStr(latest_status_.inlier_fraction) : std::string("N/A"))
        << " error=" << (latest_status_valid_ ? toStr(latest_status_.matching_error) : std::string("N/A"))
        << " status_age=" << status_age
        << " odom_age=" << odom_age
        << " jump_left=" << std::max(0.0, (jump_flag_until_ - now).toSec())
        << " jump_suppress_left=" << std::max(0.0, (jump_detection_suppress_until_ - now).toSec())
        << " recovery_attempt=" << recovery_attempts_
        << " relocalize_calls=" << relocalize_calls_in_attempt_
        << " svc_calls=" << total_relocalize_calls_
        << " svc_ok=" << total_relocalize_success_
        << " svc_fail=" << total_relocalize_fail_
        << " reasons=" << (reasons.empty() ? "none" : joinReasons(reasons));
    return oss.str();
  }

  std::string buildBriefSummaryLocked(bool raw_ok, bool healthy, const std::vector<std::string>& reasons,
                                      const ros::Time& now) const {
    std::ostringstream oss;
    oss.setf(std::ios::fixed);
    oss.precision(3);
    oss << "state=" << stateName(state_)
        << " health=" << (healthy ? "GOOD" : "BAD")
        << " inlier=" << (latest_status_valid_ ? toStr(latest_status_.inlier_fraction) : std::string("N/A"))
        << " err=" << (latest_status_valid_ ? toStr(latest_status_.matching_error) : std::string("N/A"))
        << " jump=" << std::max(0.0, (jump_flag_until_ - now).toSec())
        << " suppress=" << std::max(0.0, (jump_detection_suppress_until_ - now).toSec())
        << " reason=" << (reasons.empty() ? "none" : joinReasons(reasons));
    if (state_ == RecoverState::RECOVERY) {
      oss << " total_calls=" << relocalize_calls_in_attempt_
          << " svc=" << total_relocalize_calls_
          << "/" << total_relocalize_success_
          << "/" << total_relocalize_fail_;
    }
    return oss.str();
  }

  void appendEventLog(const std::string& tag, const std::string& summary) {
    if (!enable_file_log_) {
      return;
    }

    std::ofstream ofs(event_log_file_, std::ios::app);
    if (!ofs.is_open()) {
      ROS_WARN_STREAM_THROTTLE(5.0, "failed to open monitor log file, path=" << event_log_file_);
      return;
    }

    ofs << nowString() << " [" << tag << "] " << summary << "\n";
  }

  void publishStop() {
    geometry_msgs::Twist cmd;
    cmd_pub_.publish(cmd);
  }

  void publishRotate(bool positive) {
    geometry_msgs::Twist cmd;
    cmd.angular.z = positive ? rotate_speed_ : -rotate_speed_;
    cmd_pub_.publish(cmd);
  }

  void cancelNavigation() {
    actionlib_msgs::GoalID cancel_msg;
    cancel_pub_.publish(cancel_msg);
  }

  bool callRelocalize(std::string& result_msg) {
    result_msg.clear();

    if (!relocalize_client_.waitForExistence(ros::Duration(0.3))) {
      result_msg = "relocalize service unavailable";
      return false;
    }

    std_srvs::Empty srv;
    if (relocalize_client_.call(srv)) {
      result_msg = "relocalize called";
      return true;
    }

    result_msg = "relocalize call failed";
    return false;
  }

  void relocalizeWorker(int call_seq, int call_attempt) {
    std::string relocalize_msg;
    const bool ok = callRelocalize(relocalize_msg);

    int total_calls = 0;
    int total_ok = 0;
    int total_fail = 0;
    {
      std::lock_guard<std::mutex> lock(mtx_);
      last_relocalize_result_time_ = ros::Time::now();
      if (ok) {
        total_relocalize_success_++;
        const ros::Time suppress_until = ros::Time::now() + ros::Duration(jump_suppress_after_relocalize_);
        if (suppress_until > jump_detection_suppress_until_) {
          jump_detection_suppress_until_ = suppress_until;
        }
      } else {
        total_relocalize_fail_++;
      }
      total_calls = total_relocalize_calls_;
      total_ok = total_relocalize_success_;
      total_fail = total_relocalize_fail_;
      relocalize_in_flight_ = false;
    }

    ROS_WARN_STREAM("[LOC_SM] RESULT seq=" << call_seq
                                           << " attempt=" << call_attempt
                                           << " ok=" << (ok ? "YES" : "NO")
                                           << " msg=" << relocalize_msg
                                           << " totals=" << total_calls
                                           << "/" << total_ok
                                           << "/" << total_fail);
    appendEventLog(ok ? "RELOCALIZE_OK" : "RELOCALIZE_FAIL", relocalize_msg);
  }

  void startRecoveryAttemptLocked(const ros::Time& now, bool& do_cancel) {
    recovery_attempts_++;
    recovery_attempt_started_ = now;
    last_relocalize_result_time_ = ros::Time(0);
    relocalize_calls_in_attempt_ = 0;
    good_since_ = ros::Time(0);
    if (enable_cancel_navigation_) {
      do_cancel = true;
    }
  }

  void tick(const ros::TimerEvent&) {
    const ros::Time now = ros::Time::now();
    if (process_on_status_update_) {
      bool should_process = false;
      {
        std::lock_guard<std::mutex> lock(mtx_);
        const bool status_stale =
            latest_status_valid_ && ((now - last_status_time_).toSec() > status_timeout_);
        const bool no_status_after_grace =
            !latest_status_valid_ &&
            !start_time_.isZero() &&
            ((now - start_time_).toSec() > startup_grace_period_);

        should_process = status_updated_ || status_stale || no_status_after_grace;
        status_updated_ = false;
      }
      if (!should_process) {
        return;
      }
    }

    bool raw_ok = false;
    bool healthy = false;
    bool do_rotate = false;
    bool do_stop = false;
    bool do_cancel = false;
    bool do_relocalize = false;
    bool rotate_positive = true;
    int relocalize_call_seq = 0;
    int relocalize_call_attempt = 0;
    std::vector<std::string> reasons;
    std::string summary;
    std::string brief_summary;
    std::string state_str;

    {
      std::lock_guard<std::mutex> lock(mtx_);
      evaluateLocked(now, raw_ok, reasons);

      if (state_ == RecoverState::NORMAL) {
        if (raw_ok) {
          bad_since_ = ros::Time(0);
        } else if (enable_recovery_) {
          const bool jump_trigger = force_recovery_on_jump_ && hasReason(reasons, "pose_jump");
          if (jump_trigger) {
            state_ = RecoverState::RECOVERY;
            state_since_ = now;
            startRecoveryAttemptLocked(now, do_cancel);
          } else {
            if (bad_since_.isZero()) {
              bad_since_ = now;
            }
            if ((now - bad_since_).toSec() >= quality_bad_hold_time_) {
              state_ = RecoverState::RECOVERY;
              state_since_ = now;
              startRecoveryAttemptLocked(now, do_cancel);
            }
          }
        }
      }

      if (state_ == RecoverState::RECOVERY) {
        // Recovery flow: REQUEST -> WAIT_RESULT -> VERIFY
        // WAIT phase: rotate. VERIFY phase: stop.
        do_rotate = false;
        do_stop = true;
        rotate_positive = true;
        const bool recovery_inlier_ok =
            latest_status_valid_ && (latest_status_.inlier_fraction >= recovery_min_inlier_fraction_);
        const bool recovery_calls_ok =
            (relocalize_calls_in_attempt_ >= min_relocalize_calls_in_recovery_);
        const bool recovery_gate_ok =
            raw_ok && recovery_inlier_ok && recovery_calls_ok;
        const bool no_relocalize_in_flight = !relocalize_in_flight_;
        const double since_last_result = last_relocalize_result_time_.isZero()
                                             ? -1.0
                                             : (now - last_relocalize_result_time_).toSec();

        const double hold_elapsed = good_since_.isZero() ? 0.0 : (now - good_since_).toSec();
        if (relocalize_in_flight_) {
          do_rotate = enable_rotate_recovery_;
          do_stop = !enable_rotate_recovery_;
          const double wait_elapsed = relocalize_request_start_time_.isZero() ? 0.0 : (now - relocalize_request_start_time_).toSec();
          ROS_WARN_STREAM("[LOC_SM] WAIT elapsed=" << toStr(wait_elapsed) << "s");
          good_since_ = ros::Time(0);
        } else if (last_relocalize_result_time_.isZero()) {
          // First request in this recovery attempt
          relocalize_calls_in_attempt_++;
          total_relocalize_calls_++;
          do_relocalize = true;
          relocalize_call_seq = total_relocalize_calls_;
          relocalize_call_attempt = recovery_attempts_;
          relocalize_in_flight_ = true;
          relocalize_request_start_time_ = now;
          ROS_WARN_STREAM("[LOC_SM] REQUEST trigger=enter_recovery"
                          << " seq=" << relocalize_call_seq
                          << " attempt=" << relocalize_call_attempt);
        } else {
          // Verify stage after getting at least one relocalize result
          do_rotate = false;
          do_stop = true;
          if (recovery_gate_ok) {
            if (good_since_.isZero()) {
              good_since_ = now;
            }
            const double verify_left = std::max(0.0, recovery_success_hold_time_ - hold_elapsed);
            ROS_WARN_STREAM("[LOC_SM] VERIFY pass left=" << toStr(verify_left) << "s"
                            << " inlier=" << (latest_status_valid_ ? toStr(latest_status_.inlier_fraction) : std::string("N/A")));
            if ((now - good_since_).toSec() >= recovery_success_hold_time_) {
              state_ = RecoverState::NORMAL;
              state_since_ = now;
              bad_since_ = ros::Time(0);
              good_since_ = ros::Time(0);
              do_stop = true;
              do_rotate = false;
              recovery_attempts_ = 0;
              relocalize_calls_in_attempt_ = 0;
              jump_detection_suppress_until_ = now + ros::Duration(jump_suppress_after_recover_);
              ROS_WARN_STREAM("[LOC_SM] VERIFY success -> NORMAL");
            }
          } else {
            good_since_ = ros::Time(0);
            const double retry_left = std::max(0.0, recovery_success_hold_time_ - std::max(0.0, since_last_result));
            ROS_WARN_STREAM("[LOC_SM] VERIFY fail left_to_retry=" << toStr(retry_left) << "s"
                            << " raw_ok=" << (raw_ok ? "YES" : "NO")
                            << " inlier_ok=" << (recovery_inlier_ok ? "YES" : "NO")
                            << " calls_ok=" << (recovery_calls_ok ? "YES" : "NO")
                            << " inlier=" << (latest_status_valid_ ? toStr(latest_status_.inlier_fraction) : std::string("N/A")));
            if (since_last_result >= recovery_success_hold_time_) {
              relocalize_calls_in_attempt_++;
              total_relocalize_calls_++;
              do_relocalize = true;
              relocalize_call_seq = total_relocalize_calls_;
              relocalize_call_attempt = recovery_attempts_;
              relocalize_in_flight_ = true;
              relocalize_request_start_time_ = now;
              ROS_WARN_STREAM("[LOC_SM] REQUEST trigger=verify_failed_after_hold"
                              << " seq=" << relocalize_call_seq
                              << " attempt=" << relocalize_call_attempt);
            }
          }
        }

        if (state_ == RecoverState::RECOVERY && !recovery_attempt_started_.isZero() &&
            (now - recovery_attempt_started_).toSec() >= recovery_attempt_timeout_ && !recovery_gate_ok) {
          if (recovery_attempts_ < max_recovery_attempts_) {
            startRecoveryAttemptLocked(now, do_cancel);
            ROS_WARN_STREAM("[LOC_RECOVERY] retry recovery attempt=" << recovery_attempts_);
          } else {
            state_ = RecoverState::FAILSAFE;
            state_since_ = now;
            do_stop = true;
            do_rotate = false;
            ROS_ERROR_STREAM(red("[LOC_SM] FAILSAFE max_recovery_attempts exceeded"));
          }
        }
      } else if (state_ == RecoverState::FAILSAFE) {
        do_stop = true;
        do_rotate = false;
      }

      healthy = (state_ == RecoverState::NORMAL) && raw_ok;
      summary = buildSummaryLocked(raw_ok, healthy, reasons, now);
      brief_summary = buildBriefSummaryLocked(raw_ok, healthy, reasons, now);
      state_str = stateName(state_);
    }

    if (do_cancel) {
      cancelNavigation();
    }

    if (do_stop) {
      publishStop();
    } else if (do_rotate) {
      publishRotate(rotate_positive);
    }

    if (do_relocalize) {
      relocalize_future_ = std::async(std::launch::async, &LocalizationSupervisor::relocalizeWorker, this,
                                      relocalize_call_seq, relocalize_call_attempt);
    }

    std_msgs::Bool healthy_msg;
    healthy_msg.data = healthy;
    healthy_pub_.publish(healthy_msg);

    std_msgs::String summary_msg;
    summary_msg.data = brief_summary;
    summary_pub_.publish(summary_msg);

    std_msgs::String state_msg;
    state_msg.data = state_str;
    state_pub_.publish(state_msg);

    if (enable_info_log_ && state_ == RecoverState::NORMAL) {
      if (process_on_status_update_ && log_follow_status_) {
        ROS_INFO_STREAM("[LOC_MONITOR] " << brief_summary);
      } else {
        ROS_INFO_STREAM_THROTTLE(normal_info_log_throttle_, "[LOC_MONITOR] " << brief_summary);
      }
    }

    if (healthy != last_health_) {
      appendEventLog(healthy ? "RECOVER" : "FAIL", summary);
      last_health_ = healthy;
    }
  }

 private:
  ros::NodeHandle nh_;
  ros::NodeHandle pnh_;
  std::mutex mtx_;

  // Monitor params
  std::string status_topic_;
  std::string global_odom_topic_;
  double startup_grace_period_ = 5.0;
  double status_timeout_ = 0.8;
  double global_odom_timeout_ = 1.0;
  double min_inlier_fraction_ = 0.30;
  double max_matching_error_ = 0.30;

  // Pose jump params
  double jump_trans_thresh_ = 0.30;
  double jump_yaw_thresh_ = 0.35;
  double jump_linear_speed_gate_ = 0.10;
  double jump_angular_speed_gate_ = 0.20;
  double jump_hold_time_ = 2.0;
  double jump_dt_min_ = 0.01;
  double jump_dt_max_ = 0.30;
  double jump_suppress_after_relocalize_ = 2.5;
  double jump_suppress_after_recover_ = 2.0;

  // Recovery params
  bool enable_recovery_ = true;
  double quality_bad_hold_time_ = 1.0;
  double recovery_success_hold_time_ = 3.0;
  double recovery_min_inlier_fraction_ = 0.95;
  int min_relocalize_calls_in_recovery_ = 2;
  double recovery_attempt_timeout_ = 15.0;
  int max_recovery_attempts_ = 3;
  bool force_recovery_on_jump_ = true;

  std::string cmd_vel_topic_;
  std::string cancel_topic_;
  std::string relocalize_service_;
  bool enable_rotate_recovery_ = true;
  double rotate_speed_ = 0.18;
  bool enable_cancel_navigation_ = true;

  // Logging params
  bool enable_info_log_ = true;
  bool process_on_status_update_ = true;
  bool log_follow_status_ = true;
  double normal_info_log_throttle_ = 1.0;
  double recovery_info_log_throttle_ = 0.25;
  bool enable_periodic_fail_log_ = false;
  double periodic_fail_log_throttle_ = 2.0;
  bool enable_file_log_ = true;
  std::string event_log_file_;

  // State
  RecoverState state_ = RecoverState::NORMAL;
  ros::Time start_time_;
  ros::Time state_since_;
  ros::Time jump_flag_until_;
  ros::Time jump_detection_suppress_until_;
  ros::Time bad_since_;
  ros::Time good_since_;
  ros::Time recovery_attempt_started_;
  ros::Time last_relocalize_result_time_;
  ros::Time relocalize_request_start_time_;
  int recovery_attempts_ = 0;
  int relocalize_calls_in_attempt_ = 0;
  int total_relocalize_calls_ = 0;
  int total_relocalize_success_ = 0;
  int total_relocalize_fail_ = 0;
  bool relocalize_in_flight_ = false;
  std::future<void> relocalize_future_;
  bool last_health_ = true;

  // Latest sensor info
  bool latest_status_valid_ = false;
  hdl_localization::ScanMatchingStatus latest_status_;
  ros::Time last_status_time_;
  ros::Time last_global_odom_time_;

  bool prev_global_valid_ = false;
  double prev_global_x_ = 0.0;
  double prev_global_y_ = 0.0;
  double prev_global_yaw_ = 0.0;
  ros::Time prev_global_stamp_;
  bool status_updated_ = false;

  // ROS I/O
  ros::Subscriber status_sub_;
  ros::Subscriber global_odom_sub_;
  ros::Publisher healthy_pub_;
  ros::Publisher summary_pub_;
  ros::Publisher state_pub_;
  ros::Publisher cmd_pub_;
  ros::Publisher cancel_pub_;
  ros::ServiceClient relocalize_client_;
  ros::Timer timer_;
};

int main(int argc, char** argv) {
  ros::init(argc, argv, "localization_supervisor");
  LocalizationSupervisor node;
  ros::spin();
  return 0;
}

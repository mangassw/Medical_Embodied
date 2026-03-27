#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <mutex>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>

#include <actionlib/server/simple_action_server.h>
#include <boost/bind.hpp>
#include <ros/ros.h>
#include <std_msgs/String.h>
#include <std_srvs/Trigger.h>
#include <XmlRpcValue.h>

#include <interfaces/ActionStatus.h>
#include <interfaces/NavigateAction.h>
#include <xjrobot_task/PatrolAddWaypoint.h>

namespace {

// 统一目标位姿结构：桥接内部仅处理 map 系下的 x/y/yaw。
struct TargetPose {
  double x{0.0};
  double y{0.0};
  double yaw{0.0};
};

// 将字符串转小写，便于处理 nav_type 等不区分大小写的输入。
std::string toLower(std::string s) {
  for (char& c : s) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return s;
}

// 保存巡检状态机 ext 的精简状态快照。
// 状态来自 /patrol_state_machine_ext/status（k=v;k=v;...）。
struct PatrolExtStatus {
  bool valid{false};
  std::string state;
  std::string current_source;
  int preempt_size{0};
};

class NavigateXjBridgeServer {
 public:
  NavigateXjBridgeServer()
      : pnh_("~"),
        action_server_(nh_, "navigate", boost::bind(&NavigateXjBridgeServer::executeCb, this, _1), false) {
    // ---------------- 参数读取 ----------------
    // ext 命名空间。默认按 xj_robot0202 的启动约定。
    pnh_.param<std::string>("patrol_ext_ns", patrol_ext_ns_, "/patrol_state_machine_ext");
    // add_waypoint 的注入策略与是否硬抢占。
    pnh_.param<std::string>("ext_policy", ext_policy_, "INSERT_NEXT_ONCE");
    pnh_.param("ext_hard_preempt", ext_hard_preempt_, true);
    // 每次导航前是否自动触发 start（建议开启，降低上层时序耦合）。
    pnh_.param("ext_auto_start", ext_auto_start_, true);
    // 服务等待与整次调用超时。
    pnh_.param("wait_service_timeout", wait_service_timeout_sec_, 5.0);
    pnh_.param("call_timeout", call_timeout_sec_, 120.0);
    // 目标未给 yaw 时使用默认值。
    pnh_.param("default_yaw", default_yaw_, 0.0);

    loadTargetMap();

    // ---------------- 服务/话题绑定 ----------------
    const std::string add_waypoint_srv = patrol_ext_ns_ + "/add_waypoint";
    const std::string start_srv = patrol_ext_ns_ + "/start";
    const std::string stop_srv = patrol_ext_ns_ + "/stop";
    const std::string skip_current_srv = patrol_ext_ns_ + "/skip_current";
    const std::string status_topic = patrol_ext_ns_ + "/status";

    ext_add_waypoint_client_ = nh_.serviceClient<xjrobot_task::PatrolAddWaypoint>(add_waypoint_srv);
    ext_start_client_ = nh_.serviceClient<std_srvs::Trigger>(start_srv);
    ext_stop_client_ = nh_.serviceClient<std_srvs::Trigger>(stop_srv);
    ext_skip_current_client_ = nh_.serviceClient<std_srvs::Trigger>(skip_current_srv);
    ext_status_sub_ = nh_.subscribe(status_topic, 5, &NavigateXjBridgeServer::extStatusCb, this);

    action_server_.start();
    ROS_INFO("[navigate_xj_bridge] started, ext_ns=%s", patrol_ext_ns_.c_str());
  }

 private:
  // 读取 target_map：支持 target-> [x,y,yaw?]。
  // 如果缺失，则仍可使用 "x,y" / "x,y,yaw" 直传模式。
  void loadTargetMap() {
    target_map_.clear();

    XmlRpc::XmlRpcValue map_param;
    if (!pnh_.getParam("target_map", map_param)) {
      ROS_WARN("[navigate_xj_bridge] no target_map param, only numeric target strings are supported");
      return;
    }

    if (map_param.getType() != XmlRpc::XmlRpcValue::TypeStruct) {
      ROS_ERROR("[navigate_xj_bridge] target_map must be a dictionary");
      return;
    }

    for (auto it = map_param.begin(); it != map_param.end(); ++it) {
      const std::string key = it->first;
      auto& value = it->second;
      if (value.getType() != XmlRpc::XmlRpcValue::TypeArray || value.size() < 2) {
        ROS_WARN("[navigate_xj_bridge] target_map[%s] must be [x, y, yaw?]", key.c_str());
        continue;
      }

      try {
        TargetPose pose;
        pose.x = xmlToDouble(value[0]);
        pose.y = xmlToDouble(value[1]);
        pose.yaw = value.size() >= 3 ? xmlToDouble(value[2]) : default_yaw_;
        target_map_[key] = pose;
      } catch (...) {
        ROS_WARN("[navigate_xj_bridge] target_map[%s] parse failed", key.c_str());
      }
    }

    ROS_INFO("[navigate_xj_bridge] loaded %zu target mappings", target_map_.size());
  }

  static double xmlToDouble(const XmlRpc::XmlRpcValue& v) {
    if (v.getType() == XmlRpc::XmlRpcValue::TypeInt) {
      return static_cast<int>(v);
    }
    if (v.getType() == XmlRpc::XmlRpcValue::TypeDouble) {
      return static_cast<double>(v);
    }
    throw std::runtime_error("target_map element must be int or double");
  }

  // 目标解析策略：
  // 1) target_map 命名目标；2) current 特殊值；3) 直接数值字符串。
  bool resolveTarget(const std::string& target, TargetPose& out) const {
    auto it = target_map_.find(target);
    if (it != target_map_.end()) {
      out = it->second;
      return true;
    }

    if (target == "current") {
      out = TargetPose{0.0, 0.0, default_yaw_};
      return true;
    }

    static const std::regex pattern(
        R"(^\s*(-?\d+(?:\.\d+)?)\s*,\s*(-?\d+(?:\.\d+)?)(?:\s*,\s*(-?\d+(?:\.\d+)?))?\s*$)");
    std::smatch match;
    if (std::regex_match(target, match, pattern)) {
      out.x = std::stod(match[1].str());
      out.y = std::stod(match[2].str());
      out.yaw = match[3].matched ? std::stod(match[3].str()) : default_yaw_;
      return true;
    }

    return false;
  }

  static int parseIntSafe(const std::string& v, int fallback = 0) {
    try {
      return std::stoi(v);
    } catch (...) {
      return fallback;
    }
  }

  // 订阅 ext 状态字符串，提取桥接真正需要的最小字段。
  void extStatusCb(const std_msgs::String::ConstPtr& msg) {
    PatrolExtStatus parsed;
    parsed.valid = true;

    std::stringstream ss(msg->data);
    std::string token;
    while (std::getline(ss, token, ';')) {
      const auto pos = token.find('=');
      if (pos == std::string::npos) {
        continue;
      }
      const std::string key = token.substr(0, pos);
      const std::string value = token.substr(pos + 1);

      if (key == "state") {
        parsed.state = value;
      } else if (key == "current_source") {
        parsed.current_source = value;
      } else if (key == "preempt_size") {
        parsed.preempt_size = parseIntSafe(value, 0);
      }
    }

    std::lock_guard<std::mutex> lock(ext_status_mu_);
    ext_status_ = parsed;
  }

  PatrolExtStatus getExtStatusSnapshot() const {
    std::lock_guard<std::mutex> lock(ext_status_mu_);
    return ext_status_;
  }

  interfaces::NavigateResult makeResult(uint8_t status, const std::string& message) const {
    interfaces::NavigateResult result;
    result.status.status = status;
    result.message = message;
    return result;
  }

  void publishFeedback(float progress) {
    interfaces::NavigateFeedback feedback;
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    feedback.progress = progress;
    action_server_.publishFeedback(feedback);
  }

  // Trigger 服务统一调用封装。
  // 访问失败或服务回执 success=false 都返回 false，并写入 err。
  bool callTrigger(ros::ServiceClient& client, const std::string& name, std::string* err = nullptr) {
    if (!client.waitForExistence(ros::Duration(wait_service_timeout_sec_))) {
      if (err) *err = name + " unavailable";
      return false;
    }
    std_srvs::Trigger srv;
    if (!client.call(srv)) {
      if (err) *err = name + " call failed";
      return false;
    }
    if (!srv.response.success) {
      if (err) *err = name + " rejected: " + srv.response.message;
      return false;
    }
    return true;
  }

  // 核心执行流程：
  // 1) 可选 start
  // 2) add_waypoint 注入目标
  // 3) 监听状态机 status，直到“该 PREEMPT 目标执行完毕并退出 PREEMPT”
  // 4) 超时/抢占时主动 skip_current + stop，并向行为树返回失败
  bool runViaPatrolExt(const TargetPose& pose, std::string& err_msg, bool& timed_out, bool& preempted) {
    timed_out = false;
    preempted = false;

    if (!ext_add_waypoint_client_.waitForExistence(ros::Duration(wait_service_timeout_sec_))) {
      err_msg = "patrol_ext add_waypoint unavailable";
      return false;
    }

    if (ext_auto_start_) {
      if (!callTrigger(ext_start_client_, "patrol_ext/start", &err_msg)) {
        return false;
      }
    }

    const PatrolExtStatus before = getExtStatusSnapshot();
    const int before_preempt_size = before.valid ? before.preempt_size : 0;

    xjrobot_task::PatrolAddWaypoint srv;
    srv.request.pose.header.frame_id = "map";
    srv.request.pose.header.stamp = ros::Time::now();
    srv.request.pose.pose.position.x = pose.x;
    srv.request.pose.pose.position.y = pose.y;
    srv.request.pose.pose.position.z = 0.0;
    const double half_yaw = 0.5 * pose.yaw;
    srv.request.pose.pose.orientation.w = std::cos(half_yaw);
    srv.request.pose.pose.orientation.x = 0.0;
    srv.request.pose.pose.orientation.y = 0.0;
    srv.request.pose.pose.orientation.z = std::sin(half_yaw);
    srv.request.policy = ext_policy_;
    srv.request.hard_preempt = ext_hard_preempt_;

    if (!ext_add_waypoint_client_.call(srv)) {
      err_msg = "patrol_ext add_waypoint call failed";
      return false;
    }
    if (!srv.response.success) {
      err_msg = "patrol_ext rejected: " + srv.response.message;
      return false;
    }

    bool seen_preempt_source = false;
    const ros::Time start = ros::Time::now();
    ros::Rate rate(10.0);

    while (ros::ok()) {
      const double elapsed = (ros::Time::now() - start).toSec();
      if (elapsed > call_timeout_sec_) {
        timed_out = true;
        callTrigger(ext_skip_current_client_, "patrol_ext/skip_current", nullptr);
        callTrigger(ext_stop_client_, "patrol_ext/stop", nullptr);
        err_msg = "patrol_ext wait timeout";
        return false;
      }

      if (action_server_.isPreemptRequested()) {
        preempted = true;
        callTrigger(ext_skip_current_client_, "patrol_ext/skip_current", nullptr);
        callTrigger(ext_stop_client_, "patrol_ext/stop", nullptr);
        err_msg = "preempted";
        return false;
      }

      const PatrolExtStatus st = getExtStatusSnapshot();
      if (st.valid) {
        if (st.state == "FAILED") {
          err_msg = "patrol_ext entered FAILED state";
          return false;
        }

        if (st.current_source == "PREEMPT") {
          seen_preempt_source = true;
        }

        // 完成判定：
        // - 已经进入过 PREEMPT（说明本次注入目标开始执行）
        // - 当前已离开 PREEMPT，且 preempt 队列回到注入前水平（说明本次目标已消费）
        const bool returned_from_preempt = seen_preempt_source && st.current_source != "PREEMPT";
        const bool preempt_queue_drained = st.preempt_size <= before_preempt_size;
        if (returned_from_preempt && preempt_queue_drained) {
          return true;
        }
      }

      publishFeedback(static_cast<float>(std::min(0.99, elapsed / std::max(1.0, call_timeout_sec_))));
      rate.sleep();
    }

    err_msg = "ros shutdown";
    return false;
  }

  void executeCb(const interfaces::NavigateGoalConstPtr& goal) {
    const std::string nav_type = toLower(goal->nav_type);
    const std::string target = goal->target;

    if (action_server_.isPreemptRequested()) {
      action_server_.setPreempted(makeResult(interfaces::ActionStatus::PREEMPTED, "preempted before dispatch"));
      return;
    }

    // stop 语义：直接调用 ext stop。若服务不可用或失败，返回 ABORTED 给行为树。
    if (nav_type == "stop") {
      std::string err;
      if (callTrigger(ext_stop_client_, "patrol_ext/stop", &err)) {
        publishFeedback(1.0f);
        action_server_.setSucceeded(makeResult(interfaces::ActionStatus::OK, "stop accepted"));
      } else {
        action_server_.setAborted(makeResult(interfaces::ActionStatus::ABORTED, err));
      }
      return;
    }

    TargetPose pose;
    if (!resolveTarget(target, pose)) {
      action_server_.setAborted(makeResult(interfaces::ActionStatus::NO_PATH, "unknown target: " + target));
      return;
    }

    std::string err;
    bool timed_out = false;
    bool preempted = false;
    const bool ok = runViaPatrolExt(pose, err, timed_out, preempted);

    if (ok) {
      publishFeedback(1.0f);
      action_server_.setSucceeded(makeResult(interfaces::ActionStatus::OK, "ok"));
      return;
    }

    if (preempted) {
      action_server_.setPreempted(makeResult(interfaces::ActionStatus::PREEMPTED, err));
      return;
    }

    if (timed_out) {
      action_server_.setAborted(makeResult(interfaces::ActionStatus::TIMEOUT, err));
      return;
    }

    // 访问状态机相关服务失败、状态机异常等，统一回给行为树 ABORTED。
    action_server_.setAborted(makeResult(interfaces::ActionStatus::ABORTED, err));
  }

  ros::NodeHandle nh_;
  ros::NodeHandle pnh_;
  actionlib::SimpleActionServer<interfaces::NavigateAction> action_server_;

  ros::ServiceClient ext_add_waypoint_client_;
  ros::ServiceClient ext_start_client_;
  ros::ServiceClient ext_stop_client_;
  ros::ServiceClient ext_skip_current_client_;
  ros::Subscriber ext_status_sub_;

  mutable std::mutex ext_status_mu_;
  PatrolExtStatus ext_status_;

  std::string patrol_ext_ns_;
  std::string ext_policy_;
  bool ext_hard_preempt_{true};
  bool ext_auto_start_{true};

  double wait_service_timeout_sec_{5.0};
  double call_timeout_sec_{120.0};
  double default_yaw_{0.0};
  std::map<std::string, TargetPose> target_map_;
};

}  // namespace

int main(int argc, char** argv) {
  ros::init(argc, argv, "navigate_xj_bridge_server");
  NavigateXjBridgeServer server;
  ros::spin();
  return 0;
}

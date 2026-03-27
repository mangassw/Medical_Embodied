#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <deque>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <actionlib/client/simple_action_client.h>
#include <actionlib/client/terminal_state.h>
#include <geometry_msgs/PoseStamped.h>
#include <geometry_msgs/TransformStamped.h>
#include <geometry_msgs/Twist.h>
#include <mbf_msgs/MoveBaseAction.h>
#include <nav_msgs/Odometry.h>
#include <nav_msgs/Path.h>
#include <ros/ros.h>
#include <std_msgs/String.h>
#include <std_srvs/Trigger.h>
#include <tf2/LinearMath/Quaternion.h>
#include <tf2/utils.h>
#include <tf2_ros/buffer.h>
#include <tf2_ros/transform_listener.h>
#include <visualization_msgs/Marker.h>
#include <visualization_msgs/MarkerArray.h>
#include <xjrobot_task/PatrolAddWaypoint.h>
#include <xjrobot_task/PatrolAddWaypoints.h>
#include <xjrobot_task/PatrolSetMode.h>

class PatrolStateMachineExternal {
 public:
  // 主状态机状态定义：
  // IDLE      空闲，等待任务启动
  // NAVIGATING 正在向当前目标导航
  // PAUSE     内部暂停态（用于重试、等待取消完成等）
  // PAUSED    用户暂停态（等待 RESUME）
  // FAILED    不可恢复错误态
  enum class State {
    IDLE = 0,
    NAVIGATING = 1,
    PAUSE = 2,
    PAUSED = 3,
    FAILED = 4
  };

  // 基础路线遍历模式：
  // 说明：CSV 第一个点(索引0)为原点/返航点，巡检点从第二个点开始
  // LOOP               巡检点循环模式（不包含索引0）
  // ONCE_AND_STOP      巡检点执行一轮后停止（不回原点）
  // ONCE_AND_RETURN    巡检点执行一轮后回到索引0并停止
  // RETURN_IMMEDIATELY 立刻返回索引0，不执行巡检点
  enum class TraverseMode {
    LOOP = 0,
    ONCE_AND_STOP = 1,
    ONCE_AND_RETURN = 2,
    RETURN_IMMEDIATELY = 3
  };

  // 当前激活目标来源：
  // BASE_ROUTE    来自基础路线
  // OVERLAY_ONCE  来自一次性插入队列
  // PREEMPT_ONCE  来自抢占队列
  enum class GoalSource {
    NONE = 0,
    BASE_ROUTE = 1,
    OVERLAY_ONCE = 2,
    PREEMPT_ONCE = 3
  };

  // PAUSE 状态细分原因，便于在统一状态下实现不同处理路径。
  enum class PauseReason {
    NONE = 0,
    NAV_FAILURE_RETRY = 1,
    WAIT_CANCEL_FOR_PREEMPT = 2,
    WAIT_CANCEL_FOR_PAUSE = 3,
    WAIT_CANCEL_FOR_STOP = 4,
    WAIT_CANCEL_FOR_MODE_SWITCH = 5
  };

  // 动态加点策略：
  // INSERT_NEXT_ONCE   插入下一目标并执行一次
  // INSERT_NEXT_PERSISTENT 永久插入到基础路线的下一目标位
  // APPEND_ONCE        一次性追加到临时队列尾部
  // APPEND_PERSISTENT  永久追加到基础路线
  enum class AddPolicy {
    INSERT_NEXT_ONCE = 0,
    INSERT_NEXT_PERSISTENT = 1,
    APPEND_ONCE = 2,
    APPEND_PERSISTENT = 3
  };

  using MoveBaseClient = actionlib::SimpleActionClient<mbf_msgs::MoveBaseAction>;

  PatrolStateMachineExternal()
      : pnh_("~"), tf_buffer_(), tf_listener_(tf_buffer_) {
    // ------------------ 读取参数 ------------------
    action_name_ = pnh_.param<std::string>("action_name", "/move_base_flex/move_base");
    waypoint_file_ = pnh_.param<std::string>("waypoint_file", "");
    waypoint_frame_ = pnh_.param<std::string>("waypoint_frame", "map");
    base_frame_ = pnh_.param<std::string>("base_frame", "base_link");
    marker_topic_ = pnh_.param<std::string>("marker_topic", "patrol_waypoints_markers_ext");
    path_topic_ = pnh_.param<std::string>("path_topic", "patrol_waypoints_path_ext");
    odom_topic_ = pnh_.param<std::string>("odom_topic", "/odom");
    cmd_vel_topic_ = pnh_.param<std::string>("cmd_vel_topic", "/cmd_vel");
    abnormal_log_file_ = pnh_.param<std::string>("abnormal_log_file", "/tmp/patrol_state_machine_ext.log");

    retry_max_ = std::max(1, pnh_.param("retry_max", 5));
    single_index_ = pnh_.param("single_index", -1);
    auto_start_ = pnh_.param("auto_start", true);
    tick_hz_ = std::max(1.0, pnh_.param("tick_hz", 10.0));
    progress_log_period_ = std::max(0.2, pnh_.param("progress_log_period", 1.0));

    enable_stuck_replan_ = pnh_.param("enable_stuck_replan", true);
    stuck_timeout_sec_ = std::max(0.5, pnh_.param("stuck_timeout_sec", 5.0));
    stuck_linear_vel_eps_ = std::max(0.0, pnh_.param("stuck_linear_vel_eps", 0.02));
    stuck_angular_vel_eps_ = std::max(0.0, pnh_.param("stuck_angular_vel_eps", 0.05));
    force_stop_on_pause_ = pnh_.param("force_stop_on_pause", true);
    force_stop_duration_sec_ = std::max(0.0, pnh_.param("force_stop_duration_sec", 1.0));

    const std::string mode_str = pnh_.param<std::string>("traverse_mode", "LOOP");
    traverse_mode_ = parseTraverseMode(mode_str);

    const std::string add_policy_str = pnh_.param<std::string>("default_add_policy", "INSERT_NEXT_ONCE");
    default_add_policy_ = parseAddPolicy(add_policy_str);

    // ------------------ 通信对象 ------------------
    marker_pub_ = nh_.advertise<visualization_msgs::MarkerArray>(marker_topic_, 1, true);
    path_pub_ = nh_.advertise<nav_msgs::Path>(path_topic_, 1, true);
    status_pub_ = pnh_.advertise<std_msgs::String>("status", 2, true);
    cmd_vel_pub_ = nh_.advertise<geometry_msgs::Twist>(cmd_vel_topic_, 5, false);

    odom_sub_ = nh_.subscribe(odom_topic_, 20, &PatrolStateMachineExternal::odomCb, this);

    // 外部控制接口：
    // 1) control_cmd：仅基础控制命令（START/STOP/PAUSE/RESUME/SKIP_CURRENT）
    // 2) 复杂功能全部走 service，确保调用方可拿到 success/message 反馈
    cmd_sub_ = pnh_.subscribe("control_cmd", 20, &PatrolStateMachineExternal::commandCb, this);

    // 基础控制服务接口：不带参数。
    srv_start_ = pnh_.advertiseService("start", &PatrolStateMachineExternal::startSrvCb, this);
    srv_stop_ = pnh_.advertiseService("stop", &PatrolStateMachineExternal::stopSrvCb, this);
    srv_pause_ = pnh_.advertiseService("pause", &PatrolStateMachineExternal::pauseSrvCb, this);
    srv_resume_ = pnh_.advertiseService("resume", &PatrolStateMachineExternal::resumeSrvCb, this);
    srv_skip_current_ = pnh_.advertiseService("skip_current", &PatrolStateMachineExternal::skipCurrentSrvCb, this);
    srv_clear_overlay_ = pnh_.advertiseService("clear_overlay", &PatrolStateMachineExternal::clearOverlaySrvCb, this);
    srv_reload_waypoints_ = pnh_.advertiseService("reload_waypoints", &PatrolStateMachineExternal::reloadWaypointsSrvCb, this);
    // 参数型控制服务接口：适合模式切换、动态加点等需要回执的控制。
    srv_set_mode_ = pnh_.advertiseService("set_mode", &PatrolStateMachineExternal::setModeSrvCb, this);
    srv_add_waypoint_ = pnh_.advertiseService("add_waypoint", &PatrolStateMachineExternal::addWaypointSrvCb, this);
    srv_add_waypoints_ = pnh_.advertiseService("add_waypoints", &PatrolStateMachineExternal::addWaypointsSrvCb, this);

    // ------------------ 初始化日志、Action、路线 ------------------
    initAbnormalLog();

    move_base_client_.reset(new MoveBaseClient(action_name_, true));
    waitForActionServer();

    if (!loadWaypointsFromFile(waypoint_file_)) {
      transitionTo(State::FAILED, "failed to load waypoint file");
      return;
    }

    buildBasePlan();
    logWaypointRoleSummary();
    publishWaypointsVisualization();

    // 默认行为保持与老节点一致：auto_start=true 时自动进入任务态。
    task_requested_ = auto_start_;
    if (task_requested_) {
      ROS_INFO("[PATROL_EXT] auto_start=true, mission requested.");
    } else {
      ROS_INFO("[PATROL_EXT] auto_start=false, waiting external start command.");
    }

    state_timer_ = nh_.createTimer(ros::Duration(1.0 / tick_hz_), &PatrolStateMachineExternal::tick, this);
    status_timer_ = nh_.createTimer(ros::Duration(0.5), &PatrolStateMachineExternal::publishStatusTimer, this);
    transitionTo(State::IDLE, "node initialized");
  }

 private:
  static std::string trim(const std::string& in) {
    const std::string ws = " \t\r\n";
    const size_t begin = in.find_first_not_of(ws);
    if (begin == std::string::npos) return "";
    const size_t end = in.find_last_not_of(ws);
    return in.substr(begin, end - begin + 1);
  }

  static std::string toUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](const unsigned char c) { return std::toupper(c); });
    return s;
  }

  static std::string stateToString(const State state) {
    switch (state) {
      case State::IDLE:
        return "IDLE";
      case State::NAVIGATING:
        return "NAVIGATING";
      case State::PAUSE:
        return "PAUSE";
      case State::PAUSED:
        return "PAUSED";
      case State::FAILED:
        return "FAILED";
      default:
        return "UNKNOWN";
    }
  }

  static std::string sourceToString(const GoalSource source) {
    switch (source) {
      case GoalSource::BASE_ROUTE:
        return "BASE";
      case GoalSource::OVERLAY_ONCE:
        return "OVERLAY";
      case GoalSource::PREEMPT_ONCE:
        return "PREEMPT";
      default:
        return "NONE";
    }
  }

  static bool isSuccessfulActionState(const actionlib::SimpleClientGoalState& state) {
    return state.state_ == actionlib::SimpleClientGoalState::SUCCEEDED;
  }

  static double distance2D(const geometry_msgs::PoseStamped& a, const geometry_msgs::PoseStamped& b) {
    const double dx = a.pose.position.x - b.pose.position.x;
    const double dy = a.pose.position.y - b.pose.position.y;
    return std::sqrt(dx * dx + dy * dy);
  }

  TraverseMode parseTraverseMode(const std::string& mode_str) const {
    const std::string m = toUpper(trim(mode_str));
    if (m == "LOOP") return TraverseMode::LOOP;
    if (m == "ONCE_AND_STOP") return TraverseMode::ONCE_AND_STOP;
    if (m == "ONCE_AND_RETURN") return TraverseMode::ONCE_AND_RETURN;
    if (m == "RETURN_IMMEDIATELY") return TraverseMode::RETURN_IMMEDIATELY;
    ROS_WARN_STREAM("[PATROL_EXT] unknown traverse_mode=" << mode_str << ", fallback to LOOP.");
    return TraverseMode::LOOP;
  }

  bool tryParseTraverseMode(const std::string& mode_str, TraverseMode& mode_out) const {
    const std::string m = toUpper(trim(mode_str));
    if (m == "LOOP") {
      mode_out = TraverseMode::LOOP;
      return true;
    }
    if (m == "ONCE_AND_STOP") {
      mode_out = TraverseMode::ONCE_AND_STOP;
      return true;
    }
    if (m == "ONCE_AND_RETURN") {
      mode_out = TraverseMode::ONCE_AND_RETURN;
      return true;
    }
    if (m == "RETURN_IMMEDIATELY") {
      mode_out = TraverseMode::RETURN_IMMEDIATELY;
      return true;
    }
    return false;
  }

  AddPolicy parseAddPolicy(const std::string& policy_str) const {
    const std::string p = toUpper(trim(policy_str));
    if (p == "INSERT_NEXT_ONCE") return AddPolicy::INSERT_NEXT_ONCE;
    if (p == "INSERT_NEXT_PERSISTENT") return AddPolicy::INSERT_NEXT_PERSISTENT;
    if (p == "APPEND_ONCE") return AddPolicy::APPEND_ONCE;
    if (p == "APPEND_PERSISTENT") return AddPolicy::APPEND_PERSISTENT;
    ROS_WARN_STREAM("[PATROL_EXT] unknown add policy=" << policy_str << ", fallback to INSERT_NEXT_ONCE.");
    return AddPolicy::INSERT_NEXT_ONCE;
  }

  bool tryParseAddPolicy(const std::string& policy_str, AddPolicy& policy_out) const {
    const std::string p = toUpper(trim(policy_str));
    if (p.empty()) {
      policy_out = default_add_policy_;
      return true;
    }
    if (p == "INSERT_NEXT_ONCE") {
      policy_out = AddPolicy::INSERT_NEXT_ONCE;
      return true;
    }
    if (p == "INSERT_NEXT_PERSISTENT") {
      policy_out = AddPolicy::INSERT_NEXT_PERSISTENT;
      return true;
    }
    if (p == "APPEND_ONCE") {
      policy_out = AddPolicy::APPEND_ONCE;
      return true;
    }
    if (p == "APPEND_PERSISTENT") {
      policy_out = AddPolicy::APPEND_PERSISTENT;
      return true;
    }
    return false;
  }

  std::string traverseModeName() const {
    switch (traverse_mode_) {
      case TraverseMode::LOOP:
        return "LOOP";
      case TraverseMode::ONCE_AND_STOP:
        return "ONCE_AND_STOP";
      case TraverseMode::ONCE_AND_RETURN:
        return "ONCE_AND_RETURN";
      case TraverseMode::RETURN_IMMEDIATELY:
        return "RETURN_IMMEDIATELY";
      default:
        return "UNKNOWN";
    }
  }

  bool findBasePlanCursorByBaseIndex(const int base_index, size_t& cursor_out) const {
    for (size_t i = 0; i < base_plan_indices_.size(); ++i) {
      if (base_plan_indices_[i] == base_index) {
        cursor_out = i;
        return true;
      }
    }
    return false;
  }

  size_t computeInsertNextPersistentPos() const {
    if (base_waypoints_.empty()) {
      return 0;
    }

    const bool is_running_base = (current_goal_source_ == GoalSource::BASE_ROUTE) &&
                                 ((state_ == State::NAVIGATING) || (state_ == State::PAUSE) || goal_in_flight_);
    if (is_running_base && current_base_index_ >= 0) {
      const size_t pos = static_cast<size_t>(current_base_index_) + 1;
      return std::min(pos, base_waypoints_.size());
    }

    if (!base_plan_indices_.empty() && base_cursor_ < base_plan_indices_.size()) {
      const int next_base_idx = base_plan_indices_[base_cursor_];
      if (next_base_idx <= 0) {
        // 索引0保留给原点，常规持久插入默认放在原点之后。
        return std::min(static_cast<size_t>(1), base_waypoints_.size());
      }
      const size_t pos = static_cast<size_t>(next_base_idx);
      return std::min(pos, base_waypoints_.size());
    }

    return base_waypoints_.size();
  }

  void waitForActionServer() {
    ROS_INFO_STREAM("[PATROL_EXT] waiting action server: " << action_name_);
    while (ros::ok() && !move_base_client_->waitForServer(ros::Duration(1.0))) {
      ROS_WARN_STREAM("[PATROL_EXT] waiting for action server " << action_name_ << " ...");
      appendAbnormalLog("WARN", "waiting for action server: " + action_name_);
    }
    if (ros::ok()) {
      ROS_INFO_STREAM("[PATROL_EXT] action server connected: " << action_name_);
    }
  }

  void publishZeroVelocityOnce() {
    geometry_msgs::Twist zero;
    zero.linear.x = 0.0;
    zero.linear.y = 0.0;
    zero.linear.z = 0.0;
    zero.angular.x = 0.0;
    zero.angular.y = 0.0;
    zero.angular.z = 0.0;
    cmd_vel_pub_.publish(zero);
  }

  void triggerForceStop(const std::string& reason) {
    if (!force_stop_on_pause_ || force_stop_duration_sec_ <= 1e-6) {
      return;
    }
    const ros::Time now = ros::Time::now();
    const ros::Time until = now + ros::Duration(force_stop_duration_sec_);
    if (until > force_stop_until_) {
      force_stop_until_ = until;
    }
    publishZeroVelocityOnce();
    ROS_WARN_STREAM("[PATROL_EXT] force stop active for " << force_stop_duration_sec_
                    << "s on topic " << cmd_vel_topic_ << " | reason=" << reason);
  }

  void publishForceStopIfNeeded() {
    if (!force_stop_on_pause_) {
      return;
    }
    if (ros::Time::now() <= force_stop_until_) {
      publishZeroVelocityOnce();
    }
  }

  geometry_msgs::PoseStamped makePose(const double x, const double y, const double yaw) const {
    geometry_msgs::PoseStamped p;
    p.header.frame_id = waypoint_frame_;
    p.pose.position.x = x;
    p.pose.position.y = y;
    p.pose.position.z = 0.0;
    tf2::Quaternion q;
    q.setRPY(0.0, 0.0, yaw);
    q.normalize();
    p.pose.orientation.x = q.x();
    p.pose.orientation.y = q.y();
    p.pose.orientation.z = q.z();
    p.pose.orientation.w = q.w();
    return p;
  }

  bool loadWaypointsFromFile(const std::string& file_path) {
    if (file_path.empty()) {
      ROS_ERROR("[PATROL_EXT] waypoint_file is empty.");
      appendAbnormalLog("ERROR", "waypoint_file is empty");
      return false;
    }

    const auto dot_pos = file_path.find_last_of('.');
    if (dot_pos == std::string::npos) {
      ROS_ERROR_STREAM("[PATROL_EXT] waypoint file has no extension: " << file_path);
      appendAbnormalLog("ERROR", "waypoint file has no extension: " + file_path);
      return false;
    }
    const std::string ext = file_path.substr(dot_pos + 1);

    bool ok = false;
    if (ext == "csv") {
      ok = loadWaypointsFromCsv(file_path);
    } else {
      ROS_ERROR_STREAM("[PATROL_EXT] only csv is supported now. file=" << file_path);
      appendAbnormalLog("ERROR", "unsupported waypoint file extension: " + ext);
      return false;
    }

    if (!ok || base_waypoints_.empty()) {
      ROS_ERROR_STREAM("[PATROL_EXT] loaded waypoint count invalid from file: " << file_path);
      appendAbnormalLog("ERROR", "loaded waypoint count invalid from file: " + file_path);
      return false;
    }

    ROS_INFO_STREAM("[PATROL_EXT] loaded " << base_waypoints_.size() << " base waypoints from " << file_path);
    return true;
  }

  bool loadWaypointsFromCsv(const std::string& file_path) {
    std::ifstream fin(file_path.c_str());
    if (!fin.is_open()) {
      ROS_ERROR_STREAM("[PATROL_EXT] failed to open csv: " << file_path);
      appendAbnormalLog("ERROR", "failed to open csv: " + file_path);
      return false;
    }

    base_waypoints_.clear();
    std::string line;
    int line_no = 0;
    while (std::getline(fin, line)) {
      ++line_no;
      if (line.empty() || line[0] == '#') {
        continue;
      }
      std::stringstream ss(line);
      std::string sx, sy, syaw;
      if (!std::getline(ss, sx, ',')) continue;
      if (!std::getline(ss, sy, ',')) continue;
      if (!std::getline(ss, syaw, ',')) continue;

      try {
        const double x = std::stod(sx);
        const double y = std::stod(sy);
        const double yaw = std::stod(syaw);
        base_waypoints_.push_back(makePose(x, y, yaw));
      } catch (const std::exception&) {
        ROS_WARN_STREAM("[PATROL_EXT] skip csv line " << line_no << " parse failed: " << line);
      }
    }
    return !base_waypoints_.empty();
  }

  void buildBasePlan() {
    base_plan_indices_.clear();
    base_cursor_ = 0;
    mission_finished_ = false;

    if (base_waypoints_.empty()) {
      return;
    }

    if (single_index_ >= 0) {
      if (single_index_ >= static_cast<int>(base_waypoints_.size())) {
        transitionTo(State::FAILED, "single_index out of range");
        appendAbnormalLog("ERROR", "single_index out of range");
        return;
      }
      base_plan_indices_.push_back(single_index_);
      ROS_INFO_STREAM("[PATROL_EXT] single index mode, target=" << single_index_);
      return;
    }

    // 非 single_index 模式下：
    // 索引0作为原点/返航点，不参与常规巡检。
    if (traverse_mode_ == TraverseMode::RETURN_IMMEDIATELY) {
      base_plan_indices_.push_back(0);
      ROS_INFO_STREAM("[PATROL_EXT] return immediately mode, target origin index=0");
      return;
    }

    for (size_t i = 1; i < base_waypoints_.size(); ++i) {
      base_plan_indices_.push_back(static_cast<int>(i));
    }

    if (traverse_mode_ == TraverseMode::ONCE_AND_RETURN && base_waypoints_.size() > 1) {
      // 返航模式在尾部增加一个“回到首点”目标。
      base_plan_indices_.push_back(0);
    }

    if (base_plan_indices_.empty()) {
      ROS_WARN("[PATROL_EXT] no patrol points available (csv needs at least 2 points for patrol modes).");
    }

    ROS_INFO_STREAM("[PATROL_EXT] base plan rebuilt. mode=" << traverseModeName()
                    << " plan_size=" << base_plan_indices_.size());
  }

  void logWaypointRoleSummary() const {
    if (base_waypoints_.empty()) {
      ROS_WARN("[PATROL_EXT] waypoint summary: no waypoints loaded.");
      return;
    }

    const auto& origin = base_waypoints_.front();
    ROS_INFO_STREAM("[PATROL_EXT] waypoint summary: origin(index=0) "
                    << "x=" << origin.pose.position.x
                    << " y=" << origin.pose.position.y
                    << " yaw=" << tf2::getYaw(origin.pose.orientation));

    if (base_waypoints_.size() <= 1) {
      ROS_WARN("[PATROL_EXT] waypoint summary: no patrol waypoints (need at least index>=1).");
    } else {
      ROS_INFO_STREAM("[PATROL_EXT] waypoint summary: patrol waypoint count=" << (base_waypoints_.size() - 1));
      for (size_t i = 1; i < base_waypoints_.size(); ++i) {
        const auto& p = base_waypoints_[i];
        ROS_INFO_STREAM("[PATROL_EXT] patrol_wp[index=" << i << "] "
                        << "x=" << p.pose.position.x
                        << " y=" << p.pose.position.y
                        << " yaw=" << tf2::getYaw(p.pose.orientation));
      }
    }

    std::ostringstream oss;
    oss << "[PATROL_EXT] active plan indices(mode=" << traverseModeName() << "): ";
    if (base_plan_indices_.empty()) {
      oss << "<empty>";
    } else {
      for (size_t i = 0; i < base_plan_indices_.size(); ++i) {
        if (i > 0) oss << ",";
        oss << base_plan_indices_[i];
      }
    }
    ROS_INFO_STREAM(oss.str());
  }

  void publishWaypointsVisualization() {
    visualization_msgs::MarkerArray marker_array;

    visualization_msgs::Marker points;
    points.header.frame_id = waypoint_frame_;
    points.header.stamp = ros::Time::now();
    points.ns = "patrol_ext_base_waypoints";
    points.id = 0;
    points.type = visualization_msgs::Marker::SPHERE_LIST;
    points.action = visualization_msgs::Marker::ADD;
    points.scale.x = 0.25;
    points.scale.y = 0.25;
    points.scale.z = 0.25;
    points.color.r = 0.1f;
    points.color.g = 0.9f;
    points.color.b = 0.2f;
    points.color.a = 1.0f;
    points.pose.orientation.w = 1.0;

    visualization_msgs::Marker line;
    line.header = points.header;
    line.ns = "patrol_ext_base_waypoints";
    line.id = 1;
    line.type = visualization_msgs::Marker::LINE_STRIP;
    line.action = visualization_msgs::Marker::ADD;
    line.scale.x = 0.08;
    line.color.r = 0.2f;
    line.color.g = 0.6f;
    line.color.b = 1.0f;
    line.color.a = 1.0f;
    line.pose.orientation.w = 1.0;

    for (size_t i = 0; i < base_waypoints_.size(); ++i) {
      geometry_msgs::Point pt;
      pt.x = base_waypoints_[i].pose.position.x;
      pt.y = base_waypoints_[i].pose.position.y;
      pt.z = base_waypoints_[i].pose.position.z;
      points.points.push_back(pt);
      line.points.push_back(pt);

      visualization_msgs::Marker text;
      text.header = points.header;
      text.ns = "patrol_ext_base_waypoints_text";
      text.id = static_cast<int>(1000 + i);
      text.type = visualization_msgs::Marker::TEXT_VIEW_FACING;
      text.action = visualization_msgs::Marker::ADD;
      text.scale.z = 0.25;
      text.color.r = 1.0f;
      text.color.g = 1.0f;
      text.color.b = 1.0f;
      text.color.a = 1.0f;
      text.pose.position = pt;
      text.pose.position.z += 0.35;
      text.pose.orientation.w = 1.0;
      text.text = std::to_string(i);
      marker_array.markers.push_back(text);
    }

    marker_array.markers.push_back(points);
    marker_array.markers.push_back(line);
    marker_pub_.publish(marker_array);

    nav_msgs::Path path;
    path.header.frame_id = waypoint_frame_;
    path.header.stamp = ros::Time::now();
    path.poses = base_waypoints_;
    path_pub_.publish(path);
  }

  struct CommandResult {
    bool success{false};
    std::string message;
  };

  // 话题只保留基础命令，避免“发了参数命令但无回执”这种歧义。
  void commandCb(const std_msgs::String::ConstPtr& msg) {
    const std::string cmd = toUpper(trim(msg->data));
    if (cmd == "START") {
      commandStart();
      return;
    }
    if (cmd == "STOP") {
      commandStop();
      return;
    }
    if (cmd == "PAUSE") {
      commandPause();
      return;
    }
    if (cmd == "RESUME") {
      commandResume();
      return;
    }
    if (cmd == "SKIP_CURRENT") {
      commandSkipCurrent();
      return;
    }
    ROS_WARN_STREAM("[PATROL_EXT] unsupported topic cmd='" << msg->data
                    << "'. only START/STOP/PAUSE/RESUME/SKIP_CURRENT are allowed on control_cmd. "
                    << "Use services for set_mode/add_waypoint/add_waypoints.");
  }

  void normalizePoseFrame(geometry_msgs::PoseStamped& pose) const {
    if (pose.header.frame_id.empty()) {
      pose.header.frame_id = waypoint_frame_;
    }
    pose.header.stamp = ros::Time::now();
  }

  CommandResult commandSetMode(const std::string& mode_str) {
    const std::string mode_trim = trim(mode_str);
    if (mode_trim.empty()) {
      return {false, "mode is empty"};
    }
    TraverseMode new_mode = TraverseMode::LOOP;
    if (!tryParseTraverseMode(mode_trim, new_mode)) {
      return {false, "invalid mode, allowed: LOOP/ONCE_AND_STOP/ONCE_AND_RETURN/RETURN_IMMEDIATELY"};
    }
    if (new_mode == traverse_mode_) {
      ROS_INFO_STREAM("[PATROL_EXT] traverse mode unchanged: " << traverseModeName());
      return {true, "mode unchanged: " + traverseModeName()};
    }

    traverse_mode_ = new_mode;
    buildBasePlan();

    // RETURN_IMMEDIATELY 是“立刻返航”语义：
    // 1) 自动拉起任务请求
    // 2) 若当前有执行中目标，则取消当前目标并尽快切入返航点
    if (new_mode == TraverseMode::RETURN_IMMEDIATELY) {
      mission_finished_ = false;
      task_requested_ = true;
      user_paused_ = false;
      retry_count_ = 0;

      if (goal_in_flight_) {
        move_base_client_->cancelGoal();
        triggerForceStop("mode switch return immediately");
        goal_in_flight_ = false;
        resetStuckMonitor();
        pause_reason_ = PauseReason::WAIT_CANCEL_FOR_MODE_SWITCH;
        transitionTo(State::PAUSE, "mode switch to RETURN_IMMEDIATELY");
      } else {
        transitionTo(State::IDLE, "mode switch to RETURN_IMMEDIATELY");
      }
    }

    ROS_INFO_STREAM("[PATROL_EXT] traverse mode changed to " << traverseModeName());
    return {true, "mode changed to " + traverseModeName()};
  }

  void commandStart() {
    if (state_ == State::FAILED) {
      ROS_WARN("[PATROL_EXT] cannot START in FAILED state, please fix issue then reload.");
      return;
    }
    task_requested_ = true;
    mission_finished_ = false;
    user_paused_ = false;
    if (state_ == State::PAUSED) {
      transitionTo(State::IDLE, "start from paused");
    }
    ROS_INFO("[PATROL_EXT] mission started by external command.");
  }

  void commandStop() {
    task_requested_ = false;
    user_paused_ = false;
    mission_finished_ = false;
    retry_count_ = 0;
    overlay_queue_.clear();
    preempt_queue_.clear();

    if (goal_in_flight_) {
      move_base_client_->cancelGoal();
      triggerForceStop("stop command");
      goal_in_flight_ = false;
      resetStuckMonitor();
      pause_reason_ = PauseReason::WAIT_CANCEL_FOR_STOP;
      transitionTo(State::PAUSE, "stop waiting cancel");
      return;
    }

    transitionTo(State::IDLE, "stopped");
    ROS_INFO("[PATROL_EXT] mission stopped.");
  }

  void commandPause() {
    if (state_ == State::FAILED) {
      ROS_WARN("[PATROL_EXT] cannot PAUSE in FAILED state.");
      return;
    }
    user_paused_ = true;

    if (goal_in_flight_) {
      // 暂停也需要保留一次性临时目标，避免恢复后丢任务。
      preserveInterruptedGoalIfNeeded();
      move_base_client_->cancelGoal();
      triggerForceStop("pause command");
      goal_in_flight_ = false;
      resetStuckMonitor();
      pause_reason_ = PauseReason::WAIT_CANCEL_FOR_PAUSE;
      transitionTo(State::PAUSE, "pause waiting cancel");
      return;
    }

    triggerForceStop("pause command(no goal in flight)");
    transitionTo(State::PAUSED, "paused by user");
  }

  void commandResume() {
    if (state_ == State::FAILED) {
      ROS_WARN("[PATROL_EXT] cannot RESUME in FAILED state.");
      return;
    }
    if (!task_requested_) {
      task_requested_ = true;
      mission_finished_ = false;
    }
    user_paused_ = false;
    if (state_ == State::PAUSED) {
      transitionTo(State::IDLE, "resume from paused");
    }
    ROS_INFO("[PATROL_EXT] mission resumed.");
  }

  void commandSkipCurrent() {
    if (state_ != State::NAVIGATING && state_ != State::PAUSE) {
      ROS_WARN_STREAM("[PATROL_EXT] skip ignored in state " << stateToString(state_));
      return;
    }

    // 若当前在 BASE 目标，则推进游标到下一个基础目标。
    if (current_goal_source_ == GoalSource::BASE_ROUTE) {
      if (!advanceBaseCursorAfterCurrent()) {
        mission_finished_ = true;
        task_requested_ = false;
      }
    }

    retry_count_ = 0;

    if (goal_in_flight_) {
      move_base_client_->cancelGoal();
      triggerForceStop("skip current command");
      goal_in_flight_ = false;
      resetStuckMonitor();
    }

    if (mission_finished_) {
      transitionTo(State::IDLE, "skip current and mission finished");
    } else {
      transitionTo(State::IDLE, "skip current and continue");
    }
    ROS_INFO("[PATROL_EXT] current goal skipped by command.");
  }

  CommandResult commandAddWaypoint(const geometry_msgs::PoseStamped& pose,
                                   const AddPolicy policy,
                                   const bool hard_preempt) {
    geometry_msgs::PoseStamped p = pose;
    normalizePoseFrame(p);

    if (hard_preempt) {
      // 硬抢占逻辑：先将新点压入抢占队列，再取消当前导航，待取消完成后立即执行抢占点。
      preempt_queue_.push_front(p);
      ROS_WARN_STREAM("[PATROL_EXT] HARD_PREEMPT point queued, preempt_size=" << preempt_queue_.size());

      if (goal_in_flight_) {
        preserveInterruptedGoalIfNeeded();
        move_base_client_->cancelGoal();
        goal_in_flight_ = false;
        resetStuckMonitor();
        pause_reason_ = PauseReason::WAIT_CANCEL_FOR_PREEMPT;
        transitionTo(State::PAUSE, "hard preempt waiting cancel");
      } else if (state_ != State::FAILED) {
        task_requested_ = true;
        mission_finished_ = false;
        transitionTo(State::IDLE, "hard preempt dispatch");
      }
      return {true, "hard preempt waypoint queued"};
    }

    switch (policy) {
      case AddPolicy::INSERT_NEXT_ONCE:
        overlay_queue_.push_front(p);
        ROS_INFO_STREAM("[PATROL_EXT] add waypoint policy=INSERT_NEXT_ONCE, overlay_size=" << overlay_queue_.size());
        if (state_ == State::IDLE && task_requested_ && !mission_finished_) {
          handleIdle();
        }
        return {true, "waypoint inserted as next once"};
      case AddPolicy::INSERT_NEXT_PERSISTENT: {
        if (single_index_ >= 0) {
          return {false, "INSERT_NEXT_PERSISTENT is not supported in single_index mode"};
        }

        const int old_current_base_index = current_base_index_;
        const bool running_base = (current_goal_source_ == GoalSource::BASE_ROUTE) &&
                                  ((state_ == State::NAVIGATING) || (state_ == State::PAUSE) || goal_in_flight_);

        const size_t insert_pos = computeInsertNextPersistentPos();
        base_waypoints_.insert(base_waypoints_.begin() + static_cast<std::ptrdiff_t>(insert_pos), p);
        buildBasePlan();
        publishWaypointsVisualization();

        size_t cursor_pos = 0;
        if (running_base && old_current_base_index >= 0 && findBasePlanCursorByBaseIndex(old_current_base_index, cursor_pos)) {
          // 保持游标指向“当前执行中的基础点”，成功后自然推进到新插入点。
          base_cursor_ = cursor_pos;
        } else if (findBasePlanCursorByBaseIndex(static_cast<int>(insert_pos), cursor_pos)) {
          // 非基础点执行阶段，尽量将新点设为“下一基础目标”。
          base_cursor_ = cursor_pos;
        }

        if (state_ == State::IDLE && task_requested_ && !mission_finished_) {
          handleIdle();
        }
        return {true, "waypoint inserted as next persistent"};
      }
      case AddPolicy::APPEND_ONCE:
        overlay_queue_.push_back(p);
        ROS_INFO_STREAM("[PATROL_EXT] add waypoint policy=APPEND_ONCE, overlay_size=" << overlay_queue_.size());
        if (state_ == State::IDLE && task_requested_ && !mission_finished_) {
          handleIdle();
        }
        return {true, "waypoint appended once"};
      case AddPolicy::APPEND_PERSISTENT:
        base_waypoints_.push_back(p);
        buildBasePlan();
        publishWaypointsVisualization();
        ROS_INFO_STREAM("[PATROL_EXT] add waypoint policy=APPEND_PERSISTENT, base_size=" << base_waypoints_.size());
        return {true, "waypoint appended persistent"};
      default:
        return {false, "unknown add policy"};
    }
  }

  CommandResult commandAddWaypoints(const std::vector<geometry_msgs::PoseStamped>& poses,
                                    const AddPolicy policy,
                                    const bool hard_preempt_first,
                                    uint32_t& accepted_count) {
    accepted_count = 0;
    if (poses.empty()) {
      return {false, "poses is empty"};
    }

    if (hard_preempt_first) {
      CommandResult ret = commandAddWaypoint(poses.front(), policy, true);
      if (!ret.success) {
        return ret;
      }
      accepted_count = 1;
      for (size_t i = 1; i < poses.size(); ++i) {
        CommandResult r = commandAddWaypoint(poses[i], policy, false);
        if (!r.success) {
          return {false, "partial success: accepted=" + std::to_string(accepted_count) + ", error=" + r.message};
        }
        ++accepted_count;
      }
      return {true, "accepted with hard preempt first"};
    }

    for (const auto& p : poses) {
      CommandResult r = commandAddWaypoint(p, policy, false);
      if (!r.success) {
        return {false, "partial success: accepted=" + std::to_string(accepted_count) + ", error=" + r.message};
      }
      ++accepted_count;
    }
    return {true, "all waypoints accepted"};
  }

  CommandResult commandReloadWaypoints() {
    if (!loadWaypointsFromFile(waypoint_file_)) {
      transitionTo(State::FAILED, "reload waypoint failed");
      return {false, "reload waypoint failed"};
    }

    overlay_queue_.clear();
    preempt_queue_.clear();
    buildBasePlan();
    publishWaypointsVisualization();
    retry_count_ = 0;
    mission_finished_ = false;

    if (goal_in_flight_) {
      move_base_client_->cancelGoal();
      goal_in_flight_ = false;
    }
    transitionTo(State::IDLE, "waypoints reloaded");
    ROS_INFO("[PATROL_EXT] waypoints reloaded from file.");
    return {true, "waypoints reloaded"};
  }

  // 当前正在跑 overlay/preempt 目标时，若触发硬抢占，则将被打断目标重新塞回队头。
  // 这样抢占任务完成后，能继续之前被中断的临时任务。
  void preserveInterruptedGoalIfNeeded() {
    if (current_goal_source_ == GoalSource::OVERLAY_ONCE) {
      overlay_queue_.push_front(current_goal_pose_);
      ROS_INFO("[PATROL_EXT] interrupted overlay goal preserved to front.");
    } else if (current_goal_source_ == GoalSource::PREEMPT_ONCE) {
      preempt_queue_.push_front(current_goal_pose_);
      ROS_INFO("[PATROL_EXT] interrupted preempt goal preserved to front.");
    }
  }

  bool startSrvCb(std_srvs::Trigger::Request&, std_srvs::Trigger::Response& res) {
    commandStart();
    res.success = true;
    res.message = "started";
    return true;
  }

  bool stopSrvCb(std_srvs::Trigger::Request&, std_srvs::Trigger::Response& res) {
    commandStop();
    res.success = true;
    res.message = "stopped";
    return true;
  }

  bool pauseSrvCb(std_srvs::Trigger::Request&, std_srvs::Trigger::Response& res) {
    commandPause();
    res.success = true;
    res.message = "paused";
    return true;
  }

  bool resumeSrvCb(std_srvs::Trigger::Request&, std_srvs::Trigger::Response& res) {
    commandResume();
    res.success = true;
    res.message = "resumed";
    return true;
  }

  bool skipCurrentSrvCb(std_srvs::Trigger::Request&, std_srvs::Trigger::Response& res) {
    commandSkipCurrent();
    res.success = true;
    res.message = "skip requested";
    return true;
  }

  bool clearOverlaySrvCb(std_srvs::Trigger::Request&, std_srvs::Trigger::Response& res) {
    overlay_queue_.clear();
    preempt_queue_.clear();
    res.success = true;
    res.message = "overlay/preempt queue cleared";
    return true;
  }

  bool reloadWaypointsSrvCb(std_srvs::Trigger::Request&, std_srvs::Trigger::Response& res) {
    const CommandResult ret = commandReloadWaypoints();
    res.success = ret.success;
    res.message = ret.message;
    return true;
  }

  bool setModeSrvCb(xjrobot_task::PatrolSetMode::Request& req, xjrobot_task::PatrolSetMode::Response& res) {
    const CommandResult ret = commandSetMode(req.mode);
    res.success = ret.success;
    res.message = ret.message;
    return true;
  }

  bool addWaypointSrvCb(xjrobot_task::PatrolAddWaypoint::Request& req,
                        xjrobot_task::PatrolAddWaypoint::Response& res) {
    AddPolicy policy = default_add_policy_;
    if (!tryParseAddPolicy(req.policy, policy)) {
      res.success = false;
      res.message =
          "invalid policy, allowed: INSERT_NEXT_ONCE/INSERT_NEXT_PERSISTENT/APPEND_ONCE/APPEND_PERSISTENT";
      return true;
    }
    const CommandResult ret = commandAddWaypoint(req.pose, policy, req.hard_preempt);
    res.success = ret.success;
    res.message = ret.message;
    return true;
  }

  bool addWaypointsSrvCb(xjrobot_task::PatrolAddWaypoints::Request& req,
                         xjrobot_task::PatrolAddWaypoints::Response& res) {
    AddPolicy policy = default_add_policy_;
    if (!tryParseAddPolicy(req.policy, policy)) {
      res.success = false;
      res.message =
          "invalid policy, allowed: INSERT_NEXT_ONCE/INSERT_NEXT_PERSISTENT/APPEND_ONCE/APPEND_PERSISTENT";
      res.accepted = 0;
      return true;
    }

    uint32_t accepted = 0;
    const CommandResult ret = commandAddWaypoints(req.poses, policy, req.hard_preempt_first, accepted);
    res.success = ret.success;
    res.message = ret.message;
    res.accepted = accepted;
    return true;
  }

  void tick(const ros::TimerEvent&) {
    // 安全刹停发布：在窗口期内持续发 0 速度，降低暂停后残余运动风险。
    publishForceStopIfNeeded();

    switch (state_) {
      case State::IDLE:
        handleIdle();
        break;
      case State::NAVIGATING:
        handleNavigating();
        break;
      case State::PAUSE:
        handlePause();
        break;
      case State::PAUSED:
        handlePaused();
        break;
      case State::FAILED:
        handleFailed();
        break;
      default:
        break;
    }
  }

  void handleIdle() {
    if (user_paused_) {
      transitionTo(State::PAUSED, "user paused");
      return;
    }
    if (!task_requested_) {
      ROS_INFO_THROTTLE(5.0, "[PATROL_EXT] IDLE: waiting task request.");
      return;
    }
    if (mission_finished_) {
      ROS_INFO_THROTTLE(5.0, "[PATROL_EXT] IDLE: mission finished.");
      return;
    }

    if (!selectAndSendNextGoal()) {
      mission_finished_ = true;
      task_requested_ = false;
      transitionTo(State::IDLE, "no next goal");
      return;
    }

    transitionTo(State::NAVIGATING, "goal dispatched");
  }

  bool selectAndSendNextGoal() {
    if (!preempt_queue_.empty()) {
      current_goal_source_ = GoalSource::PREEMPT_ONCE;
      current_goal_pose_ = preempt_queue_.front();
      preempt_queue_.pop_front();
      current_base_index_ = -1;
      sendCurrentGoal();
      return true;
    }

    if (!overlay_queue_.empty()) {
      current_goal_source_ = GoalSource::OVERLAY_ONCE;
      current_goal_pose_ = overlay_queue_.front();
      overlay_queue_.pop_front();
      current_base_index_ = -1;
      sendCurrentGoal();
      return true;
    }

    if (base_plan_indices_.empty()) {
      ROS_WARN("[PATROL_EXT] base plan is empty.");
      return false;
    }

    if (traverse_mode_ == TraverseMode::LOOP) {
      if (base_cursor_ >= base_plan_indices_.size()) {
        base_cursor_ = 0;
      }
    } else {
      if (base_cursor_ >= base_plan_indices_.size()) {
        return false;
      }
    }

    current_goal_source_ = GoalSource::BASE_ROUTE;
    current_base_index_ = base_plan_indices_[base_cursor_];
    current_goal_pose_ = base_waypoints_[current_base_index_];
    sendCurrentGoal();
    return true;
  }

  void sendCurrentGoal() {
    current_goal_pose_.header.stamp = ros::Time::now();

    geometry_msgs::PoseStamped start_pose;
    if (!has_last_waypoint_) {
      if (getRobotPose(start_pose)) {
        current_segment_start_pose_ = start_pose;
      } else {
        current_segment_start_pose_ = current_goal_pose_;
      }
    } else {
      current_segment_start_pose_ = last_completed_pose_;
    }
    current_segment_length_ = std::max(1e-6, distance2D(current_segment_start_pose_, current_goal_pose_));

    mbf_msgs::MoveBaseGoal goal;
    goal.target_pose = current_goal_pose_;

    goal_in_flight_ = true;
    nav_result_ready_ = false;
    resetStuckMonitor();

    move_base_client_->sendGoal(goal,
                                boost::bind(&PatrolStateMachineExternal::doneCb, this, _1, _2),
                                boost::bind(&PatrolStateMachineExternal::activeCb, this),
                                boost::bind(&PatrolStateMachineExternal::feedbackCb, this, _1));

    ROS_INFO_STREAM("[PATROL_EXT] send goal source=" << sourceToString(current_goal_source_)
                    << " base_idx=" << current_base_index_
                    << " x=" << current_goal_pose_.pose.position.x
                    << " y=" << current_goal_pose_.pose.position.y
                    << " yaw=" << tf2::getYaw(current_goal_pose_.pose.orientation));
  }

  void handleNavigating() {
    printProgress();

    if (!nav_result_ready_) {
      if (enable_stuck_replan_) {
        checkAndHandleStuck();
      }
      return;
    }

    nav_result_ready_ = false;

    if (isSuccessfulActionState(last_terminal_state_)) {
      handleGoalSuccess();
      return;
    }

    // 非成功结果统一进入 PAUSE 走重试/跳过逻辑。
    transitionTo(State::PAUSE, "goal failed");
    pause_reason_ = PauseReason::NAV_FAILURE_RETRY;
  }

  void handleGoalSuccess() {
    retry_count_ = 0;
    has_last_waypoint_ = true;
    last_completed_pose_ = current_goal_pose_;

    // 只有 BASE 目标成功时才推进基础路线游标。
    if (current_goal_source_ == GoalSource::BASE_ROUTE) {
      if (!advanceBaseCursorAfterCurrent()) {
        if (traverse_mode_ == TraverseMode::LOOP) {
          // LOOP 模式理论不会进这里，作为防御保底。
          base_cursor_ = 0;
        } else {
          mission_finished_ = true;
          task_requested_ = false;
        }
      }
    }

    if (mission_finished_) {
      transitionTo(State::IDLE, "mission completed");
      return;
    }

    transitionTo(State::IDLE, "goal completed, dispatch next");
  }

  bool advanceBaseCursorAfterCurrent() {
    if (traverse_mode_ == TraverseMode::LOOP) {
      ++base_cursor_;
      if (base_cursor_ >= base_plan_indices_.size()) {
        base_cursor_ = 0;
        ++loop_count_;
        ROS_INFO_STREAM("[PATROL_EXT] loop cycle completed. loop_count=" << loop_count_);
      }
      return true;
    }

    ++base_cursor_;
    if (base_cursor_ >= base_plan_indices_.size()) {
      return false;
    }
    return true;
  }

  void handlePause() {
    // PAUSE 状态承载两类逻辑：
    // 1) 导航失败后的重试/跳过
    // 2) 控制命令触发 cancel 后等待切换
    if (pause_reason_ == PauseReason::WAIT_CANCEL_FOR_PAUSE) {
      if (goal_in_flight_) {
        return;
      }
      pause_reason_ = PauseReason::NONE;
      transitionTo(State::PAUSED, "pause complete");
      return;
    }

    if (pause_reason_ == PauseReason::WAIT_CANCEL_FOR_STOP) {
      if (goal_in_flight_) {
        return;
      }
      pause_reason_ = PauseReason::NONE;
      transitionTo(State::IDLE, "stop complete");
      return;
    }

    if (pause_reason_ == PauseReason::WAIT_CANCEL_FOR_PREEMPT) {
      if (goal_in_flight_) {
        return;
      }
      pause_reason_ = PauseReason::NONE;
      mission_finished_ = false;
      task_requested_ = true;
      transitionTo(State::IDLE, "preempt cancel complete");
      return;
    }

    if (pause_reason_ == PauseReason::WAIT_CANCEL_FOR_MODE_SWITCH) {
      if (goal_in_flight_) {
        return;
      }
      pause_reason_ = PauseReason::NONE;
      mission_finished_ = false;
      task_requested_ = true;
      transitionTo(State::IDLE, "mode switch cancel complete");
      return;
    }

    if (pause_reason_ != PauseReason::NAV_FAILURE_RETRY) {
      transitionTo(State::IDLE, "pause without known reason");
      return;
    }

    if (retry_count_ >= retry_max_) {
      ROS_ERROR_STREAM("[PATROL_EXT] goal failed after max retries=" << retry_max_
                       << " source=" << sourceToString(current_goal_source_)
                       << " base_idx=" << current_base_index_);
      appendAbnormalLog("ERROR", "goal failed after max retries");

      // 超过重试上限：
      // BASE 目标 -> 跳到下一基础点
      // 临时目标 -> 直接丢弃当前失败点
      if (current_goal_source_ == GoalSource::BASE_ROUTE) {
        if (!advanceBaseCursorAfterCurrent()) {
          mission_finished_ = true;
          task_requested_ = false;
        }
      }

      retry_count_ = 0;
      pause_reason_ = PauseReason::NONE;

      if (mission_finished_) {
        transitionTo(State::IDLE, "mission ended after max retry skip");
      } else {
        transitionTo(State::IDLE, "skip failed goal after max retries");
      }
      return;
    }

    ++retry_count_;
    ROS_WARN_STREAM("[PATROL_EXT] retry " << retry_count_ << "/" << retry_max_
                    << " for source=" << sourceToString(current_goal_source_)
                    << " base_idx=" << current_base_index_);

    // 重新发送同一个目标。
    sendCurrentGoal();
    transitionTo(State::NAVIGATING, "retry dispatched");
  }

  void handlePaused() {
    ROS_INFO_THROTTLE(3.0, "[PATROL_EXT] PAUSED: waiting RESUME.");
    if (!user_paused_) {
      transitionTo(State::IDLE, "resume flag observed");
    }
  }

  void handleFailed() {
    if (!failure_logged_) {
      failure_logged_ = true;
      if (goal_in_flight_) {
        move_base_client_->cancelAllGoals();
      }
      appendAbnormalLog("ERROR", "entered FAILED state");
      ROS_ERROR("[PATROL_EXT] entered FAILED state, waiting external intervention.");
    }
    ROS_ERROR_THROTTLE(5.0, "[PATROL_EXT] FAILED: no further action.");
  }

  void transitionTo(const State new_state, const std::string& reason) {
    if (state_ == new_state) {
      return;
    }
    ROS_INFO_STREAM("[PATROL_EXT] state transition: " << stateToString(state_)
                    << " -> " << stateToString(new_state)
                    << " | reason=" << reason);
    if (new_state == State::FAILED) {
      appendAbnormalLog("ERROR", "state transition to FAILED, reason=" + reason);
    }
    state_ = new_state;
  }

  void activeCb() {
    ROS_INFO_STREAM("[PATROL_EXT] action active source=" << sourceToString(current_goal_source_)
                    << " base_idx=" << current_base_index_);
  }

  void feedbackCb(const mbf_msgs::MoveBaseFeedbackConstPtr& /*feedback*/) {}

  void doneCb(const actionlib::SimpleClientGoalState& state,
              const mbf_msgs::MoveBaseResultConstPtr& result) {
    goal_in_flight_ = false;
    last_terminal_state_ = state;
    nav_result_ready_ = true;

    const int outcome = result ? result->outcome : -1;
    const std::string message = result ? result->message : "null result";
    ROS_INFO_STREAM("[PATROL_EXT] action done source=" << sourceToString(current_goal_source_)
                    << " base_idx=" << current_base_index_
                    << " state=" << state.toString()
                    << " outcome=" << outcome
                    << " msg=" << message);

    if (!isSuccessfulActionState(state)) {
      appendAbnormalLog("WARN", "action done non-success, state=" + state.toString() +
                                    " outcome=" + std::to_string(outcome) +
                                    " msg=" + message);
    }
  }

  bool getRobotPose(geometry_msgs::PoseStamped& pose_out) {
    try {
      const geometry_msgs::TransformStamped tf_stamped =
          tf_buffer_.lookupTransform(waypoint_frame_, base_frame_, ros::Time(0), ros::Duration(0.1));
      pose_out.header = tf_stamped.header;
      pose_out.pose.position.x = tf_stamped.transform.translation.x;
      pose_out.pose.position.y = tf_stamped.transform.translation.y;
      pose_out.pose.position.z = tf_stamped.transform.translation.z;
      pose_out.pose.orientation = tf_stamped.transform.rotation;
      return true;
    } catch (const tf2::TransformException& ex) {
      ROS_WARN_THROTTLE(2.0, "[PATROL_EXT] TF lookup failed: %s", ex.what());
      return false;
    }
  }

  void odomCb(const nav_msgs::Odometry::ConstPtr& msg) {
    latest_odom_ = *msg;
    has_odom_ = true;
  }

  void resetStuckMonitor() {
    low_speed_started_ = false;
    low_speed_start_time_ = ros::Time(0);
  }

  void checkAndHandleStuck() {
    if (!goal_in_flight_) {
      return;
    }
    if (!has_odom_) {
      ROS_WARN_THROTTLE(2.0, "[PATROL_EXT] stuck monitor waiting odom on topic: %s", odom_topic_.c_str());
      return;
    }

    const auto& tw = latest_odom_.twist.twist;
    const double linear_speed = std::hypot(tw.linear.x, tw.linear.y);
    const double angular_speed = std::fabs(tw.angular.z);
    const bool low_speed = (linear_speed <= stuck_linear_vel_eps_) &&
                           (angular_speed <= stuck_angular_vel_eps_);

    if (!low_speed) {
      resetStuckMonitor();
      return;
    }

    const ros::Time now = ros::Time::now();
    if (!low_speed_started_) {
      low_speed_started_ = true;
      low_speed_start_time_ = now;
      return;
    }

    const double low_speed_duration = (now - low_speed_start_time_).toSec();
    if (low_speed_duration < stuck_timeout_sec_) {
      ROS_WARN_THROTTLE(1.0,
                        "[PATROL_EXT] low speed %.2fs/%.2fs (v=%.3f, wz=%.3f)",
                        low_speed_duration,
                        stuck_timeout_sec_,
                        linear_speed,
                        angular_speed);
      return;
    }

    ROS_WARN("[PATROL_EXT] robot appears stuck, cancel current goal and retry.");
    appendAbnormalLog("WARN", "stuck detected, cancel current goal and retry");
    move_base_client_->cancelGoal();
    goal_in_flight_ = false;
    resetStuckMonitor();
    pause_reason_ = PauseReason::NAV_FAILURE_RETRY;
    transitionTo(State::PAUSE, "stuck detected");
  }

  void printProgress() {
    const ros::Time now = ros::Time::now();
    if ((now - last_progress_log_time_).toSec() < progress_log_period_) {
      return;
    }
    last_progress_log_time_ = now;

    geometry_msgs::PoseStamped robot_pose;
    if (!getRobotPose(robot_pose)) {
      return;
    }

    const double traveled = std::min(distance2D(current_segment_start_pose_, robot_pose), current_segment_length_);
    const double pct = std::max(0.0, std::min(100.0, 100.0 * traveled / std::max(1e-6, current_segment_length_)));

    ROS_INFO_STREAM("[PATROL_EXT] progress(seg)=" << pct << "% "
                    << "state=" << stateToString(state_)
                    << " src=" << sourceToString(current_goal_source_)
                    << " base_idx=" << current_base_index_
                    << " retry=" << retry_count_
                    << " overlay=" << overlay_queue_.size()
                    << " preempt=" << preempt_queue_.size());
  }

  void publishStatusTimer(const ros::TimerEvent&) {
    std::ostringstream oss;
    oss << "state=" << stateToString(state_)
        << ";mode=" << traverseModeName()
        << ";task_requested=" << (task_requested_ ? "1" : "0")
        << ";user_paused=" << (user_paused_ ? "1" : "0")
        << ";mission_finished=" << (mission_finished_ ? "1" : "0")
        << ";goal_in_flight=" << (goal_in_flight_ ? "1" : "0")
        << ";current_source=" << sourceToString(current_goal_source_)
        << ";current_base_index=" << current_base_index_
        << ";base_size=" << base_waypoints_.size()
        << ";base_cursor=" << base_cursor_
        << ";overlay_size=" << overlay_queue_.size()
        << ";preempt_size=" << preempt_queue_.size()
        << ";retry=" << retry_count_
        << ";loop_count=" << loop_count_;

    std_msgs::String msg;
    msg.data = oss.str();
    status_pub_.publish(msg);
  }

  std::string nowString() const {
    const std::time_t t = std::time(nullptr);
    std::tm tm_buf{};
    localtime_r(&t, &tm_buf);
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
    return oss.str();
  }

  void initAbnormalLog() {
    abnormal_log_stream_.open(abnormal_log_file_, std::ios::out | std::ios::app);
    if (!abnormal_log_stream_.is_open()) {
      ROS_ERROR_STREAM("[PATROL_EXT] failed to open abnormal log file: " << abnormal_log_file_);
      return;
    }
    if (!run_start_logged_) {
      abnormal_log_stream_ << "\n===== Patrol EXT Run Start: " << nowString() << " =====\n";
      abnormal_log_stream_.flush();
      run_start_logged_ = true;
    }
  }

  void appendAbnormalLog(const std::string& level, const std::string& text) {
    if (!abnormal_log_stream_.is_open()) {
      return;
    }
    abnormal_log_stream_ << "[" << nowString() << "]"
                         << " [" << level << "] " << text << "\n";
    abnormal_log_stream_.flush();
  }

 private:
  ros::NodeHandle nh_;
  ros::NodeHandle pnh_;

  tf2_ros::Buffer tf_buffer_;
  tf2_ros::TransformListener tf_listener_;

  std::unique_ptr<MoveBaseClient> move_base_client_;

  ros::Timer state_timer_;
  ros::Timer status_timer_;
  ros::Publisher marker_pub_;
  ros::Publisher path_pub_;
  ros::Publisher status_pub_;
  ros::Publisher cmd_vel_pub_;
  ros::Subscriber odom_sub_;
  ros::Subscriber cmd_sub_;

  ros::ServiceServer srv_start_;
  ros::ServiceServer srv_stop_;
  ros::ServiceServer srv_pause_;
  ros::ServiceServer srv_resume_;
  ros::ServiceServer srv_skip_current_;
  ros::ServiceServer srv_clear_overlay_;
  ros::ServiceServer srv_reload_waypoints_;
  ros::ServiceServer srv_set_mode_;
  ros::ServiceServer srv_add_waypoint_;
  ros::ServiceServer srv_add_waypoints_;

  State state_{State::IDLE};
  TraverseMode traverse_mode_{TraverseMode::LOOP};
  PauseReason pause_reason_{PauseReason::NONE};
  AddPolicy default_add_policy_{AddPolicy::INSERT_NEXT_ONCE};

  std::string action_name_;
  std::string waypoint_file_;
  std::string waypoint_frame_;
  std::string base_frame_;
  std::string marker_topic_;
  std::string path_topic_;
  std::string odom_topic_;
  std::string cmd_vel_topic_;
  std::string abnormal_log_file_;

  int retry_max_{5};
  int retry_count_{0};
  int single_index_{-1};
  bool auto_start_{true};
  bool task_requested_{false};
  bool user_paused_{false};
  bool mission_finished_{false};
  bool failure_logged_{false};
  bool goal_in_flight_{false};
  bool nav_result_ready_{false};
  bool enable_stuck_replan_{true};
  bool force_stop_on_pause_{true};
  bool has_odom_{false};
  bool low_speed_started_{false};
  bool run_start_logged_{false};
  bool has_last_waypoint_{false};

  double tick_hz_{10.0};
  double progress_log_period_{1.0};
  double stuck_timeout_sec_{5.0};
  double stuck_linear_vel_eps_{0.02};
  double stuck_angular_vel_eps_{0.05};
  double force_stop_duration_sec_{1.0};

  int loop_count_{0};
  int current_base_index_{-1};
  size_t base_cursor_{0};

  double current_segment_length_{1.0};
  ros::Time last_progress_log_time_;
  ros::Time low_speed_start_time_;
  ros::Time force_stop_until_;

  geometry_msgs::PoseStamped current_goal_pose_;
  geometry_msgs::PoseStamped current_segment_start_pose_;
  geometry_msgs::PoseStamped last_completed_pose_;
  nav_msgs::Odometry latest_odom_;
  std::ofstream abnormal_log_stream_;
  actionlib::SimpleClientGoalState last_terminal_state_{actionlib::SimpleClientGoalState::LOST};

  std::vector<geometry_msgs::PoseStamped> base_waypoints_;
  std::vector<int> base_plan_indices_;
  std::deque<geometry_msgs::PoseStamped> overlay_queue_;
  std::deque<geometry_msgs::PoseStamped> preempt_queue_;

  GoalSource current_goal_source_{GoalSource::NONE};
};

int main(int argc, char** argv) {
  ros::init(argc, argv, "patrol_state_machine_ext_node");
  PatrolStateMachineExternal node;
  ros::spin();
  return 0;
}

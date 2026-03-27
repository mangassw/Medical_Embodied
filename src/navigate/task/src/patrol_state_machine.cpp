#include <algorithm>
#include <cmath>
#include <ctime>
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
#include <mbf_msgs/MoveBaseAction.h>
#include <nav_msgs/Odometry.h>
#include <nav_msgs/Path.h>
#include <ros/ros.h>
#include <tf2/LinearMath/Quaternion.h>
#include <tf2/utils.h>
#include <tf2_ros/buffer.h>
#include <tf2_ros/transform_listener.h>
#include <visualization_msgs/Marker.h>
#include <visualization_msgs/MarkerArray.h>

class PatrolStateMachine {
 public:
  // 状态机主状态：
  // IDLE       等待任务触发
  // NAVIGATING 正在导航到当前目标点
  // PAUSE      当前目标失败后的重试/跳过决策状态
  // COMPLETED  当前点完成后的流转状态（决定下一个点或结束）
  // FAILED     任务不可恢复失败，等待外部介入
  enum class State {
    IDLE = 0,
    NAVIGATING = 1,
    PAUSE = 2,
    COMPLETED = 3,
    FAILED = 4
  };

  // 巡检遍历模式：
  // LOOP            循环执行，完成最后一点后回到第一点继续
  // ONCE_AND_STOP   执行一轮后停止在最后一点
  // ONCE_AND_RETURN 执行一轮后返回第一个点并结束
  enum class TraverseMode {
    LOOP = 0,
    ONCE_AND_STOP = 1,
    ONCE_AND_RETURN = 2
  };

  using MoveBaseClient = actionlib::SimpleActionClient<mbf_msgs::MoveBaseAction>;

  PatrolStateMachine()
      : pnh_("~"),
        tf_buffer_(),
        tf_listener_(tf_buffer_) {
    // ------------------ 参数读取 ------------------
    // 基础导航与坐标参数
    action_name_ = pnh_.param<std::string>("action_name", "/move_base_flex/move_base");
    waypoint_file_ = pnh_.param<std::string>("waypoint_file", "");
    waypoint_frame_ = pnh_.param<std::string>("waypoint_frame", "map");
    base_frame_ = pnh_.param<std::string>("base_frame", "base_link");
    marker_topic_ = pnh_.param<std::string>("marker_topic", "patrol_waypoints_markers");
    path_topic_ = pnh_.param<std::string>("path_topic", "patrol_waypoints_path");
    odom_topic_ = pnh_.param<std::string>("odom_topic", "/odom");
    // 异常日志文件（追加写，不覆盖）
    abnormal_log_file_ = pnh_.param<std::string>("abnormal_log_file", "/tmp/patrol_state_machine.log");
    // 状态机运行参数
    retry_max_ = std::max(1, pnh_.param("retry_max", 5));
    single_index_ = pnh_.param("single_index", -1);
    auto_start_ = pnh_.param("auto_start", true);
    tick_hz_ = std::max(1.0, pnh_.param("tick_hz", 10.0));
    progress_log_period_ = std::max(0.2, pnh_.param("progress_log_period", 1.0));
    // 卡住检测参数（低速持续超时触发重规划）
    enable_stuck_replan_ = pnh_.param("enable_stuck_replan", true);
    stuck_timeout_sec_ = std::max(0.5, pnh_.param("stuck_timeout_sec", 5.0));
    stuck_linear_vel_eps_ = std::max(0.0, pnh_.param("stuck_linear_vel_eps", 0.02));
    stuck_angular_vel_eps_ = std::max(0.0, pnh_.param("stuck_angular_vel_eps", 0.05));

    const std::string mode_str = pnh_.param<std::string>("traverse_mode", "LOOP");
    traverse_mode_ = parseTraverseMode(mode_str);

    // ------------------ 通信对象初始化 ------------------
    // 路点可视化发布（RViz）
    marker_pub_ = nh_.advertise<visualization_msgs::MarkerArray>(marker_topic_, 1, true);
    path_pub_ = nh_.advertise<nav_msgs::Path>(path_topic_, 1, true);
    // 订阅里程计用于“卡住”检测
    odom_sub_ = nh_.subscribe(odom_topic_, 20, &PatrolStateMachine::odomCb, this);
    // 初始化异常日志（写入本次运行起始时间）
    initAbnormalLog();

    // ------------------ MBF Action Client 初始化 ------------------
    move_base_client_.reset(new MoveBaseClient(action_name_, true));
    waitForActionServer();

    // ------------------ 加载路点并构建任务 ------------------
    if (!loadWaypointsFromFile(waypoint_file_)) {
      transitionTo(State::FAILED, "failed to load waypoint file");
      return;
    }

    publishWaypointsVisualization();
    buildMissionPlan();

    // 自动启动：启动后立刻进入巡检；否则停在 IDLE 等待外部触发
    if (auto_start_) {
      task_requested_ = true;
      ROS_INFO("[PATROL] auto_start=true, task requested.");
    } else {
      ROS_INFO("[PATROL] auto_start=false, waiting external trigger (not implemented in this node).");
    }

    // 周期驱动状态机 tick
    state_timer_ = nh_.createTimer(ros::Duration(1.0 / tick_hz_), &PatrolStateMachine::tick, this);
    transitionTo(State::IDLE, "node initialized");
  }

 private:
  static std::string stateToString(const State state) {
    switch (state) {
      case State::IDLE:
        return "IDLE";
      case State::NAVIGATING:
        return "NAVIGATING";
      case State::PAUSE:
        return "PAUSE";
      case State::COMPLETED:
        return "COMPLETED";
      case State::FAILED:
        return "FAILED";
      default:
        return "UNKNOWN";
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
    if (mode_str == "LOOP") return TraverseMode::LOOP;
    if (mode_str == "ONCE_AND_STOP") return TraverseMode::ONCE_AND_STOP;
    if (mode_str == "ONCE_AND_RETURN") return TraverseMode::ONCE_AND_RETURN;
    ROS_WARN_STREAM("[PATROL] unknown traverse_mode=" << mode_str << ", fallback to LOOP.");
    return TraverseMode::LOOP;
  }

  void waitForActionServer() {
    ROS_INFO_STREAM("[PATROL] waiting action server: " << action_name_);
    while (ros::ok() && !move_base_client_->waitForServer(ros::Duration(1.0))) {
      ROS_WARN_STREAM("[PATROL] waiting for action server " << action_name_ << " ...");
      appendAbnormalLog("WARN", "waiting for action server: " + action_name_);
    }
    if (ros::ok()) {
      ROS_INFO_STREAM("[PATROL] action server connected: " << action_name_);
    }
  }

  bool loadWaypointsFromFile(const std::string& file_path) {
    if (file_path.empty()) {
      ROS_ERROR("[PATROL] waypoint_file is empty.");
      appendAbnormalLog("ERROR", "waypoint_file is empty");
      return false;
    }

    const auto dot_pos = file_path.find_last_of('.');
    if (dot_pos == std::string::npos) {
      ROS_ERROR_STREAM("[PATROL] waypoint file has no extension: " << file_path);
      appendAbnormalLog("ERROR", "waypoint file has no extension: " + file_path);
      return false;
    }
    const std::string ext = file_path.substr(dot_pos + 1);

    bool ok = false;
    if (ext == "csv") {
      ok = loadWaypointsFromCsv(file_path);
    } else if (ext == "yaml" || ext == "yml") {
      ok = loadWaypointsFromYamlPlaceholder(file_path);
    } else {
      ROS_ERROR_STREAM("[PATROL] unsupported waypoint file extension: " << ext);
      appendAbnormalLog("ERROR", "unsupported waypoint file extension: " + ext);
      return false;
    }

    if (!ok || waypoints_.empty()) {
      ROS_ERROR_STREAM("[PATROL] loaded waypoint count invalid from file: " << file_path);
      appendAbnormalLog("ERROR", "loaded waypoint count invalid from file: " + file_path);
      return false;
    }

    ROS_INFO_STREAM("[PATROL] loaded " << waypoints_.size() << " waypoints from " << file_path);
    return true;
  }

  bool loadWaypointsFromCsv(const std::string& file_path) {
    std::ifstream fin(file_path.c_str());
    if (!fin.is_open()) {
      ROS_ERROR_STREAM("[PATROL] failed to open csv: " << file_path);
      appendAbnormalLog("ERROR", "failed to open csv: " + file_path);
      return false;
    }

    waypoints_.clear();
    std::string line;
    int line_no = 0;
    while (std::getline(fin, line)) {
      ++line_no;
      if (line.empty()) {
        continue;
      }
      if (line[0] == '#') {
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
        waypoints_.push_back(makePose(x, y, yaw));
      } catch (const std::exception&) {
        ROS_WARN_STREAM("[PATROL] skip csv line " << line_no << " (parse failed): " << line);
        appendAbnormalLog("WARN", "skip csv line parse failed, line=" + std::to_string(line_no));
      }
    }
    return !waypoints_.empty();
  }

  bool loadWaypointsFromYamlPlaceholder(const std::string& file_path) {
    ROS_WARN_STREAM("[PATROL] YAML loader placeholder called for file: " << file_path);
    ROS_WARN("[PATROL] Please implement YAML parsing here (yaml-cpp or ROS param based loader).");
    appendAbnormalLog("WARN", "YAML loader placeholder called: " + file_path);
    return false;
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

  void buildMissionPlan() {
    // 每次构建任务时重置累计进度相关变量
    plan_indices_.clear();
    plan_cursor_ = 0;
    completed_distance_ = 0.0;
    total_distance_ = 0.0;
    entry_segment_length_ = 0.0;
    loop_first_cycle_ = true;
    first_goal_sent_ = false;

    if (single_index_ >= 0) {
      if (single_index_ >= static_cast<int>(waypoints_.size())) {
        ROS_ERROR_STREAM("[PATROL] single_index out of range: " << single_index_);
        appendAbnormalLog("ERROR", "single_index out of range: " + std::to_string(single_index_));
        transitionTo(State::FAILED, "single_index out of range");
        return;
      }
      plan_indices_.push_back(single_index_);
      total_distance_ = 0.0;
      ROS_INFO_STREAM("[PATROL] single index mode, target index=" << single_index_);
      return;
    }

    if (traverse_mode_ == TraverseMode::LOOP) {
      // LOOP 模式：计划索引按 0..N-1，且计算循环闭环距离（最后一点回第一点）
      for (size_t i = 0; i < waypoints_.size(); ++i) {
        plan_indices_.push_back(static_cast<int>(i));
      }
      loop_cycle_distance_ = computeDistanceFromPlanOnce(plan_indices_);
      if (waypoints_.size() > 1) {
        loop_cycle_distance_ += distance2D(waypoints_.back(), waypoints_.front());
      }
      total_distance_ = loop_cycle_distance_;
      if (total_distance_ < 1e-6) total_distance_ = 1.0;
      ROS_INFO_STREAM("[PATROL] traverse mode LOOP, waypoint count=" << plan_indices_.size());
      return;
    }

    if (traverse_mode_ == TraverseMode::ONCE_AND_STOP) {
      for (size_t i = 0; i < waypoints_.size(); ++i) {
        plan_indices_.push_back(static_cast<int>(i));
      }
    } else if (traverse_mode_ == TraverseMode::ONCE_AND_RETURN) {
      // ONCE_AND_RETURN：在 0..N-1 之后附加一个“回到 0”的目标
      for (size_t i = 0; i < waypoints_.size(); ++i) {
        plan_indices_.push_back(static_cast<int>(i));
      }
      if (waypoints_.size() > 1) {
        plan_indices_.push_back(0);
      }
    }

    total_distance_ = computeDistanceFromPlanOnce(plan_indices_);
    if (total_distance_ < 1e-6) total_distance_ = 1.0;
    ROS_INFO_STREAM("[PATROL] finite mission prepared. mode=" << traverseModeName()
                    << " plan_size=" << plan_indices_.size()
                    << " total_distance(base)=" << total_distance_);
  }

  double computeDistanceFromPlanOnce(const std::vector<int>& plan) const {
    if (plan.size() < 2) {
      return 0.0;
    }
    double sum = 0.0;
    for (size_t i = 1; i < plan.size(); ++i) {
      sum += distance2D(waypoints_[plan[i - 1]], waypoints_[plan[i]]);
    }
    return sum;
  }

  std::string traverseModeName() const {
    switch (traverse_mode_) {
      case TraverseMode::LOOP:
        return "LOOP";
      case TraverseMode::ONCE_AND_STOP:
        return "ONCE_AND_STOP";
      case TraverseMode::ONCE_AND_RETURN:
        return "ONCE_AND_RETURN";
      default:
        return "UNKNOWN";
    }
  }

  void publishWaypointsVisualization() {
    if (waypoints_.size() <= 1) {
      ROS_INFO("[PATROL] waypoint size <= 1, skip marker visualization.");
      return;
    }

    visualization_msgs::MarkerArray marker_array;

    visualization_msgs::Marker points;
    points.header.frame_id = waypoint_frame_;
    points.header.stamp = ros::Time::now();
    points.ns = "patrol_waypoints";
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
    line.ns = "patrol_waypoints";
    line.id = 1;
    line.type = visualization_msgs::Marker::LINE_STRIP;
    line.action = visualization_msgs::Marker::ADD;
    line.scale.x = 0.08;
    line.color.r = 0.2f;
    line.color.g = 0.6f;
    line.color.b = 1.0f;
    line.color.a = 1.0f;
    line.pose.orientation.w = 1.0;

    for (size_t i = 0; i < waypoints_.size(); ++i) {
      geometry_msgs::Point pt;
      pt.x = waypoints_[i].pose.position.x;
      pt.y = waypoints_[i].pose.position.y;
      pt.z = waypoints_[i].pose.position.z;
      points.points.push_back(pt);
      line.points.push_back(pt);

      visualization_msgs::Marker text;
      text.header = points.header;
      text.ns = "patrol_waypoints_text";
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
    path.poses = waypoints_;
    path_pub_.publish(path);

    ROS_INFO_STREAM("[PATROL] published waypoint markers and path. marker_topic=" << marker_topic_
                    << " path_topic=" << path_topic_);
  }

  void tick(const ros::TimerEvent&) {
    // 状态机主分发：所有行为都在这里按状态驱动
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
      case State::COMPLETED:
        handleCompleted();
        break;
      case State::FAILED:
        handleFailed();
        break;
      default:
        break;
    }
  }

  void handleIdle() {
    // IDLE: 仅做任务可执行性检查与首个目标下发
    if (!task_requested_) {
      ROS_INFO_THROTTLE(5.0, "[PATROL] IDLE: waiting task request.");
      return;
    }
    if (plan_indices_.empty()) {
      transitionTo(State::FAILED, "mission plan empty");
      return;
    }
    if (mission_finished_) {
      ROS_INFO_THROTTLE(5.0, "[PATROL] IDLE: mission finished.");
      return;
    }
    sendCurrentGoal();
    transitionTo(State::NAVIGATING, "dispatch goal");
  }

  void handleNavigating() {
    // NAVIGATING:
    // 1) 实时打印进度
    // 2) 若 action 未结束，可选执行“卡住检测”
    // 3) 若 action 已结束，根据结果流转到 COMPLETED/PAUSE
    printProgress();
    if (!nav_result_ready_) {
      if (enable_stuck_replan_) {
        checkAndHandleStuck();
      }
      return;
    }
    nav_result_ready_ = false;
    if (isSuccessfulActionState(last_terminal_state_)) {
      transitionTo(State::COMPLETED, "goal reached");
    } else {
      transitionTo(State::PAUSE, "goal failed");
    }
  }

  void handlePause() {
    // PAUSE:
    // 未超过重试上限 -> 重发当前点
    // 超过上限 -> 跳过当前点一次，前往下一个候选点
    if (retry_count_ >= retry_max_) {
      ROS_ERROR_STREAM("[PATROL] waypoint index " << current_waypoint_index_
                       << " failed after " << retry_max_
                       << " retries, skip this point once.");
      appendAbnormalLog("ERROR",
                        "waypoint failed after max retries, skip once, index=" +
                            std::to_string(current_waypoint_index_));

      // Skip current failed point once, then continue with the next candidate point.
      if (!advanceToNextGoal()) {
        appendAbnormalLog("ERROR", "no next waypoint after max-retry skip");
        transitionTo(State::FAILED, "no next waypoint after max-retry skip");
        return;
      }

      retry_count_ = 0;
      sendCurrentGoal();
      transitionTo(State::NAVIGATING, "skip failed waypoint and continue");
      return;
    }
    ++retry_count_;
    ROS_WARN_STREAM("[PATROL] retry " << retry_count_ << "/" << retry_max_
                    << " for waypoint index " << current_waypoint_index_);
    sendCurrentGoal();
    transitionTo(State::NAVIGATING, "retry dispatched");
  }

  void handleCompleted() {
    // COMPLETED:
    // 记录当前段距离进度，然后尝试流转到下一个目标点
    // 若没有后续点，则任务结束回到 IDLE
    retry_count_ = 0;
    completed_distance_ += current_segment_length_;

    if (!advanceToNextGoal()) {
      mission_finished_ = true;
      task_requested_ = false;
      transitionTo(State::IDLE, "mission completed");
      return;
    }

    sendCurrentGoal();
    transitionTo(State::NAVIGATING, "next waypoint");
  }

  void handleFailed() {
    // FAILED:
    // 一次性执行取消动作并上报异常，之后仅保留心跳式错误日志
    if (!failure_logged_) {
      failure_logged_ = true;
      if (goal_in_flight_) {
        move_base_client_->cancelAllGoals();
      }
      ROS_ERROR("[PATROL] entered FAILED state. waiting external intervention.");
      appendAbnormalLog("ERROR", "entered FAILED state");
    }
    ROS_ERROR_THROTTLE(5.0, "[PATROL] FAILED: no further action.");
  }

  void sendCurrentGoal() {
    // 组装并发送当前目标点到 MBF action server
    // 同时更新“当前段起点/长度”，供进度计算使用
    if (plan_indices_.empty()) {
      transitionTo(State::FAILED, "send goal but plan empty");
      return;
    }
    if (plan_cursor_ >= plan_indices_.size()) {
      transitionTo(State::FAILED, "plan cursor out of range");
      return;
    }

    current_waypoint_index_ = plan_indices_[plan_cursor_];
    current_goal_pose_ = waypoints_[current_waypoint_index_];
    current_goal_pose_.header.stamp = ros::Time::now();

    geometry_msgs::PoseStamped start_pose;
    if (!has_last_waypoint_) {
      if (getRobotPose(start_pose)) {
        current_segment_start_pose_ = start_pose;
      } else {
        current_segment_start_pose_ = current_goal_pose_;
      }
    } else {
      current_segment_start_pose_ = waypoints_[last_waypoint_index_];
    }
    current_segment_length_ = distance2D(current_segment_start_pose_, current_goal_pose_);
    if (current_segment_length_ < 1e-6) current_segment_length_ = 1e-6;

    if (!first_goal_sent_) {
      // 首次发目标时，修正总进度分母（single/非 LOOP 模式需要计入入场段）
      first_goal_sent_ = true;
      entry_segment_length_ = current_segment_length_;
      if (single_index_ >= 0 || traverse_mode_ != TraverseMode::LOOP) {
        total_distance_ += entry_segment_length_;
        if (total_distance_ < 1e-6) {
          total_distance_ = 1e-6;
        }
      }
      ROS_INFO_STREAM("[PATROL] mission entry segment=" << entry_segment_length_
                      << " adjusted total_distance=" << total_distance_);
    }

    mbf_msgs::MoveBaseGoal goal;
    goal.target_pose = current_goal_pose_;

    goal_in_flight_ = true;
    nav_result_ready_ = false;
    resetStuckMonitor();
    move_base_client_->sendGoal(goal,
                                boost::bind(&PatrolStateMachine::doneCb, this, _1, _2),
                                boost::bind(&PatrolStateMachine::activeCb, this),
                                boost::bind(&PatrolStateMachine::feedbackCb, this, _1));

    ROS_INFO_STREAM("[PATROL] send goal waypoint[" << current_waypoint_index_ << "] "
                    << "x=" << current_goal_pose_.pose.position.x
                    << " y=" << current_goal_pose_.pose.position.y
                    << " yaw=" << tf2::getYaw(current_goal_pose_.pose.orientation)
                    << " plan_cursor=" << plan_cursor_);
  }

  bool advanceToNextGoal() {
    // 目标推进逻辑：
    // - single_index: 无下一个点
    // - LOOP: 循环回到 0，并在每轮开始重置 completed_distance_
    // - 其他模式: 到末尾即返回 false
    has_last_waypoint_ = true;
    last_waypoint_index_ = current_waypoint_index_;

    if (single_index_ >= 0) {
      return false;
    }

    if (traverse_mode_ == TraverseMode::LOOP) {
      ++plan_cursor_;
      if (plan_cursor_ >= plan_indices_.size()) {
        plan_cursor_ = 0;
        ++loop_count_;
        completed_distance_ = 0.0;
        loop_first_cycle_ = false;
        ROS_INFO_STREAM("[PATROL] LOOP cycle completed. loop_count=" << loop_count_);
      }
      return true;
    }

    ++plan_cursor_;
    if (plan_cursor_ >= plan_indices_.size()) {
      return false;
    }
    return true;
  }

  void printProgress() {
    // 进度 = (已完成路段 + 当前路段已走距离) / 总任务距离
    // LOOP 模式每轮重置，首轮额外计入“入场段”修正
    const ros::Time now = ros::Time::now();
    if ((now - last_progress_log_time_).toSec() < progress_log_period_) {
      return;
    }
    last_progress_log_time_ = now;

    geometry_msgs::PoseStamped robot_pose;
    if (!getRobotPose(robot_pose)) {
      ROS_WARN_THROTTLE(2.0, "[PATROL] progress: TF unavailable.");
      return;
    }

    double traveled = distance2D(current_segment_start_pose_, robot_pose);
    traveled = std::min(traveled, current_segment_length_);

    double denom = total_distance_;
    if (denom < 1e-6) denom = 1.0;
    double numer = completed_distance_ + traveled;

    if (traverse_mode_ == TraverseMode::LOOP) {
      // LOOP 模式显示当前循环进度，百分比每循环重置。
      const double first_cycle_extra = loop_first_cycle_ ? entry_segment_length_ : 0.0;
      denom = std::max(1e-6, loop_cycle_distance_ + first_cycle_extra);
      numer = completed_distance_ + traveled;
    }

    const double pct = std::max(0.0, std::min(100.0, 100.0 * numer / denom));
    ROS_INFO_STREAM("[PATROL] progress=" << pct << "% "
                    << "state=" << stateToString(state_)
                    << " wp_idx=" << current_waypoint_index_
                    << " retry=" << retry_count_
                    << (traverse_mode_ == TraverseMode::LOOP ? (" loop=" + std::to_string(loop_count_)) : ""));
  }

  bool getRobotPose(geometry_msgs::PoseStamped& pose_out) {
    // 从 TF 获取机器人在 waypoint_frame_ 下的位置，用于进度估计
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
      ROS_WARN_THROTTLE(2.0, "[PATROL] TF lookup failed: %s", ex.what());
      return false;
    }
  }

  void odomCb(const nav_msgs::Odometry::ConstPtr& msg) {
    // 保存最新里程计，用于卡住检测
    latest_odom_ = *msg;
    has_odom_ = true;
  }

  void resetStuckMonitor() {
    // 每次发送新目标时重置低速计时器
    low_speed_started_ = false;
    low_speed_start_time_ = ros::Time(0);
  }

  void checkAndHandleStuck() {
    // 卡住判据：线速度和角速度均低于阈值，并持续超过超时时间
    // 处理策略：取消当前目标 -> 进入 PAUSE（按既有重试/跳过规则处理）
    if (!goal_in_flight_) {
      return;
    }
    if (!has_odom_) {
      ROS_WARN_THROTTLE(2.0, "[PATROL] stuck monitor waiting odom on topic: %s", odom_topic_.c_str());
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
                        "[PATROL] low speed %.2fs/%.2fs (v=%.3f, wz=%.3f), monitoring...",
                        low_speed_duration, stuck_timeout_sec_, linear_speed, angular_speed);
      return;
    }

    ROS_WARN("[PATROL] robot appears stuck (low speed timeout), cancel current goal and replan.");
    appendAbnormalLog("WARN", "stuck detected, cancel current goal and replan");
    move_base_client_->cancelGoal();
    goal_in_flight_ = false;
    nav_result_ready_ = false;
    resetStuckMonitor();
    transitionTo(State::PAUSE, "stuck detected, retry replan");
  }

  void transitionTo(const State new_state, const std::string& reason) {
    // 统一状态切换入口，便于日志与异常统计集中管理
    if (state_ == new_state) {
      return;
    }
    if (new_state == State::FAILED) {
      appendAbnormalLog("ERROR", "state transition to FAILED, reason=" + reason);
    }
    ROS_INFO_STREAM("[PATROL] state transition: " << stateToString(state_)
                    << " -> " << stateToString(new_state)
                    << " | reason=" << reason);
    state_ = new_state;
  }

  void activeCb() {
    ROS_INFO_STREAM("[PATROL] action active. wp_idx=" << current_waypoint_index_);
  }

  void feedbackCb(const mbf_msgs::MoveBaseFeedbackConstPtr& /*feedback*/) {}

  void doneCb(const actionlib::SimpleClientGoalState& state,
              const mbf_msgs::MoveBaseResultConstPtr& result) {
    // Action 结束回调：记录终态，唤醒 NAVIGATING 状态内的结果处理
    goal_in_flight_ = false;
    last_terminal_state_ = state;
    nav_result_ready_ = true;
    const int outcome = result ? result->outcome : -1;
    const std::string message = result ? result->message : "null result";
    ROS_INFO_STREAM("[PATROL] action done. wp_idx=" << current_waypoint_index_
                    << " state=" << state.toString()
                    << " outcome=" << outcome
                    << " msg=" << message);
    if (state.state_ != actionlib::SimpleClientGoalState::SUCCEEDED) {
      appendAbnormalLog("WARN",
                        "action done non-success, wp_idx=" + std::to_string(current_waypoint_index_) +
                            " state=" + state.toString() + " outcome=" + std::to_string(outcome) +
                            " msg=" + message);
    }
  }

  std::string nowString() const {
    // 统一时间格式：YYYY-MM-DD HH:MM:SS
    const std::time_t t = std::time(nullptr);
    std::tm tm_buf{};
    localtime_r(&t, &tm_buf);
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
    return oss.str();
  }

  void initAbnormalLog() {
    // 以追加模式打开日志文件，并写入本次运行起始标记
    abnormal_log_stream_.open(abnormal_log_file_, std::ios::out | std::ios::app);
    if (!abnormal_log_stream_.is_open()) {
      ROS_ERROR_STREAM("[PATROL] failed to open abnormal log file: " << abnormal_log_file_);
      return;
    }
    if (!run_start_logged_) {
      abnormal_log_stream_ << "\n===== Patrol Run Start: " << nowString() << " =====\n";
      abnormal_log_stream_.flush();
      run_start_logged_ = true;
    }
  }

  void appendAbnormalLog(const std::string& level, const std::string& text) {
    // 仅记录异常事件（WARN/ERROR），正常流程不写文件
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
  ros::Publisher marker_pub_;
  ros::Publisher path_pub_;
  ros::Subscriber odom_sub_;

  State state_{State::IDLE};
  TraverseMode traverse_mode_{TraverseMode::LOOP};

  std::string action_name_;
  std::string waypoint_file_;
  std::string waypoint_frame_;
  std::string base_frame_;
  std::string marker_topic_;
  std::string path_topic_;
  std::string odom_topic_;
  std::string abnormal_log_file_;

  int retry_max_{5};
  int retry_count_{0};
  int single_index_{-1};
  bool auto_start_{true};
  bool task_requested_{false};
  bool mission_finished_{false};
  bool failure_logged_{false};
  bool goal_in_flight_{false};
  bool nav_result_ready_{false};
  bool enable_stuck_replan_{true};
  bool has_odom_{false};
  bool low_speed_started_{false};
  bool run_start_logged_{false};
  bool has_last_waypoint_{false};
  bool first_goal_sent_{false};
  double tick_hz_{10.0};
  double progress_log_period_{1.0};
  double stuck_timeout_sec_{5.0};
  double stuck_linear_vel_eps_{0.02};
  double stuck_angular_vel_eps_{0.05};

  int loop_count_{0};
  int current_waypoint_index_{-1};
  int last_waypoint_index_{-1};
  size_t plan_cursor_{0};

  double total_distance_{1.0};
  double completed_distance_{0.0};
  double current_segment_length_{0.0};
  double entry_segment_length_{0.0};
  double loop_cycle_distance_{1.0};
  bool loop_first_cycle_{true};
  ros::Time last_progress_log_time_;
  ros::Time low_speed_start_time_;

  geometry_msgs::PoseStamped current_goal_pose_;
  geometry_msgs::PoseStamped current_segment_start_pose_;
  nav_msgs::Odometry latest_odom_;
  std::ofstream abnormal_log_stream_;
  actionlib::SimpleClientGoalState last_terminal_state_{actionlib::SimpleClientGoalState::LOST};

  std::vector<geometry_msgs::PoseStamped> waypoints_;
  std::vector<int> plan_indices_;
};

int main(int argc, char** argv) {
  ros::init(argc, argv, "patrol_state_machine_node");
  PatrolStateMachine node;
  ros::spin();
  return 0;
}

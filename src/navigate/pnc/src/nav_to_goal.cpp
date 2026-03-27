#include "xjrobot_pnc/nav_to_goal.h"


namespace nav_to_goal {

uint8_t* Task::cost_translation_ = nullptr;

Task::Task():
    tf_(new tf2_ros::Buffer()),
    tfl_(new tf2_ros::TransformListener(*tf_)),
    goto_ctrl_(new GotoCtrl("move_base_flex/move_base")),
    exe_ctrl_(new ExeCtrl("move_base_flex/exe_path")),
    cur_state_(StateValue::Idle)
{
  service_thread = std::thread(&Task::serviceThread, this);

  if (!cost_translation_) {
    cost_translation_ = new uint8_t[101];
    for (int i = 0; i < 101; ++i) {
      cost_translation_[i] = static_cast<uint8_t>(i * 254 / 100);
    }
  }
}
Task::~Task()
{
  if (service_thread.joinable()) {
    service_thread.join();
  }
}

void Task::init()
{
  ros::NodeHandle nh;
  vel_pub_ = nh.advertise<geometry_msgs::Twist>("cmd_vel", 1);
  visual_path_pub_ = nh.advertise<nav_msgs::Path>("record_waypoint_visual",1);
  // keyboard_sub_ = nh.subscribe<std_msgs::Int32>("keyboard_input", 1, &Task::keyboardInputCB,this);
  // costmap_sub_ = nh.subscribe(COSTMAP, 5, &Task::costmapCB, this);
  // costmap_update_sub_ = nh.subscribe(COSTMAP_UPDATE, 5, &Task::costmapUpdateCB, this);

  nh.param<std::string>("file_path", FILE_PATH, "/home/wu/xjrobot/PATH/path_1");


  ROS_ERROR_COND(!goto_ctrl_->waitForServer(ros::Duration(5.0)), "move base action not online!");
  ROS_ERROR_COND(!exe_ctrl_->waitForServer(ros::Duration(5.0)), "exe path action not online!");
  ROS_INFO("Task Manager Initialized!");
}

void Task::run()
{
  ros::Rate r(ros::Duration(0.05));
  while (ros::ok())
  {
    ros::spinOnce();
    switch (cur_state_)
    {
      case StateValue::Idle:
        ROS_INFO_THROTTLE(3.0,"Idle");
        break;
      case StateValue::RecordStart:
        ROS_INFO_ONCE("record start.");
        // recordPath();
        break;
      case StateValue::RecordStop:
        ROS_INFO_ONCE("record stop.");
        break;
      case StateValue::Pause:
        break;
      case StateValue::Navigate:

        if(last_state_ == StateValue::Idle){
          sendGoto(cur_goal_);
        }

        if(is_going_home_) navigate_home(); 
        else               navigate();

        break;
      case StateValue::Wait:
        // ROS_INFO("Wait and send next waypoint. ");
        // cur_index_ ++;
        // sendGoto(cur_index_);
        // cur_state_ = StateValue::Navigate;
        break;  
      default:
        break;
    }
    last_state_ = cur_state_;
    r.sleep();
  }
}

auto Task::getRobotPose() -> std::optional<geometry_msgs::Pose>
{
  geometry_msgs::Pose ret_pose;
  geometry_msgs::TransformStamped transformStamped;
  try
  {
    transformStamped = tf_->lookupTransform("map", "base_link", ros::Time(0), ros::Duration(0.1));
  } catch (tf2::TransformException &ex)
  {
    ROS_WARN("%s", ex.what());
    return std::nullopt;
  }

  ret_pose.position.x = transformStamped.transform.translation.x;
  ret_pose.position.y = transformStamped.transform.translation.y;
  ret_pose.orientation = transformStamped.transform.rotation;
  ROS_DEBUG_STREAM("getRobotPose: " << ret_pose);
  return std::make_optional(ret_pose);

}

void Task::pubZeroVel()
{
  geometry_msgs::Twist zero_vel;
  zero_vel.angular.x = 0;
  zero_vel.angular.y = 0;
  zero_vel.angular.z = 0;
  zero_vel.linear.x = 0;
  zero_vel.linear.y = 0;
  zero_vel.linear.z = 0;
  vel_pub_.publish(zero_vel);
}

void Task::stop()
{
  cancelNav();
  // pub zero velocity for 1s
  for (auto i = 0; i < 5; ++i) {
    pubZeroVel();
    ros::Duration(0.1).sleep();
  }
}
void Task::cancelNav() {
  if (!goto_ctrl_->getState().isDone()) {
    ROS_INFO("Cancel current goto goal");
    goto_ctrl_->cancelGoal();
  }

  if (!exe_ctrl_->getState().isDone()) {
    ROS_INFO("Cancel current exe path goal");
    exe_ctrl_->cancelGoal();
  }
}

// 导航
void Task::navigate()
{
  if(goto_ctrl_->getState() == goalState::SUCCEEDED){
    ROS_INFO("Navigate success, arrived goal: %.2f,%.2f|%.1f", cur_goal_.pose.position.x, cur_goal_.pose.position.y, tf2::getYaw(cur_goal_.pose.orientation)*57.3);
    reset();
    nav_finished_ = true;
    nav_result_ = true;
  }else if(goto_ctrl_->getState().isDone()){
    ROS_ERROR("Navigate failed, stop.");
    reset();
    nav_finished_ = true;
    nav_result_ = false;
  }
  auto cur_pose = getRobotPose();
  ROS_INFO_THROTTLE(1.0,"Navigate... now: %.2f,%.2f|%.1f",cur_pose.value().position.x, cur_pose.value().position.y, tf2::getYaw(cur_pose.value().orientation)*57.3); 
}

// 导航至原点 + 视觉
void Task::navigate_home()
{
  if(goto_ctrl_->getState() == goalState::SUCCEEDED){
    ROS_INFO("Navigate success, arrived goal: %.2f,%.2f|%.1f", cur_goal_.pose.position.x, cur_goal_.pose.position.y, tf2::getYaw(cur_goal_.pose.orientation)*57.3);
    reset();
    // 再加入视觉导航
    ROS_INFO("Vision navigate to home...");
    
    // nav_finished_ = true;
    // nav_result_ = true;
  }else if(goto_ctrl_->getState().isDone()){
    ROS_ERROR("Navigate to home failed, stop.");
    reset();
    nav_finished_ = true;
    nav_result_ = false;
  }

  // if(nav_finished_) is_going_home_ = false;
  auto cur_pose = getRobotPose();
  ROS_INFO_THROTTLE(1.0,"Navigate to home... now: %.2f,%.2f|%.1f",cur_pose.value().position.x, cur_pose.value().position.y, tf2::getYaw(cur_pose.value().orientation)*57.3); 
}

void Task::reset()
{
  cur_state_ = StateValue::Idle;
  // cur_index_ = 0;
  // cur_waypoint_.clear();
  stop();
}

auto Task::sendGoto(geometry_msgs::PoseStamped const& goal) -> bool {
  mbf_msgs::MoveBaseGoal mbf_goal{};
  mbf_goal.target_pose = goal;
  ROS_INFO("send goal: %.2f,%.2f|%.1f", goal.pose.position.x, goal.pose.position.y, tf2::getYaw(goal.pose.orientation)*57.3);
  goto_ctrl_->sendGoal(mbf_goal,
                       boost::bind(&Task::gotoDone, this, _1, _2),
                       GotoCtrl::SimpleActiveCallback(),
                       GotoCtrl::SimpleFeedbackCallback());
  return true;
}

// 点到点结果检查
void Task::gotoDone(const actionlib::SimpleClientGoalState& state,
                             const mbf_msgs::MoveBaseResultConstPtr& result) {
  ROS_INFO("MoveBase got state [%s]", state.toString().c_str());

  if (!result) return;
  ROS_INFO("MoveBase got result [%d]", result->outcome);
}

void Task::serviceThread()
{
  ros::NodeHandle nh__;
  nav_server = nh__.advertiseService("navigate_to_goal", &Task::navServiceCB, this);
  ros::spin();
}

bool Task::navServiceCB(xjrobot_pnc::nav_goal::Request& req, xjrobot_pnc::nav_goal::Response& res)
{
  if(req.tar_x == 0 && req.tar_y == 0 ){
    is_going_home_ = true;
  }
  cur_goal_.header.frame_id = "map";
  cur_goal_.header.stamp = ros::Time::now();
  cur_goal_.pose.position.x = req.tar_x;
  cur_goal_.pose.position.y = req.tar_y;
  tf2::Quaternion q;
  q.setRPY(0, 0, req.tar_yaw);
  q.normalize();
  cur_goal_.pose.orientation.w = q.w() ;
  cur_goal_.pose.orientation.x = q.x() ;
  cur_goal_.pose.orientation.y = q.y() ;
  cur_goal_.pose.orientation.z = q.z() ;

  cur_state_ = StateValue::Navigate;
  ROS_INFO("Resquest nav goal: %.2f,%.2f|%.1f", req.tar_x, req.tar_y, req.tar_yaw);

  // 等待导航完成
  ros::Rate r(ros::Duration(0.1));
  while (ros::ok()) {
    if(nav_finished_){
      res.reach_ok = nav_result_;
      nav_finished_ = false;
      break;
    }
    r.sleep();
  }
  ROS_INFO("Resopons nav result: %d", nav_result_);
  return true;
}

}











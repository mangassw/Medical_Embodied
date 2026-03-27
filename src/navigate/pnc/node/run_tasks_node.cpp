
#include <csignal>
#include "xjrobot_pnc/run_tasks.h"

std::shared_ptr<xjrobot::Task> task_ptr;

void sigintHandler(int sig) {
  if (task_ptr) {
    task_ptr->stop();
    // task_ptr.reset();
  }

  ROS_INFO("xjrobot task_ptr shutting down!");
  ros::shutdown();
}

int main(int argc, char** argv) {
  ros::init(argc, argv, xjrobot::NODE_NAME);
  task_ptr = std::make_shared<xjrobot::Task>();
  signal(SIGINT, sigintHandler);
  task_ptr->init();
  task_ptr->run();
  return 0;
}

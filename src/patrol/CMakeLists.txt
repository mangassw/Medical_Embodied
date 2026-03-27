cmake_minimum_required(VERSION 3.0.2)
project(patrol)

find_package(catkin REQUIRED COMPONENTS
  rospy
  interfaces
  std_msgs
)

catkin_package(
  CATKIN_DEPENDS rospy interfaces std_msgs
)

catkin_install_python(PROGRAMS
  scripts/patrol_trigger_server.py
  DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)

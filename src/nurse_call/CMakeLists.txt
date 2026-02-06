cmake_minimum_required(VERSION 3.0.2)
project(nurse_call)

find_package(catkin REQUIRED COMPONENTS
  rospy
  actionlib
  interfaces
)

catkin_package(
  CATKIN_DEPENDS rospy actionlib interfaces
)

catkin_install_python(PROGRAMS
  scripts/call_nurse_mock_server.py
  scripts/call_nurse_mock_client.py
  DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)

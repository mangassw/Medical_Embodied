cmake_minimum_required(VERSION 3.0.2)
project(nav)

find_package(catkin REQUIRED COMPONENTS
  rospy
  actionlib
  interfaces
)

catkin_package(
  CATKIN_DEPENDS rospy actionlib interfaces
)

catkin_install_python(PROGRAMS
  scripts/navigate_mock_server.py
  scripts/navigate_mock_client.py
  DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)

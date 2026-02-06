cmake_minimum_required(VERSION 3.0.2)
project(charge)

find_package(catkin REQUIRED COMPONENTS
  rospy
  interfaces
)

catkin_package(
  CATKIN_DEPENDS rospy interfaces
)

catkin_install_python(PROGRAMS
  scripts/charge_services.py
  DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)

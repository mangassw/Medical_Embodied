cmake_minimum_required(VERSION 3.0.2)
project(xjrobot_bridge)

find_package(catkin REQUIRED COMPONENTS
  roscpp
  rospy
  actionlib
  interfaces
  xjrobot_task
)

catkin_package(
  CATKIN_DEPENDS roscpp rospy actionlib interfaces xjrobot_task
)

include_directories(
  ${catkin_INCLUDE_DIRS}
)

add_executable(navigate_xj_bridge_server
  src/navigate_xj_bridge_server.cpp
)

add_dependencies(navigate_xj_bridge_server ${${PROJECT_NAME}_EXPORTED_TARGETS} ${catkin_EXPORTED_TARGETS})

target_link_libraries(navigate_xj_bridge_server
  ${catkin_LIBRARIES}
)

catkin_install_python(PROGRAMS
  scripts/navigate_mock_server.py
  scripts/navigate_mock_client.py
  DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)

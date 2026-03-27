cmake_minimum_required(VERSION 3.0.2)
project(xjrobot_task)

add_compile_options(-std=c++17)

find_package(catkin REQUIRED COMPONENTS
  actionlib
  actionlib_msgs
  geometry_msgs
  message_generation
  mbf_msgs
  nav_msgs
  roscpp
  std_msgs
  std_srvs
  tf2
  tf2_ros
  visualization_msgs
)

add_service_files(
  FILES
    PatrolSetMode.srv
    PatrolAddWaypoint.srv
    PatrolAddWaypoints.srv
)

generate_messages(
  DEPENDENCIES
    geometry_msgs
    std_msgs
)

catkin_package(
  CATKIN_DEPENDS
    actionlib_msgs
    geometry_msgs
    message_runtime
    mbf_msgs
    nav_msgs
    roscpp
    std_msgs
    std_srvs
    tf2
    tf2_ros
    visualization_msgs
)

include_directories(
  ${catkin_INCLUDE_DIRS}
)

add_executable(patrol_state_machine_node
  src/patrol_state_machine.cpp
)
add_dependencies(patrol_state_machine_node ${${PROJECT_NAME}_EXPORTED_TARGETS} ${catkin_EXPORTED_TARGETS})
target_link_libraries(patrol_state_machine_node
  ${catkin_LIBRARIES}
)

add_executable(patrol_state_machine_ext_node
  src/patrol_state_machine_ext.cpp
)
add_dependencies(patrol_state_machine_ext_node ${${PROJECT_NAME}_EXPORTED_TARGETS} ${catkin_EXPORTED_TARGETS})
target_link_libraries(patrol_state_machine_ext_node
  ${catkin_LIBRARIES}
)

install(TARGETS patrol_state_machine_node patrol_state_machine_ext_node
  RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)

install(DIRECTORY launch config srv
  DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}
)

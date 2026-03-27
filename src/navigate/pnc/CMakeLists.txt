cmake_minimum_required(VERSION 3.0.2)
project(xjrobot_pnc)

add_compile_options(-std=c++17)

find_package(catkin REQUIRED COMPONENTS
        actionlib
        actionlib_msgs
        costmap_2d
        geometry_msgs
        hdl_localization
        message_generation
        mbf_msgs
        nav_msgs
        roscpp
        roslaunch
        sensor_msgs
        std_srvs
        tf2
        tf2_ros
)

add_service_files(
        DIRECTORY srv
        FILES
        exe_path.srv
        record_start.srv
        record_stop.srv
        xjrobot_task.srv
        nav_goal.srv
)

generate_messages()

catkin_package(CATKIN_DEPENDS
        actionlib_msgs
        costmap_2d
        geometry_msgs
        hdl_localization
        message_runtime
        mbf_msgs
        nav_msgs
        roscpp
        sensor_msgs
        std_srvs
        tf2
        tf2_ros
        )
include_directories(
  include
  include/${PROJECT_NAME}
  ${catkin_INCLUDE_DIRS}
)
add_executable(xjrobot_mbf_bridge
        node/mbf_bridge_node.cpp)
add_dependencies(xjrobot_mbf_bridge ${${PROJECT_NAME}_EXPORTED_TARGETS} ${catkin_EXPORTED_TARGETS})
target_link_libraries(xjrobot_mbf_bridge ${catkin_LIBRARIES})

# add_executable(xjrobot_state_machine
#         node/state_machine_node.cpp
#         src/state_machine.cpp)
# add_dependencies(xjrobot_state_machine ${${PROJECT_NAME}_EXPORTED_TARGETS} ${catkin_EXPORTED_TARGETS})
# target_link_libraries(xjrobot_state_machine ${catkin_LIBRARIES})

# add_executable(xjrobot_run_tasks
#         node/run_tasks_node.cpp
#         src/run_tasks.cpp)
# add_dependencies(xjrobot_run_tasks ${${PROJECT_NAME}_EXPORTED_TARGETS} ${catkin_EXPORTED_TARGETS})
# target_link_libraries(xjrobot_run_tasks ${catkin_LIBRARIES})
# add_executable(run_waypoint
#         node/run_waypoint_node.cpp
#         src/run_waypoint.cpp)
# add_dependencies(run_waypoint ${${PROJECT_NAME}_EXPORTED_TARGETS} ${catkin_EXPORTED_TARGETS})
# target_link_libraries(run_waypoint ${catkin_LIBRARIES})
add_executable(nav_to_goal
        node/nav_to_goal_node.cpp
        src/nav_to_goal.cpp)
add_dependencies(nav_to_goal ${${PROJECT_NAME}_EXPORTED_TARGETS} ${catkin_EXPORTED_TARGETS})
target_link_libraries(nav_to_goal ${catkin_LIBRARIES})

add_executable(localization_supervisor_node
        src/localization_supervisor.cpp)
add_dependencies(localization_supervisor_node ${${PROJECT_NAME}_EXPORTED_TARGETS} ${catkin_EXPORTED_TARGETS})
target_link_libraries(localization_supervisor_node ${catkin_LIBRARIES})

add_executable(goal_reach_debug_node
        src/goal_reach_debug.cpp)
add_dependencies(goal_reach_debug_node ${${PROJECT_NAME}_EXPORTED_TARGETS} ${catkin_EXPORTED_TARGETS})
target_link_libraries(goal_reach_debug_node ${catkin_LIBRARIES})

catkin_install_python(PROGRAMS
  scripts/vel_plotter.py
  DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)

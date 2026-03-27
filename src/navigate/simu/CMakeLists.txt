cmake_minimum_required(VERSION 3.0.2)
project(xjrobot_simu)


find_package(catkin REQUIRED COMPONENTS
	roscpp
	rospy
	std_msgs
	message_generation
	message_runtime
	tf
  tf_conversions
)

add_message_files(FILES
		robopose.msg
		fusion_analysis.msg
		)
generate_messages()

catkin_package(CATKIN_DEPENDS message_generation message_runtime roscpp)

include_directories(include ${catkin_INCLUDE_DIRS})

# add_executable(msg_bridge_node node/msg_bridge_node.cpp src/msg_bridge.cpp)
# add_dependencies(msg_bridge_node ${${PROJECT_NAME}_EXPORTED_TARGETS} ${catkin_EXPORTED_TARGETS})
# target_link_libraries(msg_bridge_node ${catkin_LIBRARIES})


# add_executable(new_msg_bridge src/new_msg_bridge.cpp)
# add_dependencies(new_msg_bridge ${${PROJECT_NAME}_EXPORTED_TARGETS} ${catkin_EXPORTED_TARGETS})
# target_link_libraries(new_msg_bridge ${catkin_LIBRARIES})

add_executable(fusion_data_node node/fusion_data_node.cpp src/fusion_data.cpp)
add_dependencies(fusion_data_node ${${PROJECT_NAME}_EXPORTED_TARGETS} ${catkin_EXPORTED_TARGETS})
target_link_libraries(fusion_data_node ${catkin_LIBRARIES})

add_executable(square_movement src/square_movement.cpp)
add_dependencies(square_movement ${${PROJECT_NAME}_EXPORTED_TARGETS} ${catkin_EXPORTED_TARGETS})
target_link_libraries(square_movement ${catkin_LIBRARIES})


install(TARGETS fusion_data_node
	DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
	)

include_directories(
	include
	${catkin_INCLUDE_DIRS}
)
install(PROGRAMS
		teleop/teleop_key
		script/plotter
		DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
		)
install(DIRECTORY include
		DESTINATION ${CATKIN_PACKAGE_INCLUDE_DESTINATION}
		FILES_MATCHING PATTERN "*.h"
		)
install(DIRECTORY launch rviz urdf worlds
		DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}
		)
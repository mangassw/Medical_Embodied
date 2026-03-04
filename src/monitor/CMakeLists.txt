cmake_minimum_required(VERSION 3.0.2)
project(monitor)

find_package(catkin REQUIRED COMPONENTS
  rospy
  roscpp
  interfaces
  std_msgs
  serial
  tf2_ros
)

catkin_package(
  CATKIN_DEPENDS rospy interfaces std_msgs
)

catkin_install_python(PROGRAMS
  scripts/monitor_mock_pub.py
  scripts/anomaly_detect_server.py
  scripts/face_identify_server.py
  DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)

#battery#
add_executable(battery_status_get battery/src/battery_status_get.cpp)
target_include_directories(battery_status_get
  PRIVATE
    ${catkin_INCLUDE_DIRS})
target_link_libraries(battery_status_get ${catkin_LIBRARIES})
#battery#

#chassis#
add_executable(control_main_node 
          chassis/car_move/node/control_main_node.cpp
          chassis/car_move/src/car_control.cpp)
target_include_directories(control_main_node
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/chassis/car_move/include
    ${catkin_INCLUDE_DIRS}                       
    ${CMAKE_SOURCE_DIR}/../devel/include)
target_link_libraries(control_main_node ${catkin_LIBRARIES})
################
add_executable(car_hardware_node 
          chassis/car_driver/node/car_hardware_node.cpp
          chassis/car_driver/src/wheel_motor_driver.cpp
          chassis/car_driver/src/car_canalyst.cpp)
target_include_directories(car_hardware_node
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/chassis/car_driver/include
    ${catkin_INCLUDE_DIRS})                    
target_link_libraries(car_hardware_node
  ${catkin_LIBRARIES}
  ${CMAKE_CURRENT_SOURCE_DIR}/chassis/car_driver/lib/libcontrolcan.so)
###############
add_executable(motor_status_get
          chassis/car_driver/node/motor_status_get.cpp)
target_include_directories(motor_status_get
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/chassis/car_driver/include
    ${catkin_INCLUDE_DIRS})                    
target_link_libraries(motor_status_get
  ${catkin_LIBRARIES})
#chassis#


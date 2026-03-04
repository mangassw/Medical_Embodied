#ifndef PRINT_CAN_MSG_H
#define PRINT_CAN_MSG_H

#include <ros/ros.h>
#include <std_msgs/String.h>
#include <geometry_msgs/Twist.h>
#include <sensor_msgs/Joy.h>
#include <std_msgs/Int16MultiArray.h>
#include <std_msgs/Float32MultiArray.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <termios.h>
#include <signal.h>
#include <cmath>
#include <ctime>
#include <cstdlib>

#include "controlcan.h"
#include "printf_utils.h"
#include "can_msgs/Frame.h"
#include <bitset>

int count_num;
void print_send_info(const VCI_CAN_OBJ* can_obj)
{
	// cout << GREEN << ">>>>>>>>>>>>>>>>> CAN1 Transmit <<<<<<<<<<<<<<<<<" << TAIL << endl;
	cout << GREEN << setfill('0') << setw(4) << dec << count_num <<"  "<< TAIL << flush;
	count_num++;  //*******全局变量 暂时用于计数*******
	cout << GREEN << "[CAN1] " << "ID:0x" <<setfill('0') << setw(3) << hex << can_obj[0].ID << TAIL << flush;

	if(can_obj[0].ExternFlag==0)      cout << GREEN << " Standard"<< TAIL <<flush;  //帧格式：标准帧
	else if(can_obj[0].ExternFlag==1) cout << GREEN << " Extend  "<< TAIL <<flush;  //帧格式：扩展帧
	if(can_obj[0].RemoteFlag==0)      cout << GREEN << " Data  "  << TAIL <<flush;  //帧类型：数据帧
	else if(can_obj[0].RemoteFlag==1) cout << GREEN << " Remote"  << TAIL <<flush;  //帧类型：远程帧
	cout << GREEN << "DLC:0x" << setfill('0') << setw(2) <<hex << (int)can_obj[0].DataLen << TAIL << flush;
	cout << GREEN  << " [DATA:0x"<< TAIL << flush;
	for(int i=0;i<can_obj[0].DataLen;i++)
	{
		cout << GREEN << " " << setfill('0') << setw(2) << (int)can_obj[0].Data[i] << TAIL << flush;
	}
	cout << GREEN  << "]"<< TAIL <<endl; 
}
void print_send_info_can2(const VCI_CAN_OBJ* can_obj)
{
	// cout << GREEN << ">>>>>>>>>>>>>>>>> CAN2 Transmit <<<<<<<<<<<<<<<<<" << TAIL << endl;
	cout << GREEN << setfill('0') << setw(4) << dec << count_num <<"  "<< TAIL << flush;
	count_num++;  //*******全局变量 暂时用于计数*******
	cout << GREEN << "[CAN2] " << "ID:0x" <<setfill('0') << setw(3) << hex << can_obj[0].ID << TAIL << flush;

	if(can_obj[0].ExternFlag==0)      cout << GREEN << " Standard"<< TAIL <<flush;  //帧格式：标准帧
	else if(can_obj[0].ExternFlag==1) cout << GREEN << " Extend  "<< TAIL <<flush;  //帧格式：扩展帧
	if(can_obj[0].RemoteFlag==0)      cout << GREEN << " Data  "  << TAIL <<flush;  //帧类型：数据帧
	else if(can_obj[0].RemoteFlag==1) cout << GREEN << " Remote"  << TAIL <<flush;  //帧类型：远程帧
	cout << GREEN << "DLC:0x" << setfill('0') << setw(2) <<hex << (int)can_obj[0].DataLen << TAIL << flush;
	cout << GREEN  << " [DATA:0x"<< TAIL << flush;
	for(int i=0;i<can_obj[0].DataLen;i++)
	{
		cout << GREEN << " " << setfill('0') << setw(2) << (int)can_obj[0].Data[i] << TAIL << flush;
	}
	cout << GREEN  << "]"<< TAIL <<endl; 
}
void print_can1_recv(const can_msgs::Frame::ConstPtr& can_msg)
{
	// cout << BLUE << ">>>>>>>>>>>>>>>>> CAN1 Receive <<<<<<<<<<<<<<<<<" << TAIL << endl;
	cout << BLUE << setfill('0') << setw(4) << dec << count_num <<"  "<< TAIL << flush;
	count_num ++;
	cout << BLUE << "[CAN1] "<< "ID:0x" <<setfill('0') << setw(3) << hex << can_msg->id << TAIL << flush;
	if(can_msg->is_extended==0)      cout << BLUE << " Standard"<< TAIL <<flush;  //帧格式：标准帧
	else if(can_msg->is_extended==1) cout << BLUE << " Extend  "<< TAIL <<flush;  //帧格式：扩展帧
	if(can_msg->is_rtr==0)           cout << BLUE << " Data  "  << TAIL <<flush;  //帧类型：数据帧
	else if(can_msg->is_rtr==1)      cout << BLUE << " Remote"  << TAIL <<flush;  //帧类型：远程帧
	cout << BLUE << "DLC:0x" << setfill('0') << setw(2) <<hex << (int)can_msg->dlc<< TAIL << flush;
	cout << BLUE  << " [DATA:0x"<< TAIL << flush;
	for(int i = 0; i < can_msg->dlc; i++)
	{
		cout << BLUE<< " " << setfill('0') << setw(2) << (int)can_msg->data[i]<< TAIL << flush;
	}
	cout << BLUE<< "] " << TAIL <<endl;
	// cout << BLUE <<"] TS:0x" << setfill('0') << setw(5) << hex <<can_obj. << TAIL << endl;
}
void print_can2_recv(const can_msgs::Frame::ConstPtr& can_msg)
{
	// cout << BLUE << ">>>>>>>>>>>>>>>>> CAN2 Receive <<<<<<<<<<<<<<<<<" << TAIL << endl;
	// cout << BLUE << setfill('0') << setw(4) << dec << count_num <<"  "<< TAIL << flush;
	// count_num ++;
	// cout << BLUE << "[CAN2] "<< "ID:0x" <<setfill('0') << setw(3) << hex << can_msg->id << TAIL << flush;
	// if(can_msg->is_extended==0)      cout << BLUE << " Standard"<< TAIL <<flush;  //帧格式：标准帧
	// else if(can_msg->is_extended==1) cout << BLUE << " Extend  "<< TAIL <<flush;  //帧格式：扩展帧
	// if(can_msg->is_rtr==0)           cout << BLUE << " Data  "  << TAIL <<flush;  //帧类型：数据帧
	// else if(can_msg->is_rtr==1)      cout << BLUE << " Remote"  << TAIL <<flush;  //帧类型：远程帧
	// cout << BLUE << "DLC:0x" << setfill('0') << setw(2) <<hex << (int)can_msg->dlc<< TAIL << flush;
	// cout << BLUE  << " [DATA:0x"<< TAIL << flush;
	// for(int i = 0; i < can_msg->dlc; i++)
	// {
	// 	cout << BLUE<< " " << setfill('0') << setw(2) << (int)can_msg->data[i]<< TAIL << flush;
	// }
	// cout << BLUE<< "] " << TAIL <<endl;
	float res_data;
	static int32_t  res_data1,res_data2;
	res_data = can_msg->data[1] + (can_msg->data[2] << 8) + (can_msg->data[3] << 16) + (can_msg->data[4] << 24);
	res_data = -res_data;
	if(can_msg->id == 1 && can_msg->data[0] == 0x08) 	   res_data1 = static_cast<int32_t>(res_data);
	else if(can_msg->id == 2 && can_msg->data[0] == 0x08)  res_data2 = static_cast<int32_t>(res_data);
	
	if(std::abs(res_data1) < 30000 && std::abs(res_data2) < 30000)
		ROS_INFO_STREAM_THROTTLE(1,"GearFore: "<< dec<< static_cast<float>(res_data1/10000) <<"w,  GearBack:"<< static_cast<float>(res_data2/10000) <<"w.");
	else
		ROS_WARN_STREAM_THROTTLE(1,"GearFore: "<< dec<< static_cast<float>(res_data1/10000) <<"w,  GearBack:"<< static_cast<float>(res_data2/10000) <<"w.");
	// switch (can_msg->data[0])
	// {
	// 	case 0x08:
	// 		cout << DEEPGREEN1 << " --->>> [ Position ] :  " <<dec <<fixed <<setprecision(0)<< res_data << TAIL << endl;
	// 		break;
	// 	case 0x04:
	// 		cout << DEEPGREEN1 << " --->>> [  Current ] :  " <<dec << res_data << " mA" << TAIL << endl;
	// 		break;
	// 	case 0x0A:
	// 		cout << DEEPGREEN1 << " --->>> [  State   ] :  " <<bitset<sizeof(res_data)*8>(res_data) << TAIL << flush;
	// 		cout << DEEPGREEN1 << "      [ default : 0 ] " << TAIL << endl;
	// 		break;
	// 	case 0x12:
	// 		cout << DEEPGREEN1 << " --->>> [  Pos_Kp  ] :  " <<dec << res_data << TAIL << endl;
	// 		break;
	// 	case 0x34:
	// 		cout << DEEPGREEN1 << " --->>> [  Pos_Ki  ] :  " <<dec << res_data << TAIL << endl;
	// 		break;
	// 	case 0x13:
	// 		cout << DEEPGREEN1 << " --->>> [  Pos_Kd  ] :  " <<dec << res_data << TAIL << endl;
	// 		break;
	// 	case 0x14:
	// 		cout << DEEPGREEN1 << " --->>> [  BUS_Vol ] :  " <<dec << res_data <<" V" << TAIL << endl;
	// 		break;
	// 	case 0x31:
	// 		cout << DEEPGREEN1 << " --->>> [Motor_Temp] :  " <<dec << res_data <<" °C" << TAIL << endl;
	// 		break;
	// 	case 0x32:
	// 		cout << DEEPGREEN1 << " --->>> [ PCB_Temp ] :  " <<dec << res_data <<" °C" << TAIL << endl;
	// 		break;
	// 	default:
	// 		break;
	// }
	// cout << BLUE <<"] TS:0x" << setfill('0') << setw(5) << hex <<can_obj. << TAIL << endl;
}

void print_recv_velocity(const can_msgs::Frame::ConstPtr& can_msg)
{
	// cout << BLUE << ">>>>>>>>>>>>>>>>> CAN1 Receive <<<<<<<<<<<<<<<<<" << TAIL << endl;
	// uint8_t test[8] = {0x64,0x00,0x9c,0xff};
	uint16_t left_v,right_v;
	left_v  = (can_msg->data[1]<<8) | can_msg->data[0];
	right_v = (can_msg->data[3]<<8) | can_msg->data[2];
	if(left_v > 32768)  
	{
		left_v = -left_v;
		cout << WHITE <<"-------->  [Actual] left_v: -" << setfill('0') << setw(5) << dec << left_v << flush;
	}else 
		cout << WHITE <<"-------->  [Actual] left_v:  " << setfill('0') << setw(5) << dec << left_v << flush;
	if(right_v > 32768) 
	{
		right_v = -right_v;
		cout << WHITE <<"  ----- right_v: -" << setfill('0') << setw(5) << dec << right_v << endl;
	}else 
		cout << WHITE <<"  ----- right_v:  " << setfill('0') << setw(5) << dec << right_v << endl;
}





#endif
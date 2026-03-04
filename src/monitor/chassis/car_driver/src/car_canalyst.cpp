#include "car_canalyst.h"

namespace car {
// -------------------------------------------------------------------------------------------------------------
// ------------------------------------------- CAN分析仪
// --------------------------------------------------------
// -------------------------------------------------------------------------------------------------------------
CarCanalyst::~CarCanalyst() {
  usleep(100000);                   // 延时100ms。
  VCI_ResetCAN(VCI_USBCAN2, 0, 0);  // 复位CAN1通道。
  VCI_ResetCAN(VCI_USBCAN2, 0, 1);  // 复位CAN2通道。
  usleep(100000);                   // 延时100ms。
  VCI_CloseDevice(VCI_USBCAN2, 0);  // 关闭设备。
  std::cout << YELLOW << "--->  ......" << TAIL << std::endl;
  std::cout << YELLOW << "--->  ......" << TAIL << std::endl;
  std::cout << YELLOW << "--->  CarCanalyst had closed !  <---" << TAIL
            << std::endl;
  std::cout << YELLOW << "--->  CarCanalyst had closed !  <---" << TAIL
            << std::endl;
}

void CarCanalyst::init() {
  // 初始化 订阅cmd_vel节点发布的速度消息，CAN再将此消息发送到设备。
  real_ctrl_sub_ = nh.subscribe<interfaces::RealCMD>("/realcmd_ctrl", 10, &CarCanalyst::real_ctrl_cb, this);
  can_msg_fb_pub_ = nh.advertise<can_msgs::Frame>("/can_msg_fb", 1);
	
}

void CarCanalyst::real_ctrl_cb(const interfaces::RealCMD::ConstPtr& real_cmd) {
  static int32_t last_cmd[6];
  static int32_t last_spc_cmd[3];
  std_msgs::Int16MultiArray can_trans_err_code;
  can_trans_err_code.data.resize(3);
  WheelMotorCanCtrl MotorCMD;
  // 【特殊指令】 急停、重新使能、失能
  last_spc_cmd[0] = real_cmd->stop_flag;
  if (real_cmd->stop_flag) {
    MotorCMD.Clear_Error_Code(0x01);
    usleep(10000);
    MotorCMD.Re_Enabled(0x01);
    usleep(10000);
    if (MotorCMD.Quick_Stop(0x01))  // 急停
      cout << RED1 << ">>>>  Wheel Quick Stop !  <<<<" << TAIL << endl;
    return;
  }
  if (last_spc_cmd[1] != real_cmd->re_enabled) {
    last_spc_cmd[1] = real_cmd->re_enabled;
    if (real_cmd->re_enabled) {
      MotorCMD.Clear_Error_Code(0x01);
      usleep(10000);
      if (MotorCMD.Re_Enabled(0x01))  // 重新使能
        cout << GREEN1 << ">>>>  Wheel Re_Enabled !  <<<<" << TAIL << endl;
      usleep(20000);
      return;
    }
  }
  if (last_spc_cmd[2] != real_cmd->disabled) {
    last_spc_cmd[2] = real_cmd->disabled;
    if (real_cmd->disabled) {
      if (MotorCMD.Driver_Disabled(0x01))  // 失能电机
        cout << RED1 << ">>>>  Wheel Disabled !    <<<<" << TAIL << endl;
      usleep(20000);
      return;
    }
  }

// 【轮毂电机运动】
#define wheel_max_rpm 150  // 约等于线速度0.4m/s
  uint16_t wh_vel_left = real_cmd->wheel_left_v;
  uint16_t wh_vel_righ = real_cmd->wheel_right_v;
  if (wh_vel_left >= wheel_max_rpm && wh_vel_left <= 30000)
    wh_vel_left = wheel_max_rpm;
  else if (wh_vel_left <= (65535 - wheel_max_rpm) && wh_vel_left >= 30000)
    wh_vel_left = 65535 - wheel_max_rpm;
  if (wh_vel_righ >= wheel_max_rpm && wh_vel_righ <= 30000)
    wh_vel_righ = wheel_max_rpm;
  else if (wh_vel_righ <= (65535 - wheel_max_rpm) && wh_vel_righ >= 30000)
    wh_vel_righ = 65535 - wheel_max_rpm;  // 左右轮终值速度限幅

  MotorCMD.Velocity_Joy_Control(0x01, wh_vel_left, wh_vel_righ);

  // 一次判断，相同数据没必要持续显示输出
  static int last_wheel_1, last_wheel_2;
  static bool OnceFlag3, OnceFlag4;
  last_wheel_1 != real_cmd->wheel_left_v
      ? (last_wheel_1 = real_cmd->wheel_left_v, OnceFlag3 = 1)
      : OnceFlag3 = 0;
  last_wheel_2 != real_cmd->wheel_right_v
      ? (last_wheel_2 = real_cmd->wheel_right_v, OnceFlag4 = 1)
      : OnceFlag4 = 0;
  if (OnceFlag3 || OnceFlag4) {
    // if (real_cmd->car_ctrl_mode == 0)
    // 	std::cout << YELLOW1 << ">>>>  Mode: [ Manual ]" << TAIL << std::endl;
    // else if (real_cmd->car_ctrl_mode == 1)
    // 	std::cout << YELLOW1 << ">>>>  Mode: [  Auto  ]" << TAIL << std::endl;
    std::cout << GREEN1 << "--->  [Set_Wheel] Left_V: " << dec
              << real_cmd->wheel_left_v << " --- Right_V: " << dec
              << real_cmd->wheel_right_v << TAIL << std::endl;
    std::cout << " " << std::endl;
  }
  usleep(2000);  // 延时2ms
}

// CAN接收到消息后发布，供其他节点使用。
void CarCanalyst::receive_func() {
  int reclen = 0;
  VCI_CAN_OBJ rec[3000];  // 接收缓存，设为3000为佳。
  can_msgs::Frame can_msg;
  // can1 收到数据
  if ((reclen = VCI_Receive(VCI_USBCAN2, 0, 0, rec, 3000, 100)) > 0) {
    for (int idx = 0; idx < reclen; ++idx) {
      const VCI_CAN_OBJ& frame = rec[idx];
      // CAN在此发布接收到的消息
      can_msg.id = frame.ID;
      can_msg.dlc = frame.DataLen;
      can_msg.is_extended = frame.ExternFlag;
      can_msg.is_rtr = frame.RemoteFlag;
      can_msg.is_error = 0;
      for (uint8_t i = 0; i < frame.DataLen; ++i) {
        can_msg.data[i] = frame.Data[i];
      }

      // std::cout << GREEN1 << "[CAN RX] ID:0x" << std::hex << frame.ID
      //           << " DLC:" << std::dec << static_cast<int>(frame.DataLen)
      //           << " Data:";
      // for (uint8_t i = 0; i < frame.DataLen; ++i) {
      //   std::cout << " 0x" << std::hex << std::setfill('0') << std::setw(2)
      //             << static_cast<int>(frame.Data[i]);
      // }
      // std::cout << std::dec << TAIL << std::endl;

      can_msg_fb_pub_.publish(can_msg);
      usleep(10);
      // print_send_info(rec);
    }
  }
}
// 初始化can1设备
void CarCanalyst::init_can1(void) {
  int can1_baud_t0, can1_baud_t1;
  VCI_BOARD_INFO pInfo1[50];
  int num = 0;
  num = VCI_FindUsbDevice2(pInfo1);
  if (VCI_OpenDevice(VCI_USBCAN2, 0, 0) == 1)  // 打开设备
  {
    std::cout << DEEPGREEN1 << "--->  [CarCanalyst-II] is already turned on  "
              << std::endl;  // 打开设备成功
  } else {
    std::cout << RED1 << "--->  [CarCanalyst-II] open deivce error!"
              << std::endl;
    exit(1);
  }
  nh.param<int>("can1_baud_t0", can1_baud_t0, 0x00);
  nh.param<int>("can1_baud_t1", can1_baud_t1, 0x1c);

  // 初始化参数，严格参数二次开发函数库说明书。
  VCI_INIT_CONFIG config;
  config.AccCode = 0;
  config.AccMask = 0xFFFFFFFF;
  config.Filter = 1;             // 接收所有帧
  config.Timing0 = can1_baud_t0; /*波特率500 Kbps  0x00  0x1C*/
  config.Timing1 = can1_baud_t1;
  config.Mode = 0;  // 正常模式

  // 0 for can1 ，1 for can2.
  if (VCI_InitCAN(VCI_USBCAN2, 0, 0, &config) != 1) {
    std::cout << RED1 << "--->  [CAN1] Init error" << TAIL << std::endl;
    VCI_CloseDevice(VCI_USBCAN2, 0);
  } else {
    std::cout << DEEPGREEN1 << "--->  [CAN1] Init Success !" << TAIL
              << std::endl;
  }
  if (VCI_StartCAN(VCI_USBCAN2, 0, 0) != 1) {
    std::cout << RED1 << "--->  [CAN1] Start error" << TAIL << std::endl;
    VCI_CloseDevice(VCI_USBCAN2, 0);
  } else {
    std::cout << DEEPGREEN1 << "--->  [CAN1] Start Success !" << TAIL
              << std::endl;
  }

  std::cout << " " << std::endl;
  std::cout << DEEPGREEN1
            << ">>>>>>>>>>>>>>>>>>>>  [Can1] Param  <<<<<<<<<<<<<<<<<<<<<"
            << TAIL << std::endl;
  std::cout << DEEPGREEN1 << ">>>  Can Filter  = " << (int)config.Filter << TAIL
            << std::endl;
  std::cout << DEEPGREEN1 << ">>>  Can Timing0 = 0x" << setfill('0') << setw(2)
            << hex << (int)config.Timing0 << TAIL << std::endl;
  std::cout << DEEPGREEN1 << ">>>  Can Timing1 = 0x" << setfill('0') << setw(2)
            << hex << (int)config.Timing1 << TAIL << std::endl;
  std::cout << DEEPGREEN1 << ">>>  Can Mode    = " << (int)config.Mode << TAIL
            << std::endl
            << std::endl;
}
void CarCanalyst::init_can2(void) {
  int can2_baud_t0, can2_baud_t1;
  nh.param<int>("can2_baud_t0", can2_baud_t0, 0x00);
  nh.param<int>("can2_baud_t1", can2_baud_t1, 0x14);
  VCI_INIT_CONFIG config;
  config.AccCode = 0;
  config.AccMask = 0xFFFFFFFF;
  config.Filter = 1;             // 接收所有帧
  config.Timing0 = can2_baud_t0; /*波特率1000 Kbps  0x00  0x14*/
  config.Timing1 = can2_baud_t1;
  config.Mode = 0;  // 正常模式

  // 0 for can1 ，1 for can2.
  if (VCI_InitCAN(VCI_USBCAN2, 0, 1, &config) != 1) {
    std::cout << RED1 << "--->  [CAN2] Init error" << TAIL << std::endl;
    VCI_CloseDevice(VCI_USBCAN2, 0);
  } else {
    std::cout << DEEPGREEN1 << "--->  [CAN2] Init Success !" << TAIL
              << std::endl;
  }

  if (VCI_StartCAN(VCI_USBCAN2, 0, 1) != 1) {
    std::cout << RED1 << "--->  [CAN2] Start error" << TAIL << std::endl;
    VCI_CloseDevice(VCI_USBCAN2, 0);
  } else {
    std::cout << DEEPGREEN1 << "--->  [CAN2] Start Success !" << TAIL
              << std::endl;
  }

  std::cout << " " << std::endl;
  std::cout << DEEPGREEN1
            << ">>>>>>>>>>>>>>>>>>>>  [Can2] Param  <<<<<<<<<<<<<<<<<<<<<"
            << TAIL << std::endl;
  std::cout << DEEPGREEN1 << ">>>  Can Filter  = " << (int)config.Filter << TAIL
            << std::endl;
  std::cout << DEEPGREEN1 << ">>>  Can Timing0 = 0x" << setfill('0') << setw(2)
            << hex << (int)config.Timing0 << TAIL << std::endl;
  std::cout << DEEPGREEN1 << ">>>  Can Timing1 = 0x" << setfill('0') << setw(2)
            << hex << (int)config.Timing1 << TAIL << std::endl;
  std::cout << DEEPGREEN1 << ">>>  Can Mode    = " << (int)config.Mode << TAIL
            << std::endl
            << std::endl;
}
void CarCanalyst::dev_close(void) {
  usleep(100000);                   // 延时100ms。
  VCI_ResetCAN(VCI_USBCAN2, 0, 0);  // 复位CAN1通道。
  VCI_ResetCAN(VCI_USBCAN2, 0, 1);  // 复位CAN2通道。
  usleep(100000);                   // 延时100ms。
  VCI_CloseDevice(VCI_USBCAN2, 0);  // 关闭设备。
  std::cout << YELLOW << "--->  ......" << TAIL << std::endl;
  std::cout << YELLOW << "--->  ......" << TAIL << std::endl;
  std::cout << YELLOW << "--->  CarCanalyst had closed !  <---" << TAIL
            << std::endl;
  std::cout << YELLOW << "--->  CarCanalyst had closed !  <---" << TAIL
            << std::endl;
}

}  // namespace car

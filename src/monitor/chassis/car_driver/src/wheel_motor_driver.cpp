#include "wheel_motor_driver.h"
namespace car {

// -------------------------------------------------------------------------------------------------------------
// ------------------------------------------轮毂电机指令 --------------------------------------------------------
// -------------------------------------------------------------------------------------------------------------
void WheelMotorCanCtrl::init()
{	

	// WheelMotorCanCtrl MotorCMD;
	// MotorCMD.ZLAC8015D_Init_Velocity_Mode();

}


void WheelMotorCanCtrl::print_send_info(const VCI_CAN_OBJ* can_obj)
{
	// cout << GREEN << ">>>>>>>>>>>>>>>>> CAN1 Transmit <<<<<<<<<<<<<<<<<" << TAIL << endl;
	cout << GREEN << setfill('0') << setw(4) << dec << count_num <<"  "<< TAIL << flush;
	count_num++;  //命令计数
	cout << GREEN << "[CAN1] " << "ID:0x" <<setfill('0') << setw(3) << hex << can_obj[0].ID << TAIL << flush;

	if(can_obj[0].ExternFlag==0)      cout << GREEN << " Standard"<< TAIL <<flush;  //帧格式：标准帧
	else if(can_obj[0].ExternFlag==1) cout << GREEN << " Extend  "<< TAIL <<flush;  //帧格式：扩展帧
	if(can_obj[0].RemoteFlag==0)      cout << GREEN << " Data  "  << TAIL <<flush;  //帧类型：数据帧
	else if(can_obj[0].RemoteFlag==1) cout << GREEN << " Remote"  << TAIL <<flush;  //帧类型：远程帧
	std::cout << GREEN << "DLC:0x" << setfill('0') << setw(2) <<hex << (int)can_obj[0].DataLen << TAIL << flush;
	std::cout << GREEN  << " [DATA:0x"<< TAIL << flush;
	for(int i=0;i<can_obj[0].DataLen;i++)
	{
		std::cout << GREEN << " " << setfill('0') << setw(2) << (int)can_obj[0].Data[i] << TAIL << flush;
	}
	std::cout << GREEN  << "]"<< TAIL <<endl; 
}


// 初始化ZLAC8015D电机控制器的速度模式
void WheelMotorCanCtrl::ZLAC8015D_Init_Velocity_Mode(void)
{
  RPDO0_Config(0x01);  // control word
  usleep(1000);        // 延时1ms。
  RPDO1_Config(0x01);  // target velocity
  usleep(1000);
  RPDO2_Config(0x01);  // quick stop code
  usleep(1000);
  // RPDO2_Config(0x01);//left motor target position
  // RPDO3_Config(0x01);//right motor target position
  TPDO0_Config(0x01);  // actual velocity
  usleep(1000);
  TPDO1_Config(0x01);  // 状态反馈
  usleep(1000);
  Profile_Velocity_Init(0x01);
  usleep(1000);
  NMT_Control(0x01, 0x01);
  usleep(1000);
  Clear_Error_Code(0x01);  // 清除轮毂电机错误
  usleep(1000);
  Set_Overload_param(0x01);  // 设置过载系数300
  usleep(1000);
  Enable_Error_PWM(0x01);  // 报警后锁住电机
  usleep(1000);
  Set_max_current(0x01);  // 左右电机最大电流15A
  usleep(1000);
  Driver_Enable(0x01);
  usleep(1000);
  Set_Overload_Time(0x01);
  usleep(1000);
}

//RPDO事件触发
//RPDO0映射0x6040（控制字）
//RPDO0-COB-ID:0x200 + ID
uint8_t WheelMotorCanCtrl::RPDO0_Config(uint8_t ID)
{
	std::cout << std::endl << GREEN << "--->  [CAN Msg] Transmit: RPDO0_Config " << TAIL << std::endl;
    VCI_CAN_OBJ can_frame[1];
	uint8_t Data[8] = {0x2F, 0x00, 0x14, 0x02, 0xFE, 0x00, 0x00, 0x00};//RPDO0事件触发
	//0x2F:设置，1字节   0x1400:控制字   0x02:指定传输类型  0xFE:传输类型为事件触发 
	can_frame->ID = 0x600+ID;
    can_frame->ExternFlag = false;
    can_frame->RemoteFlag = false;
    can_frame->DataLen = 8; //有效数据长度
    for(int i=0;i<8;i++)can_frame->Data[i] = Data[i];
	if(VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1) == 1) print_send_info(can_frame);
	else std::cout << RED <<"--->  CAN Transmit ERROR!  <---" <<  TAIL << std::endl<< std::endl;

	Data[0] = 0x23;	//0x23:设置,4字节
	Data[1] = 0x00;
	Data[2] = 0x16; //0x1600:RPDO0-映射  
	Data[3] = 0x01; //0x01: PDO0,U32
	Data[4] = 0x10; 
	Data[5] = 0x00;
	Data[6] = 0x40;
	Data[7] = 0x60;	//映射至0x6040: 默认值是 60400010h
    for(int i=0;i<8;i++)can_frame->Data[i] = Data[i];
    if(VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1) == 1) print_send_info(can_frame);
	else std::cout << RED <<"--->  CAN Transmit ERROR!  <---" <<  TAIL << std::endl<< std::endl;

	Data[0] = 0x2F; //0x2F:设置，1字节
	Data[1] = 0x00; 
	Data[2] = 0x16; //0x1600:RPDO0-映射 
	Data[3] = 0x00; //0x00:对象映射个数
	Data[4] = 0x01;	//0x01:对象映射个数为1
	Data[5] = 0x00;
	Data[6] = 0x00;
	Data[7] = 0x00;
	for(int i=0;i<8;i++)can_frame->Data[i] = Data[i];
    if(VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1) == 1) print_send_info(can_frame);
	else std::cout << RED <<"--->  CAN Transmit ERROR!  <---" <<  TAIL << std::endl<< std::endl;
	return 0x00;
}

//RPDO1事件触发
//RPDO1映射0x60FF 03（目标速度）
//RPDO1-COB-ID:0x300 + ID
uint8_t WheelMotorCanCtrl::RPDO1_Config(uint8_t ID) //RPDO:接受PDO, 发送或接受均是相对于从节点来说的
{								 //RPDO:从节点(8015D) <-- 主节点(工控机)
    std::cout << std::endl << GREEN << "--->  [CAN Msg] Transmit: RPDO1_Config" << TAIL <<std::endl;
	VCI_CAN_OBJ can_frame[1];
    uint8_t Data[8];							 
	Data[0] = 0x2F; //0x2F:设置，1字节
	Data[1] = 0x01;
	Data[2] = 0x14; //0x1401:RPDO1控制字
	Data[3] = 0x02; //0x02:指定传输类型
	Data[4] = 0xFE; //0xFE:传输类型为事件触发
	Data[5] = 0x00;
	Data[6] = 0x00;
	Data[7] = 0x00;
	can_frame->ID = 0x600+ID;
    can_frame->ExternFlag = false;
    can_frame->RemoteFlag = false;
    can_frame->DataLen = 8; //有效数据长度
    for(char i=0;i<8;i++)can_frame->Data[i] = Data[i];
    if(VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1) == 1) print_send_info(can_frame);
	else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl;


	Data[0] = 0x23; //0x23:设置,4字节
	Data[1] = 0x01; 
	Data[2] = 0x16; //0x1601:RPDO1-映射 
	Data[3] = 0x01; //0x01:RPDO1-映射1
	Data[4] = 0x20; //???
	Data[5] = 0x03; //??? 十进制0x0320=800 0x2003=8195
	Data[6] = 0xFF;
	Data[7] = 0x60; //0x60FF: 映射速度模式时的目标速度；范围：-1000-1000r/min
	 for(char i=0;i<8;i++)can_frame->Data[i] = Data[i];
    if(VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1) == 1) print_send_info(can_frame);
	else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl;//RPDO1映射0x60FF 03
	
	Data[0] = 0x2F; //0x2F:设置，1字节
	Data[1] = 0x01; 
	Data[2] = 0x16; //0x1601:RPDO1-映射 
	Data[3] = 0x00; //0x00:对象映射个数
	Data[4] = 0x01; //0x01:对象映射个数为1
	Data[5] = 0x00;
	Data[6] = 0x00;
	Data[7] = 0x00;
	for(char i=0;i<8;i++)can_frame->Data[i] = Data[i];
    if(VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1) == 1) print_send_info(can_frame);
	else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl;//RPDO1开启1个映射
	return 0x00;
}
//RPDO2事件触发
//RPDO2映射0x605A（快速停止代码）
//RPDO2-COB-ID:0x400 + ID
uint8_t WheelMotorCanCtrl::RPDO2_Config(uint8_t ID) //RPDO:接受PDO, 发送或接受均是相对于从节点来说的
{								 //RPDO:从节点(8015D) <-- 主节点(工控机)
    std::cout << std::endl << GREEN << "--->  [CAN Msg] Transmit: RPDO2_Config" << TAIL <<std::endl;
	VCI_CAN_OBJ can_frame[1];
    uint8_t Data[8];							 
	Data[0] = 0x2F; //0x2F:设置，1字节
	Data[1] = 0x02;
	Data[2] = 0x14; //0x1402:RPDO2控制字
	Data[3] = 0x02; //0x02:指定传输类型
	Data[4] = 0xFE; //0xFE:传输类型为事件触发
	Data[5] = 0x00;
	Data[6] = 0x00;
	Data[7] = 0x00;
	can_frame->ID = 0x600+ID;
    can_frame->ExternFlag = false;
    can_frame->RemoteFlag = false;
    can_frame->DataLen = 8; //有效数据长度
    for(char i=0;i<8;i++)can_frame->Data[i] = Data[i];
    if(VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1) == 1) print_send_info(can_frame);
	else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl;


	Data[0] = 0x23; //0x23:设置,4字节
	Data[1] = 0x02; 
	Data[2] = 0x16; //0x1602:RPDO2-映射 
	Data[3] = 0x01; //0x01:RPDO2-映射1
	Data[4] = 0x00; 
	Data[5] = 0x00; 
	Data[6] = 0x5A;
	Data[7] = 0x60; //0x605A: 快速停止代码 5：正常停止；6：急减速停；7：急停;    均维持 quickstop 状态；
	 for(char i=0;i<8;i++)can_frame->Data[i] = Data[i];
    if(VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1) == 1) print_send_info(can_frame);
	else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl;//RPDO1映射0x60FF 03
	
	Data[0] = 0x2F; //0x2F:设置，1字节
	Data[1] = 0x02; 
	Data[2] = 0x16; //0x1602:RPDO2-映射 
	Data[3] = 0x00; //0x00:对象映射个数
	Data[4] = 0x01; //0x01:对象映射个数为1
	Data[5] = 0x00;
	Data[6] = 0x00;
	Data[7] = 0x00;
	for(char i=0;i<8;i++)can_frame->Data[i] = Data[i];
    if(VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1) == 1) print_send_info(can_frame);
	else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl;//RPDO2开启1个映射
	return 0x00;
}

//TPDO定时器触发
//TPDO0定时器100ms
//TPDO0映射0x606C 03（反馈速度）
//TPDO0-COB-ID:0x180 + ID
uint8_t WheelMotorCanCtrl::TPDO0_Config(uint8_t ID) //TPDO:发送PDO，发送或接受均是相对于从节点来说的
{								 //TPDO:从节点(8015D) --> 主节点(工控机)
    std::cout << std::endl << GREEN << "--->  [CAN Msg] Transmit: TPDO0_Config" << TAIL <<std::endl;
	uint8_t Data[8];							 
	
	VCI_CAN_OBJ can_frame[1];
	Data[0] = 0x2F; // 清空映射
	Data[1] = 0x00;
	Data[2] = 0x1A;
	Data[3] = 0x00;
	Data[4] = 0x00;
	Data[5] = 0x00;
	Data[6] = 0x00;
	Data[7] = 0x00;
	can_frame->ID = 0x600+ID;
    can_frame->ExternFlag = false;
    can_frame->RemoteFlag = false;
    can_frame->DataLen = 8; //有效数据长度
    for(char i=0;i<8;i++)can_frame->Data[i] = Data[i];
    if(VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1) == 1) print_send_info(can_frame);
	else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl;//TPDO0事件触发
	
	Data[0] = 0x2F;
	Data[1] = 0x00;
	Data[2] = 0x18;
	Data[3] = 0x02;
	Data[4] = 0xFF;
	Data[5] = 0x00;
	Data[6] = 0x00;
	Data[7] = 0x00;
	can_frame->ID = 0x600+ID;
    can_frame->ExternFlag = false;
    can_frame->RemoteFlag = false;
    can_frame->DataLen = 8; //有效数据长度
    for(char i=0;i<8;i++)can_frame->Data[i] = Data[i];
    if(VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1) == 1) print_send_info(can_frame);
	else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl;//TPDO0事件触发
	
	Data[0] = 0x2B;
	Data[1] = 0x00;
	Data[2] = 0x18;
	Data[3] = 0x05;
	Data[4] = 0x28;
	Data[5] = 0x00;
	Data[6] = 0x00;
	Data[7] = 0x00;
	for(char i=0;i<8;i++)can_frame->Data[i] = Data[i];
    if(VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1) == 1) print_send_info(can_frame);
	else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl;//TPDO0定时器200*0.5ms

	Data[0] = 0x23;
	Data[1] = 0x00;
	Data[2] = 0x1A;
	Data[3] = 0x01;
	Data[4] = 0x20;
	Data[5] = 0x03;
	Data[6] = 0x6C;
	Data[7] = 0x60;
	for(char i=0;i<8;i++)can_frame->Data[i] = Data[i];
    if(VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1) == 1) print_send_info(can_frame);
	else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl;//TPDO0映射0x606C 03

	Data[0] = 0x23;
	Data[1] = 0x00;
	Data[2] = 0x1A;
	Data[3] = 0x02;
	Data[4] = 0x20;
	Data[5] = 0x00;
	Data[6] = 0x3F;
	Data[7] = 0x60;
	for(char i=0;i<8;i++)can_frame->Data[i] = Data[i];
    if(VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1) == 1) print_send_info(can_frame);
	else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl;
	Data[0] = 0x2F;
	Data[1] = 0x00;
	Data[2] = 0x1A;
	Data[3] = 0x00;
	Data[4] = 0x02;
	Data[5] = 0x00;
	Data[6] = 0x00;
	Data[7] = 0x00;
	for(char i=0;i<8;i++)can_frame->Data[i] = Data[i];
    if(VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1) == 1) print_send_info(can_frame);
	else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl;//TPDO0开启1个映射
	return 0x00;
}
//TPDO1定时器触发
//TPDO1定时器20ms
//TPDO1 01 映射0x6077（反馈实时转矩）
//TPDO1-COB-ID:0x280 + ID
uint8_t WheelMotorCanCtrl::TPDO1_Config(uint8_t ID)
{
    std::cout << std::endl << GREEN << "--->  [CAN Msg] Transmit: TPDO1_Config" << TAIL <<std::endl;
	uint8_t Data[8];							 
	VCI_CAN_OBJ can_frame[1];
	Data[0] = 0x2F; // 清空映射
	Data[1] = 0x01;
	Data[2] = 0x1A;
	Data[3] = 0x00;
	Data[4] = 0x00;
	Data[5] = 0x00;
	Data[6] = 0x00;
	Data[7] = 0x00;
	can_frame->ID = 0x600+ID;
    can_frame->ExternFlag = false;
    can_frame->RemoteFlag = false;
    can_frame->DataLen = 8; //有效数据长度
    for(char i=0;i<8;i++)can_frame->Data[i] = Data[i];
    if(VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1) == 1) print_send_info(can_frame);
	else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl;
	
	Data[0] = 0x23; // 6077 03 至 1A01 01
	Data[1] = 0x01;
	Data[2] = 0x1A;
	Data[3] = 0x01;
	Data[4] = 0x20;
	Data[5] = 0x03;
	Data[6] = 0x77;
	Data[7] = 0x60;
	for(char i=0;i<8;i++)can_frame->Data[i] = Data[i];
    if(VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1) == 1) print_send_info(can_frame);
	else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl;

	Data[0] = 0x23; // 2032 01 至 1A01 02
	Data[1] = 0x01;
	Data[2] = 0x1A;
	Data[3] = 0x02;
	Data[4] = 0x10;
	Data[5] = 0x01;
	Data[6] = 0x32;
	Data[7] = 0x20;
	for(char i=0;i<8;i++)can_frame->Data[i] = Data[i];
    if(VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1) == 1) print_send_info(can_frame);
	else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl;

	Data[0] = 0x23; // 2032 02 至 1A01 03
	Data[1] = 0x01;
	Data[2] = 0x1A;
	Data[3] = 0x03;
	Data[4] = 0x10;
	Data[5] = 0x02;
	Data[6] = 0x32;
	Data[7] = 0x20;
	for(char i=0;i<8;i++)can_frame->Data[i] = Data[i];
    if(VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1) == 1) print_send_info(can_frame);
	else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl;


	Data[0] = 0x2F;
	Data[1] = 0x01;
	Data[2] = 0x18;
	Data[3] = 0x02;
	Data[4] = 0xFF;
	Data[5] = 0x00;
	Data[6] = 0x00;
	Data[7] = 0x00;
	for(char i=0;i<8;i++)can_frame->Data[i] = Data[i];
    if(VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1) == 1) print_send_info(can_frame);
	else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl;//TPDO0映射0x606C 03

	Data[0] = 0x2B;
	Data[1] = 0x01;
	Data[2] = 0x18;
	Data[3] = 0x05;
	Data[4] = 0x28;
	Data[5] = 0x00;
	Data[6] = 0x00;
	Data[7] = 0x00;
	for(char i=0;i<8;i++)can_frame->Data[i] = Data[i];
    if(VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1) == 1) print_send_info(can_frame);
	else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl;
	Data[0] = 0x2F; // 开启映射 多个
	Data[1] = 0x01;
	Data[2] = 0x1A;
	Data[3] = 0x00;
	Data[4] = 0x04;
	Data[5] = 0x00;
	Data[6] = 0x00;
	Data[7] = 0x00;
	for(char i=0;i<8;i++)can_frame->Data[i] = Data[i];
    if(VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1) == 1) print_send_info(can_frame);
	else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl;//TPDO0开启1个映射
	return 0x00;
	
}
//速度模式初始化
uint8_t WheelMotorCanCtrl::Profile_Velocity_Init(uint8_t ID)
{
	std::cout << std::endl << GREEN << "--->  [CAN Msg] Transmit: Profile_Velocity_Init " << TAIL <<std::endl;
    VCI_CAN_OBJ can_frame[1];
	uint8_t Data[8] = {0x2F, 0x60, 0x60, 0x00, 0x03, 0x00, 0x00, 0x00};//设置速度模式
    can_frame->ID = 0x600+ID;
    can_frame->ExternFlag = false;
    can_frame->RemoteFlag = false;
    can_frame->DataLen = 8; //有效数据长度
    for(char i=0;i<8;i++)can_frame->Data[i] = Data[i];
    if(VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1) == 1) print_send_info(can_frame);
	else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl;

	Data[0] = 0x23;
	Data[1] = 0x83;
	Data[2] = 0x60;
	Data[3] = 0x01;
	Data[4] = 0x64;
	Data[5] = 0x00;
	Data[6] = 0x00;
	Data[7] = 0x00;
	for(char i=0;i<8;i++)can_frame->Data[i] = Data[i];
    if(VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1) == 1) print_send_info(can_frame);
	else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl;//设置左电机加速时间100ms

	Data[0] = 0x23;
	Data[1] = 0x83;
	Data[2] = 0x60;
	Data[3] = 0x02;
	Data[4] = 0x64;
	Data[5] = 0x00;
	Data[6] = 0x00;
	Data[7] = 0x00;
	for(char i=0;i<8;i++)can_frame->Data[i] = Data[i];
    if(VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1) == 1) print_send_info(can_frame);
	else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl;//设置右电机加速时间100ms
	
	Data[0] = 0x23;
	Data[1] = 0x84;
	Data[2] = 0x60;
	Data[3] = 0x01;
	Data[4] = 0x64;
	Data[5] = 0x00;
	Data[6] = 0x00;
	Data[7] = 0x00;
	for(char i=0;i<8;i++)can_frame->Data[i] = Data[i];
    if(VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1) == 1) print_send_info(can_frame);
	else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl;//设置左电机减速时间100ms

	Data[0] = 0x23;
	Data[1] = 0x84;
	Data[2] = 0x60;
	Data[3] = 0x02;
	Data[4] = 0x64;
	Data[5] = 0x00;
	Data[6] = 0x00;
	Data[7] = 0x00;
	for(char i=0;i<8;i++)can_frame->Data[i] = Data[i];
    if(VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1) == 1) print_send_info(can_frame);
	else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl;//设置右电机减速时间100ms
/* 
	// 设置左电机最大速度为1000
	Data[0] = 0x2B;   // U16 2字节
	Data[1] = 0x0A;  
	Data[2] = 0x20;   // 0X200A
	Data[3] = 0x01;   // 01左电机 02右电机
	Data[4] = 0xE8;	
	Data[5] = 0x03;   // 0x03E8 = 1000(十进制)
	Data[6] = 0x00;
	Data[7] = 0x00;
	for(char i=0;i<8;i++)can_frame->Data[i] = Data[i];
    if(VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1) == 1) print_send_info(can_frame);
	else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl;

	// 设置右电机最大速度为1000
	Data[0] = 0x2B;   // U16 2字节
	Data[1] = 0x0A;  
	Data[2] = 0x20;   // 0X200A
	Data[3] = 0x02;   // 01左电机 02右电机
	Data[4] = 0xE8;	
	Data[5] = 0x03;   // 0x03E8 = 1000(十进制)
	Data[6] = 0x00;
	Data[7] = 0x00;
	for(char i=0;i<8;i++)can_frame->Data[i] = Data[i];
    if(VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1) == 1) print_send_info(can_frame);
	else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl; 
*/

	return 0x00;
}

//NMT发送
//写入0x01 0x01：开启1号驱动PDO传输
uint8_t WheelMotorCanCtrl::NMT_Control(uint8_t Data0, uint8_t ID)
{
    std::cout << std::endl << GREEN << "--->  [CAN Msg] Transmit: NMT_Control" << TAIL <<std::endl;
    VCI_CAN_OBJ can_frame[1];
	uint8_t Data[2] = {Data0, ID};
    can_frame->ID = 0x000;
    can_frame->ExternFlag = false;
    can_frame->RemoteFlag= false;
    can_frame->DataLen = 2; //有效数据长度
    for(char i=0;i<2;i++)can_frame->Data[i] = Data[i];
    if(VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1) == 1) print_send_info(can_frame);
	else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl;

	return 0x00;
}

//使能电机
uint8_t WheelMotorCanCtrl::Driver_Enable(uint8_t ID)
{
	std::cout << std::endl << GREEN << "--->  [CAN Msg] Transmit: Driver_Enable " << TAIL <<std::endl;
    VCI_CAN_OBJ can_frame[1];
	uint8_t Data_2Byte[2];
	
	Data_2Byte[0] = 0x06;
	Data_2Byte[1] = 0x00;
	can_frame->ID = 0x200+ID;
    can_frame->ExternFlag = false;
    can_frame->RemoteFlag = false;
    can_frame->DataLen = 2; //有效数据长度
    for(char i=0;i<2;i++)can_frame->Data[i] = Data_2Byte[i];
    if(VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1) == 1) print_send_info(can_frame);
	else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl;

	Data_2Byte[0] = 0x07;
	Data_2Byte[1] = 0x00;
    for(char i=0;i<2;i++)can_frame->Data[i] = Data_2Byte[i];
    if(VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1) == 1) print_send_info(can_frame);
	else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl;
	
	Data_2Byte[0] = 0x0F;
	Data_2Byte[1] = 0x00;
	for(char i=0;i<2;i++)can_frame->Data[i] = Data_2Byte[i];
    if(VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1) == 1) print_send_info(can_frame);
	else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl;
    return 0x00;
}

// 设置电机左右最大电流
void WheelMotorCanCtrl::Set_max_current(uint8_t ID)
{
	std::cout << std::endl << RED1 << "--->  [CAN Msg] Transmit: Set motor max current !!! " << TAIL <<std::endl;
	VCI_CAN_OBJ can_frame[1];
	uint8_t Data[8] = {0x2B, 0x15, 0x20, 0x01, 0x2C, 0x01, 0x00, 0x00}; // 设置左电机最大电流15A
    can_frame->ID = 0x600+ID;
    can_frame->ExternFlag = false;
    can_frame->RemoteFlag = false;
    can_frame->DataLen = 8; //有效数据长度
    for(char i=0;i<8;i++)can_frame->Data[i] = Data[i];
    if(VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1) == 1) print_send_info(can_frame);
	else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl;

	Data[3] = 0x02;  													 // 设置右电机最大电流15A
	for(char i=0;i<8;i++)can_frame->Data[i] = Data[i];
    if(VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1) == 1) print_send_info(can_frame);
	else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl;
}

// 失能电机
bool WheelMotorCanCtrl::Driver_Disabled(uint8_t ID)
{
	std::cout << std::endl << RED1 << "--->  [CAN Msg] Transmit: Driver_Disabled !!! " << TAIL <<std::endl;
	VCI_CAN_OBJ can_frame[1];
	uint8_t Data_2Byte[2];
	bool send_success;

	Data_2Byte[0] = 0x06;     //0x07; 07为0x605A的快速停止指令，02为0x6040控制字的急停。。先测试控制字的
	Data_2Byte[1] = 0x00;
	can_frame->ID = 0x200+ID;  //can_frame->ID = 0x400+ID;
    can_frame->ExternFlag = false;
    can_frame->RemoteFlag = false;
    can_frame->DataLen = 2; //有效数据长度
    for(char i=0;i<2;i++)can_frame->Data[i] = Data_2Byte[i];
	send_success = VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1);
    if( send_success== 1) /* print_send_info(can_frame) */;
	else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl;
	
	return send_success;
}
// 急停电机
bool WheelMotorCanCtrl::Quick_Stop(uint8_t ID)
{
	std::cout << std::endl << RED1 << "--->  [CAN Msg] Transmit: Quick_Stop !!! " << TAIL <<std::endl;
	VCI_CAN_OBJ can_frame[1];
	uint8_t Data_2Byte[2];
	bool send_success;

	Data_2Byte[0] = 0x02;     //0x07; 07为0x605A的快速停止指令，02为0x6040控制字的急停。。先测试控制字的
	Data_2Byte[1] = 0x00;
	can_frame->ID = 0x200+ID;  //can_frame->ID = 0x400+ID;
    can_frame->ExternFlag = false;
    can_frame->RemoteFlag = false;
    can_frame->DataLen = 2; //有效数据长度
    for(char i=0;i<2;i++)can_frame->Data[i] = Data_2Byte[i];
	send_success = VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1);
    if( send_success== 1) /* print_send_info(can_frame) */;
	else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl;
	
	return send_success;
}
// 速度模式急停后重新使能
bool WheelMotorCanCtrl::Re_Enabled(uint8_t ID)
{
	std::cout << std::endl << RED1 << "--->  [CAN Msg] Transmit: Re_Enabled !!! " << TAIL <<std::endl;
	VCI_CAN_OBJ can_frame[1];
	uint8_t Data_2Byte[2];
	bool send_success;

	Data_2Byte[0] = 0x06;
	Data_2Byte[1] = 0x00;
	can_frame->ID = 0x200+ID;
    can_frame->ExternFlag = false;
    can_frame->RemoteFlag = false;
    can_frame->DataLen = 2; //有效数据长度
    for(char i=0;i<2;i++)can_frame->Data[i] = Data_2Byte[i];
    if(VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1) == 1) print_send_info(can_frame);
	else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl;
	
	Data_2Byte[0] = 0x07;
	Data_2Byte[1] = 0x00;
    for(char i=0;i<2;i++)can_frame->Data[i] = Data_2Byte[i];
    if(VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1) == 1) print_send_info(can_frame);
	else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl;

	Data_2Byte[0] = 0x0F;     //0x07; 07为0x605A的快速停止指令，02为0x6040控制字的急停。。先测试控制字的
	Data_2Byte[1] = 0x00;
	can_frame->ID = 0x200+ID;  //can_frame->ID = 0x400+ID;
    can_frame->ExternFlag = false;
    can_frame->RemoteFlag = false;
    can_frame->DataLen = 2; //有效数据长度
    for(char i=0;i<2;i++)can_frame->Data[i] = Data_2Byte[i];
	send_success = VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1);
    if( send_success== 1) /* print_send_info(can_frame) */;
	else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl;
	
	return send_success;
}
//速度模式测试
void WheelMotorCanCtrl::Profile_Velocity_Test(uint8_t ID)
{
	std::cout << GREEN << "--->  [CAN Msg] Transmit: Velocity_Test: 100rpm" << TAIL <<std::endl;
    VCI_CAN_OBJ can_frame[1];
	uint8_t Data_4Byte[4];
	Data_4Byte[0] = 0x64;
	Data_4Byte[1] = 0x00;
	Data_4Byte[2] = 0x64;
	Data_4Byte[3] = 0x00;
	can_frame->ID = 0x300+ID;
    can_frame->ExternFlag = false;
    can_frame->RemoteFlag = false;
    can_frame->DataLen = 4; //有效数据长度
    for(char i=0;i<4;i++)can_frame->Data[i] = Data_4Byte[i];
    if(VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1) == 1) print_send_info(can_frame);
	else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl;//同步100rpm
	// Data_4Byte[0] = 0x9C;
	// Data_4Byte[1] = 0xFF;
	// Data_4Byte[2] = 0x9C;
	// Data_4Byte[3] = 0xFF;
	// CAN_Send(0x300 + ID, 0x04, Data_4Byte);//同步-100rpm
	// HAL_Delay(5000);

}
//手柄控制速度参数
bool WheelMotorCanCtrl::Velocity_Joy_Control(uint8_t ID,uint16_t left_v,uint16_t right_v)
{
	// std::cout << GREEN << "--->  [CAN Msg] Transmit: Velocity_Joy_Control " << TAIL <<std::endl;
    VCI_CAN_OBJ can_frame[1];
	bool send_success;
	uint8_t Data_4Byte[4];
	Data_4Byte[0] = left_v & 0xff;
	Data_4Byte[1] = left_v >> 8;
	Data_4Byte[2] = right_v & 0xff;
	Data_4Byte[3] = right_v >> 8;
	can_frame->ID = 0x300+ID;
    can_frame->ExternFlag = false;
    can_frame->RemoteFlag = false;
    can_frame->DataLen = 4; //有效数据长度
    for(char i=0;i<4;i++)can_frame->Data[i] = Data_4Byte[i];
	send_success = VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1);
	if( send_success== 1) /* print_send_info(can_frame) */;
	else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl;

	return send_success;
}

void WheelMotorCanCtrl::Clear_Error_Code(uint8_t ID)
{
	std::cout << std::endl << GREEN << "--->  [CAN Msg] Transmit: Clear_Error_Code " << TAIL <<std::endl;
    VCI_CAN_OBJ can_frame[1];
	uint8_t Data[8] = {0x2B, 0x40, 0x60, 0x00, 0x80, 0x00, 0x00, 0x00};
    can_frame->ID = 0x600+ID;
    can_frame->ExternFlag = false;
    can_frame->RemoteFlag = false;
    can_frame->DataLen = 8; //有效数据长度
    for(char i=0;i<8;i++)can_frame->Data[i] = Data[i];
    if(VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1) == 1) print_send_info(can_frame);
	else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl;
}
void  WheelMotorCanCtrl::Enable_Error_PWM(uint8_t ID)
{
	std::cout << std::endl << GREEN << "--->  [CAN Msg] Transmit: Enable_Error_PWM " << TAIL <<std::endl;
    VCI_CAN_OBJ can_frame[1];
	uint8_t Data[8] = {0x2B, 0x26, 0x20, 0x01, 0x01, 0x00, 0x00, 0x00};
    can_frame->ID = 0x600+ID;
    can_frame->ExternFlag = false;
    can_frame->RemoteFlag = false;
    can_frame->DataLen = 8; //有效数据长度
    for(char i=0;i<8;i++)can_frame->Data[i] = Data[i];
    if(VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1) == 1) print_send_info(can_frame);
	else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl;
}

void  WheelMotorCanCtrl::Set_Overload_param(uint8_t ID)
{
	std::cout << std::endl << GREEN << "--->  [CAN Msg] Transmit: Set_Overload_param " << TAIL <<std::endl;
	usleep(10000);
    VCI_CAN_OBJ can_frame[1];
	uint8_t Data[8] = {0x2B, 0x12, 0x20, 0x01, 0x2c, 0x01, 0x00, 0x00};
    can_frame->ID = 0x600+ID;
    can_frame->ExternFlag = false;
    can_frame->RemoteFlag = false;
    can_frame->DataLen = 8; //有效数据长度
    for(char i=0;i<8;i++)can_frame->Data[i] = Data[i];
    if(VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1) == 1) print_send_info(can_frame);
	else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl;
	usleep(10000);
	Data[3] = 0x02;
    can_frame->ID = 0x600+ID;
    can_frame->ExternFlag = false;
    can_frame->RemoteFlag = false;
    can_frame->DataLen = 8; //有效数据长度
    for(char i=0;i<8;i++)can_frame->Data[i] = Data[i];
    if(VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1) == 1) print_send_info(can_frame);
	else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl;
}
void  WheelMotorCanCtrl::Set_Overload_Time(uint8_t ID)
{
	std::cout << std::endl << GREEN << "--->  [CAN Msg] Transmit: Set_Overload_param " << TAIL <<std::endl;
	usleep(10000);
    VCI_CAN_OBJ can_frame[1];
	uint8_t Data[8] = {0x2B, 0x16, 0x20, 0x01, 0x20, 0x03, 0x00, 0x00};
    can_frame->ID = 0x600+ID;
    can_frame->ExternFlag = false;
    can_frame->RemoteFlag = false;
    can_frame->DataLen = 8; //有效数据长度
    for(char i=0;i<8;i++)can_frame->Data[i] = Data[i];
    if(VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1) == 1) print_send_info(can_frame);
	else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl;
	usleep(10000);
	Data[3] = 0x02;
    can_frame->ID = 0x600+ID;
    can_frame->ExternFlag = false;
    can_frame->RemoteFlag = false;
    can_frame->DataLen = 8; //有效数据长度
    for(char i=0;i<8;i++)can_frame->Data[i] = Data[i];
    if(VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1) == 1) print_send_info(can_frame);
	else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl;
}
void  WheelMotorCanCtrl::Query_Wheel_Param(uint8_t ID)
{
	std::cout << std::endl << GREEN << "--->  [CAN Msg] Transmit: Enable_Error_PWM " << TAIL <<std::endl;
    VCI_CAN_OBJ can_frame[1];
	uint8_t Data[8] = {0x40, 0x0E, 0x20, 0x01, 0X00, 0X00, 0x00, 0x00};
    can_frame->ID = 0x600+ID;
    can_frame->ExternFlag = false;
    can_frame->RemoteFlag = false;
    can_frame->DataLen = 8; //有效数据长度
    for(char i=0;i<8;i++)can_frame->Data[i] = Data[i];
    // if(VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1) == 1) print_send_info(can_frame);
	// else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl; usleep(100000);
	Data[1] = 0x13;
	for(char i=0;i<8;i++)can_frame->Data[i] = Data[i];
    // if(VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1) == 1) print_send_info(can_frame);
	// else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl;usleep(100000);
	Data[1] = 0x14;
	// for(char i=0;i<8;i++)can_frame->Data[i] = Data[i];
    // if(VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1) == 1) print_send_info(can_frame);
	// else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl;usleep(100000);
	Data[1] = 0x15;
	// for(char i=0;i<8;i++)can_frame->Data[i] = Data[i];
    // if(VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1) == 1) print_send_info(can_frame);
	// else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl;usleep(100000);
	Data[1] = 0x16;
	for(char i=0;i<8;i++)can_frame->Data[i] = Data[i];
    if(VCI_Transmit(VCI_USBCAN2, 0, 0, can_frame, 1) == 1) print_send_info(can_frame);
	else std::cout << RED <<"--->  CAN Transmit ERROR!  <---"  <<  TAIL << std::endl <<std::endl;usleep(100000);

}






}
#include <ros/ros.h>
#include <interfaces/Battery.h>
#include <serial/serial.h>
#include <vector>
#include <stdint.h>

/*
 * 工业级串口BMS读取节点
 *
 * 协议结构：
 * DD | CMD | STATUS | LEN | DATA(33字节) | CRC_H | CRC_L | 77
 *
 * 已知：
 * LEN = 0x21 (33)
 * 整帧固定长度 = 40 字节
 */

class BatterySerialNode
{
public:
    BatterySerialNode(ros::NodeHandle& nh)
        : nh_(nh)
    {
        /* ------------ 参数 ------------ */
        nh_.param<std::string>("port", port_, "/dev/ttyUSB0");
        nh_.param<int>("baudrate", baudrate_, 9600);

        /* ------------ 初始化串口 ------------ */
        openSerial();

        /* ------------ 发布器 ------------ */
        pub_ = nh_.advertise<interfaces::Battery>("/battery", 10);

        /* ------------ 查询命令 ------------ */
        request_cmd_ = {0xDD, 0xA5, 0x03, 0x00, 0xFF, 0xFD, 0x77};
    }

    void run()
    {
        ros::Rate rate(10);   // 10Hz

        while (ros::ok())
        {
            if (!ser_.isOpen())
            {
                openSerial();
                rate.sleep();
                continue;
            }

            /* 发送查询命令 */
            ser_.write(request_cmd_);

            /* 读取串口数据 */
            readToBuffer();

            /* 拼帧解析 */
            parseBuffer();

            ros::spinOnce();
            rate.sleep();
        }
    }

private:

    /* ================= 打开串口 ================= */
    void openSerial()
    {
        try
        {
            ser_.setPort(port_);
            ser_.setBaudrate(baudrate_);
            serial::Timeout to(100, 0, 0, 100, 0);
            ser_.setTimeout(to);
            ser_.open();

            if (ser_.isOpen())
                ROS_INFO("Serial opened: %s", port_.c_str());
        }
        catch (std::exception& e)
        {
            ROS_ERROR("Serial open failed: %s", e.what());
        }
    }

    /* ================= 串口读取到缓存 ================= */
    void readToBuffer()
    {
        size_t available = ser_.available();
        if (available > 0)
        {
            std::vector<uint8_t> temp;
            ser_.read(temp, available);
            rx_buffer_.insert(rx_buffer_.end(),
                              temp.begin(),
                              temp.end());
        }
    }

    /* ================= 拼帧状态机 ================= */
    void parseBuffer()
    {
        const size_t FRAME_LEN = 38;   // 固定帧长

        while (true)
        {
            if (rx_buffer_.empty())
                return;

            /* 查找帧头 */
            if (rx_buffer_[0] != 0xDD)
            {
                rx_buffer_.erase(rx_buffer_.begin());
                continue;
            }

            /* 数据不足一整帧，等待 */
            if (rx_buffer_.size() < FRAME_LEN)
                return;

            /* 固定位置检查帧尾 */
            if (rx_buffer_[FRAME_LEN - 1] != 0x77)
            {
                // 当前0xDD不是真正帧头
                rx_buffer_.erase(rx_buffer_.begin());
                continue;
            }

            /* 取出整帧 */
            std::vector<uint8_t> frame(
                rx_buffer_.begin(),
                rx_buffer_.begin() + FRAME_LEN);

            /* CRC校验 */
            if (!checkCRC(frame))
            {
                // CRC错误，同步错误，只删1字节重新同步
                rx_buffer_.erase(rx_buffer_.begin());
                continue;
            }

            /* 解析并发布 */
            parseAndPublish(frame);

            /* 移除整帧 */
            rx_buffer_.erase(
                rx_buffer_.begin(),
                rx_buffer_.begin() + FRAME_LEN);
        }
    }

    /* ================= CRC校验 =================
     * 校验规则：
     * 从 CMD 到 DATA 结束字节求和
     * 取低16位
     */
    bool checkCRC(const std::vector<uint8_t>& frame)
    {
        uint16_t sum = 0;

        for (size_t i = 2; i < frame.size() - 3; ++i)
        {
            sum += frame[i];
        }
        uint16_t result = ((~sum)+1)&0xffff;
        uint16_t received =
            (frame[frame.size()-3] << 8) |
             frame[frame.size()-2];

        return (result == received);
    }

    /* ================= 解析电池数据 ================= */
    void parseAndPublish(const std::vector<uint8_t>& frame)
    {
        /*
         * 根据协议图：
         * 电压: frame[4], frame[5]
         * SOC : frame[23]
         * MOS : frame[24]
         */

        uint16_t voltage_raw =
            (frame[4] << 8) | frame[5];

        float voltage = voltage_raw * 0.01f;  // 10mV → V

        float soc = static_cast<float>(frame[21]);

        uint8_t mos_status = frame[24];
        bool charging = (mos_status & 0x01);

        interfaces::Battery msg;
        msg.voltage = voltage;
        msg.soc = soc;
        msg.charging = charging;

        pub_.publish(msg);
    }

private:
    ros::NodeHandle nh_;
    ros::Publisher pub_;
    serial::Serial ser_;

    std::string port_;
    int baudrate_;

    std::vector<uint8_t> request_cmd_;
    std::vector<uint8_t> rx_buffer_;
};

/* ================= main ================= */
int main(int argc, char** argv)
{
    ros::init(argc, argv, "battery_serial_node");
    ros::NodeHandle nh("~");

    BatterySerialNode node(nh);
    node.run();

    return 0;
}
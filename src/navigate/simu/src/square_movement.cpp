#include <ros/ros.h>
#include <geometry_msgs/Twist.h>

// 线速度和角速度
const double LINEAR_SPEED = 1.0;  // m/s
const double ANGULAR_SPEED = 1.0; // rad/s
// 正方形边长
const double SIDE_LENGTH = 2.0;   // m
// 旋转角度（90度）
const double ROTATION_ANGLE = M_PI / 2;

// 移动指定距离
void moveStraight(ros::Publisher& pub, double distance) {
    geometry_msgs::Twist twist_msg;
    twist_msg.linear.x = (distance > 0)? LINEAR_SPEED : -LINEAR_SPEED;
    twist_msg.angular.z = 0;
    ros::Rate rate(50);
    double time = std::abs(distance) / LINEAR_SPEED;
    ros::Time start_time = ros::Time::now();
    while (ros::ok() && (ros::Time::now() - start_time).toSec() < time) {
        pub.publish(twist_msg);
        rate.sleep();
    }
    // 停止
    twist_msg.linear.x = 0.0;
    pub.publish(twist_msg);
}

// 旋转指定角度
void rotate(ros::Publisher& pub, double angle) {
    geometry_msgs::Twist twist_msg;
    twist_msg.linear.x = 0;
    twist_msg.angular.z = (angle > 0)? ANGULAR_SPEED : -ANGULAR_SPEED;
    ros::Rate rate(50);
    double time = std::abs(angle) / ANGULAR_SPEED;
    ros::Time start_time = ros::Time::now();
    while (ros::ok() && (ros::Time::now() - start_time).toSec() < time) {
        pub.publish(twist_msg);
        rate.sleep();
    }
    // 停止
    twist_msg.angular.z = 0.0;
    pub.publish(twist_msg);
}

int main(int argc, char** argv) {
    ros::init(argc, argv, "square_movement");
    ros::NodeHandle nh;
    ros::Publisher pub = nh.advertise<geometry_msgs::Twist>("/cmd_vel", 10);
    ros::Rate rate(10);

    // 等待发布者连接到订阅者
    while (pub.getNumSubscribers() == 0) {
        rate.sleep();
    }

    // 控制机器人移动
    moveStraight(pub, 3.0);
    moveStraight(pub, -6.0);
    moveStraight(pub, 3.0);

    return 0;
}    
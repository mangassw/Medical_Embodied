#!/usr/bin/env python3 
# -*- coding: utf-8 -*- 

import rospy 
import tf2_ros 
from geometry_msgs.msg import Twist
from std_msgs.msg import String
import tf.transformations as tf_trans 
import math 

class AutoDocking: 
    def __init__(self): 
        rospy.init_node('auto_docking_node', anonymous=True) 
        self.is_running = False
        self.current_state = "IDLE"
        
        # 参数设置 
        self.target_distance = 0.315  # 目标距离：25cm 
        self.horizontal_tolerance = 0.03  # 水平容差：3cm 
        self.angular_tolerance = 0.05  # 角度容差：约3度 
        self.linear_max_speed = 0.1  # 最大线速度 
        self.angular_max_speed = 0.3  # 最大角速度 
        
        # PID控制器参数 
        self.linear_kp = 0.5 
        self.linear_ki = 0.0 
        self.linear_kd = 0.1 
        self.angular_kp = 1.0 
        self.angular_ki = 0.0 
        self.angular_kd = 0.1 
        
        # 新增：PID误差累积上限
        self.linear_error_sum_max = 1.0  # 线性误差累积上限
        self.angular_error_sum_max = 1.0  # 角度误差累积上限
        
        # PID误差累积和上一次误差 
        self.linear_error_sum = 0.0 
        self.angular_error_sum = 0.0 
        self.last_linear_error = 0.0 
        self.last_angular_error = 0.0 
        
        # 状态变量 
        self.is_docking = False 
        self.tag_visible = False 
        self.last_tag_time = rospy.Time.now() 
        self.tag_timeout = rospy.Duration(1.0)  # 1秒没有检测到标签就认为丢失 
        
        # 重试对接相关变量
        self.retry_threshold = 0.5  # 30cm重试阈值
        self.is_retreating = False   # 是否正在后退重试
        self.retreat_start_time = None  # 后退开始时间
        self.retreat_duration = rospy.Duration(3.0)  # 后退持续2秒
        self.retreat_speed = 0.1  # 后退速度
        self.retreat_angular_factor = 1.5  # 重试过程中的角速度调整因子
        self.retreat_lateral_factor = 2  # 重试过程中的横向调整因子
        
        # 创建速度发布器 
        self.cmd_vel_pub = rospy.Publisher('cmd_vel', Twist, queue_size=10) 
        self.state_pub = rospy.Publisher('/docking/state', String, queue_size=10)
        self.control_sub = rospy.Subscriber('/dock/control_cmd', String, self.control_callback)
        
        # 创建TF监听器 
        self.tf_buffer = tf2_ros.Buffer() 
        self.tf_listener = tf2_ros.TransformListener(self.tf_buffer) 
        
        # 创建定时器，控制频率为10Hz 
        self.timer = rospy.Timer(rospy.Duration(0.1), self.control_loop) 
        
        rospy.loginfo("自动对接节点已初始化") 
    
    def control_callback(self, msg):
        command = msg.data.strip().lower()
        if command == "start" and not self.is_running:
            self.start_docking()
            rospy.loginfo("Docking started")
        elif command == "stop":
            self.stop_docking()
            rospy.loginfo("Docking stopped")
        self.publish_state()

    def publish_state(self):
        state_msg = String()
        state_msg.data = self.current_state
        self.state_pub.publish(state_msg)
    
    def get_tag_transform(self): 
        """获取相机与标签之间的变换""" 
        try: 
            # 查询相机到标签的变换 
            transform = self.tf_buffer.lookup_transform('base', 'tag_2', rospy.Time(0), rospy.Duration(0.1)) 
            self.tag_visible = True 
            self.last_tag_time = rospy.Time.now() 
            return transform 
        except (tf2_ros.LookupException, tf2_ros.ConnectivityException, tf2_ros.ExtrapolationException) as e: 
            # 检查标签是否超时 
            if (rospy.Time.now() - self.last_tag_time) > self.tag_timeout: 
                self.tag_visible = False 
            rospy.logwarn_throttle(1.0, "无法获取标签变换: %s" % e) 
            return None 
    
    def start_docking(self): 
        """开始对接过程""" 
        self.is_running = True
        self.is_docking = True 
        self.is_retreating = False
        self.current_state = "RUNNING"
        # 重置PID误差累积
        self.linear_error_sum = 0.0
        self.angular_error_sum = 0.0
        self.last_linear_error = 0.0
        self.last_angular_error = 0.0
        self.publish_state()
        rospy.loginfo("开始自动对接") 
    
    def stop_docking(self): 
        """停止对接过程""" 
        self.is_running = False
        self.is_docking = False 
        self.is_retreating = False
        self.current_state = "IDLE"
        self.send_stop_command()
        self.publish_state()
        rospy.loginfo("停止自动对接") 
    
    def start_retreating(self):
        """开始后退重试过程"""
        self.is_retreating = True
        self.retreat_start_time = rospy.Time.now()
        # 重置PID误差累积
        self.linear_error_sum = 0.0
        self.angular_error_sum = 0.0
        rospy.loginfo("开始前进重试")
    
    def publish_zero_velocity(self): 
        """发布零速度命令""" 
        cmd = Twist() 
        self.cmd_vel_pub.publish(cmd) 

    def send_stop_command(self):
        cmd = Twist()
        for _ in range(10):
            self.cmd_vel_pub.publish(cmd)
            rospy.sleep(0.1)
    
    def control_loop(self, event): 
        """主控制循环""" 
        self.publish_state()
        if not self.is_docking: 
            return 
        
        # 获取标签变换 
        transform = self.get_tag_transform() 
        if not transform or not self.tag_visible: 
            rospy.logwarn_throttle(1.0, "标签不可见，停止移动") 
            self.publish_zero_velocity() 
            return 
        
        # 提取位置和方向信息 
        x = transform.transform.translation.x 
        y = transform.transform.translation.z  # 注意：在相机坐标系中，z轴是深度 
        
        # 计算当前距离和偏移 
        current_distance = y 
        lateral_error = x 
        
        # 计算角度偏差（确保机器人正对标签） 
        q = transform.transform.rotation 
        euler = tf_trans.euler_from_quaternion([q.x, q.y, q.z, q.w]) 
        angular_error = euler[1]  # 取pitch角作为角度误差 
        
        # 如果正在前进重试
        if self.is_retreating:
            # 检查是否前进时间已到
            if (rospy.Time.now() - self.retreat_start_time) > self.retreat_duration:
                self.is_retreating = False
                rospy.loginfo("前进完成，重新开始对接")
                return
            
            # 计算前进过程中的调整控制
            # 使用横向误差和角度误差来调整前进方向，但使用较小的调整因子
            lateral_correction = lateral_error * self.retreat_lateral_factor
            angular_correction = -angular_error * self.retreat_angular_factor
            
            # 限制调整幅度
            lateral_correction = max(-self.angular_max_speed/2, min(self.angular_max_speed/2, lateral_correction))
            angular_correction = max(-self.angular_max_speed/2, min(self.angular_max_speed/2, angular_correction))
            
            # 发送前进命令，同时进行方向调整
            cmd = Twist()
            cmd.linear.x = self.retreat_speed  # 正值表示前进
            cmd.angular.z = lateral_correction + angular_correction  # 组合横向和角度调整
            
            self.cmd_vel_pub.publish(cmd)
            
            # 日志输出
            rospy.loginfo_throttle(1.0, "前进重试中: 距离: %.3f m, 横向误差: %.3f m, 角度误差: %.3f rad, 调整角速度: %.3f rad/s" 
                                  % (current_distance, lateral_error, angular_error, lateral_correction + angular_correction))
            return
        
        # 检查是否需要前进重试
        if (current_distance < self.retry_threshold and 
            (abs(lateral_error) > self.horizontal_tolerance or 
             abs(angular_error) > self.angular_tolerance) and
            not self.is_retreating):
            rospy.loginfo("距离小于30cm但未满足对接条件，开始前进重试")
            self.start_retreating()
            return
        
        # 计算线速度（PID控制） 
        distance_error = current_distance - self.target_distance 
        self.linear_error_sum += distance_error 
        
        # 限制线性误差累积
        self.linear_error_sum = max(-self.linear_error_sum_max, min(self.linear_error_sum_max, self.linear_error_sum))
        
        linear_p = self.linear_kp * distance_error 
        linear_i = self.linear_ki * self.linear_error_sum 
        linear_d = self.linear_kd * (distance_error - self.last_linear_error) 
        linear_velocity = linear_p + linear_i + linear_d 
        self.last_linear_error = distance_error 
        
        # 计算角速度（PID控制） 
        # 组合横向误差和角度误差 
        effective_angular_error = angular_error + math.atan2(lateral_error, current_distance) 
        self.angular_error_sum += effective_angular_error 
        
        # 限制角度误差累积
        self.angular_error_sum = max(-self.angular_error_sum_max, min(self.angular_error_sum_max, self.angular_error_sum))
        
        angular_p = self.angular_kp * effective_angular_error 
        angular_i = self.angular_ki * self.angular_error_sum 
        angular_d = self.angular_kd * (effective_angular_error - self.last_angular_error) 
        angular_velocity = angular_p + angular_i + angular_d 
        self.last_angular_error = effective_angular_error 
        
        # 限制速度 
        linear_velocity = max(-self.linear_max_speed, min(self.linear_max_speed, linear_velocity)) 
        angular_velocity = max(-self.angular_max_speed, min(self.angular_max_speed, angular_velocity)) 
        
        # 检查是否已到达目标位置 
        if (abs(distance_error) < 0.01 and abs(lateral_error) < self.horizontal_tolerance 
                and abs(angular_error) < self.angular_tolerance): 
            rospy.loginfo("已到达目标位置！") 
            self.current_state = "DOCKING_COMPLETED"
            self.publish_state()
            self.send_stop_command()
            self.is_running = False
            self.is_docking = False 
            return 
        else :
            if (abs(distance_error) < 0.02 and abs(lateral_error) < self.horizontal_tolerance 
                    and abs(angular_error) < self.angular_tolerance and linear_velocity<0.01): 
                rospy.loginfo("已到达目标位置！") 
                self.current_state = "DOCKING_COMPLETED"
                self.publish_state()
                self.send_stop_command()
                self.is_running = False
                self.is_docking = False 
                return
        
        # 发布速度命令 
        cmd = Twist() 
        cmd.linear.x = -linear_velocity 
        cmd.angular.z = -angular_velocity 
        self.cmd_vel_pub.publish(cmd) 
        
        # 日志输出（降低频率） 
        rospy.loginfo_throttle(1.0, "距离: %.3f m, 横向误差: %.3f m, 角度误差: %.3f rad, 线速度: %.3f m/s, 角速度: %.3f rad/s, 线性误差累积: %.3f, 角度误差累积: %.3f" 
                              % (current_distance, lateral_error, angular_error, linear_velocity, angular_velocity, self.linear_error_sum, self.angular_error_sum)) 

if __name__ == '__main__': 
    try: 
        docking = AutoDocking() 
        rospy.spin() 
    except rospy.ROSInterruptException: 
        pass

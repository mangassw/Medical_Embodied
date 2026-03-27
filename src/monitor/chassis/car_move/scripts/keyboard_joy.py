#!/usr/bin/env python3

import rospy
import time
from sensor_msgs.msg import Joy
from pynput import keyboard  # 用于监听键盘输入
import threading

# 全局变量，用于存储按键状态
joy_msg = Joy()
joy_msg.axes = [0.0] * 13  # 初始化13个轴
joy_msg.buttons = [0] * 15  # 初始化15个按钮

# 定义加速度相关参数
acceleration = 0.1  # 加速度值
deceleration = 0.15  # 减速度值
max_speed = 1.0  # 最大速度
min_speed = 0.0  # 最小速度
speed_threshold = 0.01  # 认为速度为零的阈值
update_rate = 0.05  # 更新频率 (秒)

# 移动控制状态
forward_pressed = False
backward_pressed = False
current_speed = 0.0
need_publish = False  # 标记是否需要发布消息
final_zero_sent = True  # 标记是否已经发送了最后的零速度消息

# 定义按键映射
key_mapping = {
    keyboard.KeyCode.from_char('w'): ('forward'),          # 前进
    keyboard.KeyCode.from_char('s'): ('backward'),         # 后退
    keyboard.KeyCode.from_char('a'): ('axes', 2, 1.0),     # 右摇杆左
    keyboard.KeyCode.from_char('d'): ('axes', 2, -1.0),    # 右摇杆右
    keyboard.KeyCode.from_char('1'): ('buttons', 0, 1),    # A键
    keyboard.KeyCode.from_char('2'): ('buttons', 1, 1),    # B键
    keyboard.KeyCode.from_char('3'): ('buttons', 2, 1),    # X键
    keyboard.KeyCode.from_char('4'): ('buttons', 3, 1),    # Y键
    keyboard.KeyCode.from_char('5'): ('axes', 6, 1.0),     # 左上扳机键
    keyboard.KeyCode.from_char('6'): ('buttons', 7, 1),    # M_button1
    keyboard.KeyCode.from_char('7'): ('buttons', 8, 1),    # M_button3
    keyboard.KeyCode.from_char('8'): ('buttons', 4, 1),    # L_button
    keyboard.KeyCode.from_char('9'): ('buttons', 5, 1),    # R_button
    keyboard.KeyCode.from_char('0'): ('axes', 11, 1.0),    # 方向键下键
    keyboard.KeyCode.from_char('p'): ('axes', 12, 1.0),    # 右上扳机键
    keyboard.KeyCode.from_char('l'): ('buttons', 13, 1),   # 左摇杆中键
    keyboard.KeyCode.from_char('r'): ('buttons', 14, 1),   # 右摇杆中键
}

# 检查是否有任何控制输入激活
def is_any_control_active():
    # 检查是否有任何按钮被按下
    if 1 in joy_msg.buttons:
        return True
    
    # 检查除了速度轴外是否有其他轴被激活
    for i, value in enumerate(joy_msg.axes):
        if i != 1 and abs(value) > speed_threshold:
            return True
    
    # 检查是否有方向键被按下
    if forward_pressed or backward_pressed:
        return True
    
    return False

# 键盘按下事件回调函数
def on_press(key):
    global forward_pressed, backward_pressed, need_publish, final_zero_sent
    
    if key in key_mapping:
        action = key_mapping[key]
        need_publish = True
        final_zero_sent = False  # 有新的输入，重置标志
        
        if action == 'forward':
            forward_pressed = True
        elif action == 'backward':
            backward_pressed = True
        else:
            data_type, index, value = action
            if data_type == 'axes':
                joy_msg.axes[index] = value
            elif data_type == 'buttons':
                joy_msg.buttons[index] = 1

# 键盘释放事件回调函数
def on_release(key):
    global forward_pressed, backward_pressed, need_publish
    
    if key in key_mapping:
        action = key_mapping[key]
        need_publish = True
        
        if action == 'forward':
            forward_pressed = False
        elif action == 'backward':
            backward_pressed = False
        else:
            data_type, index, _ = action
            if data_type == 'axes':
                joy_msg.axes[index] = 0.0  # 只重置当前松开的轴
            elif data_type == 'buttons':
                joy_msg.buttons[index] = 0  # 只重置当前松开的按钮
    
    # 按下 'q' 键退出
    if key == keyboard.KeyCode.from_char('q'):
        return False  # 停止监听

# 更新速度的线程函数
def speed_updater():
    global current_speed, need_publish, final_zero_sent
    
    while not rospy.is_shutdown():
        old_speed = current_speed
        target_speed = 0.0
        
        # 确定目标速度方向
        if forward_pressed and not backward_pressed:
            target_speed = max_speed
        elif backward_pressed and not forward_pressed:
            target_speed = -max_speed
        
        # 速度变化
        speed_changed = False
        
        # 根据目标速度和当前速度应用加速度或减速度
        if abs(current_speed - target_speed) > speed_threshold:
            if current_speed < target_speed:
                current_speed = min(current_speed + acceleration, target_speed)
                speed_changed = True
            elif current_speed > target_speed:
                current_speed = max(current_speed - deceleration, target_speed)
                speed_changed = True
                # 如果接近零，直接设为零
                if abs(current_speed) < speed_threshold:
                    current_speed = 0.0
        
        # 更新轴值
        joy_msg.axes[1] = current_speed
        
        # 检查控制状态
        any_control_active = is_any_control_active()
        
        # 根据不同情况决定是否发布消息
        if need_publish or speed_changed:
            # 有按键按下或速度正在变化时发布
            if any_control_active or abs(current_speed) > speed_threshold:
                pub.publish(joy_msg)
                need_publish = False
                final_zero_sent = False
            # 当速度已经为0，且需要发送最后一个零速度消息时
            elif not final_zero_sent and abs(current_speed) <= speed_threshold:
                joy_msg.axes[1] = 0.0
                current_speed = 0.0
                pub.publish(joy_msg)
                final_zero_sent = True  # 标记已发送最后的零速度消息
                print("已发送最终零速度消息，停止发布")
                need_publish = False
        
        time.sleep(update_rate)

def publish_joy():
    global pub
    rospy.init_node('keyboard_to_joy', anonymous=True)
    pub = rospy.Publisher('/joy_node_bl/joy', Joy, queue_size=10)

    print("Press keys to simulate joystick input. Press 'q' to quit.")
    print("W/S: 前进/后退 (带加速度)")
    print("消息仅在按键按下或速度变化时发布，速度为0后停止发布")

    # 启动速度更新线程
    speed_thread = threading.Thread(target=speed_updater)
    speed_thread.daemon = True
    speed_thread.start()

    # 启动键盘监听器
    listener = keyboard.Listener(on_press=on_press, on_release=on_release)
    listener.start()

    # 保持 ROS 节点运行
    rospy.spin()

    # 停止监听器
    listener.stop()

if __name__ == '__main__':
    try:
        publish_joy()
    except rospy.ROSInterruptException:
        pass

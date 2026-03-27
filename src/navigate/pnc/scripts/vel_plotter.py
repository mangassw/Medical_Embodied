#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import math
import threading
import time

import rospy
from geometry_msgs.msg import Twist
from nav_msgs.msg import Odometry

try:
    import matplotlib.pyplot as plt
    from matplotlib.animation import FuncAnimation
except Exception as e:
    plt = None
    FuncAnimation = None


class VelPlotter(object):
    def __init__(self):
        self.lock = threading.Lock()
        self.t0 = time.time()
        self.window = rospy.get_param("~window", 15.0)
        self.sample_hz = rospy.get_param("~sample_hz", 20.0)
        self.use_odom = rospy.get_param("~use_odom", True)
        self.cmd_topic = rospy.get_param("~cmd_topic", "/cmd_vel")
        self.odom_topic = rospy.get_param("~odom_topic", "/odom")

        self.ts = []
        self.cmd_v = []
        self.cmd_w = []
        self.odom_v = []
        self.odom_w = []
        self.acc_v = []
        self.acc_w = []

        self.last_v = None
        self.last_w = None
        self.last_t = None

        self.latest_cmd_v = 0.0
        self.latest_cmd_w = 0.0
        self.latest_odom_v = 0.0
        self.latest_odom_w = 0.0

        rospy.Subscriber(self.cmd_topic, Twist, self.cmd_cb, queue_size=50)
        if self.use_odom:
            rospy.Subscriber(self.odom_topic, Odometry, self.odom_cb, queue_size=50)

        self.timer = rospy.Timer(rospy.Duration(1.0 / self.sample_hz), self.sample_cb)

        rospy.loginfo("速度监测启动: cmd_topic=%s odom_topic=%s use_odom=%s sample_hz=%.1f",
                      self.cmd_topic, self.odom_topic, self.use_odom, self.sample_hz)

    def now(self):
        return time.time() - self.t0

    def trim(self):
        # keep only last window seconds
        if not self.ts:
            return
        tmin = self.ts[-1] - self.window
        idx = 0
        for i, t in enumerate(self.ts):
            if t >= tmin:
                idx = i
                break
        if idx > 0:
            self.ts = self.ts[idx:]
            self.cmd_v = self.cmd_v[idx:]
            self.cmd_w = self.cmd_w[idx:]
            self.odom_v = self.odom_v[idx:]
            self.odom_w = self.odom_w[idx:]
            self.acc_v = self.acc_v[idx:]
            self.acc_w = self.acc_w[idx:]

    def cmd_cb(self, msg):
        with self.lock:
            self.latest_cmd_v = msg.linear.x
            self.latest_cmd_w = msg.angular.z

    def odom_cb(self, msg):
        with self.lock:
            self.latest_odom_v = msg.twist.twist.linear.x
            self.latest_odom_w = msg.twist.twist.angular.z

    def sample_cb(self, _event):
        t = self.now()
        with self.lock:
            v = self.latest_cmd_v
            w = self.latest_cmd_w
            self.ts.append(t)
            self.cmd_v.append(v)
            self.cmd_w.append(w)
            if self.use_odom:
                self.odom_v.append(self.latest_odom_v)
                self.odom_w.append(self.latest_odom_w)
            if self.last_t is None:
                self.acc_v.append(0.0)
                self.acc_w.append(0.0)
            else:
                dt = max(t - self.last_t, 1e-3)
                self.acc_v.append((v - self.last_v) / dt)
                self.acc_w.append((w - self.last_w) / dt)
            self.last_t = t
            self.last_v = v
            self.last_w = w
            self.trim()

    def run_plot(self):
        if plt is None:
            rospy.logerr("matplotlib 未安装，无法绘图。请安装 python3-matplotlib")
            return

        fig, axes = plt.subplots(3, 1, figsize=(9, 8), sharex=True)
        ax_v, ax_w, ax_a = axes

        ax_v.set_ylabel("v (m/s)")
        ax_w.set_ylabel("w (rad/s)")
        ax_a.set_ylabel("acc (m/s^2, rad/s^2)")
        ax_a.set_xlabel("t (s)")

        ax_v.set_ylim(-0.5, 0.5)
        ax_w.set_ylim(-0.5, 0.5)
        ax_a.set_ylim(-6, 6)

        line_cmd_v, = ax_v.plot([], [], label="cmd_v")
        line_odom_v, = ax_v.plot([], [], label="odom_v")
        ax_v.legend(loc="upper right")

        line_cmd_w, = ax_w.plot([], [], label="cmd_w")
        line_odom_w, = ax_w.plot([], [], label="odom_w")
        ax_w.legend(loc="upper right")

        line_acc_v, = ax_a.plot([], [], label="acc_v")
        line_acc_w, = ax_a.plot([], [], label="acc_w")
        ax_a.legend(loc="upper right")

        def update(_):
            with self.lock:
                if not self.ts:
                    return line_cmd_v, line_odom_v, line_cmd_w, line_odom_w, line_acc_v, line_acc_w
                t = list(self.ts)
                cmd_v = list(self.cmd_v)
                cmd_w = list(self.cmd_w)
                odom_v = list(self.odom_v) if self.odom_v else [0.0] * len(t)
                odom_w = list(self.odom_w) if self.odom_w else [0.0] * len(t)
                acc_v = list(self.acc_v)
                acc_w = list(self.acc_w)

            line_cmd_v.set_data(t, cmd_v)
            line_odom_v.set_data(t, odom_v)
            line_cmd_w.set_data(t, cmd_w)
            line_odom_w.set_data(t, odom_w)
            line_acc_v.set_data(t, acc_v)
            line_acc_w.set_data(t, acc_w)

            for ax in axes:
                ax.relim()
                ax.autoscale_view()

            return line_cmd_v, line_odom_v, line_cmd_w, line_odom_w, line_acc_v, line_acc_w

        # Keep a reference to avoid animation being garbage-collected.
        self.anim = FuncAnimation(fig, update, interval=100)
        plt.tight_layout()
        plt.show()


def main():
    rospy.init_node("vel_plotter")
    plotter = VelPlotter()
    plotter.run_plot()
    rospy.spin()


if __name__ == "__main__":
    main()

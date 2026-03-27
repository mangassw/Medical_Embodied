# patrol_state_machine_ext 使用说明

## 1. 概述

`patrol_state_machine_ext_node` 是外部可控的巡检状态机节点。

本版控制策略：

- 话题 `~control_cmd` 只保留基础控制（`START/STOP/PAUSE/RESUME/SKIP_CURRENT`）
- 所有带参数、需要明确回执的功能（模式切换、加点、批量加点）统一改为 service
- 每个参数型 service 都返回 `success/message`，可以明确判断节点是否接收成功

## 2. 状态与控制类别（简版）

### 2.1 状态类别

- `IDLE`：空闲/等待任务
- `NAVIGATING`：正在导航
- `PAUSE`：内部过渡态（重试、等待取消）
- `PAUSED`：用户暂停态
- `FAILED`：故障态，等待外部处理

### 2.2 控制类别

- 基础控制（话题或 Trigger）：`START/STOP/PAUSE/RESUME/SKIP_CURRENT`
- 参数控制（仅 service）：`set_mode`、`add_waypoint`、`add_waypoints`

### 2.4 路点约定

- CSV 第一个点（索引 `0`）定义为原点/返航点
- 常规巡检点从第二个点开始（索引 `1...N-1`）
- `ONCE_AND_RETURN` 与 `RETURN_IMMEDIATELY` 会使用索引 `0` 作为回点目标
- `RETURN_IMMEDIATELY` 为强制即时返航：切换后会优先返航（必要时取消当前导航）

### 2.3 `~status` 关键字段

- 任务使能：`task_requested`
- 暂停标志：`user_paused`
- 当前状态：`state`
- 当前目标来源：`current_source`（`BASE/OVERLAY/PREEMPT`）
- 队列长度：`overlay_size`、`preempt_size`
- 重试与循环：`retry`、`loop_count`

## 3. 接口总览

本节点接口按“可靠回执”分层：

- 话题：用于简单、快速的基础控制
- Trigger 服务：用于基础动作的标准调用
- 参数型服务：用于模式切换、加点等需要明确回执的动作

### 3.1 话题

1. `~control_cmd` (`std_msgs/String`)
- 仅支持：`START`、`STOP`、`PAUSE`、`RESUME`、`SKIP_CURRENT`
- 示例：

```bash
rostopic pub -1 /patrol_state_machine_ext/control_cmd std_msgs/String "data: 'START'"
```

2. `~status` (`std_msgs/String`, latched)
- 周期发布状态，格式 `k=v;k=v;...`

### 3.2 基础服务（Trigger）

用于基础控制，调用成功表示请求已被节点接收并执行处理流程。

1. `~start`
2. `~stop`
3. `~pause`
4. `~resume`
5. `~skip_current`
6. `~clear_overlay`
7. `~reload_waypoints`

### 3.3 参数型服务（有明确回执）

用于带参数控制，建议上位机优先使用；返回 `success/message`，其中 `message` 给出执行结果或失败原因。

1. `~set_mode` (`xjrobot_task/PatrolSetMode`)

请求：

- `mode`: `LOOP` / `ONCE_AND_STOP` / `ONCE_AND_RETURN` / `RETURN_IMMEDIATELY`

响应：

- `success`
- `message`

2. `~add_waypoint` (`xjrobot_task/PatrolAddWaypoint`)

请求：

- `pose` (`geometry_msgs/PoseStamped`)
- `policy`：`INSERT_NEXT_ONCE` / `INSERT_NEXT_PERSISTENT` / `APPEND_ONCE` / `APPEND_PERSISTENT`（空字符串则使用参数 `~default_add_policy`）
- `hard_preempt`：`true` 表示硬抢占

响应：

- `success`
- `message`

3. `~add_waypoints` (`xjrobot_task/PatrolAddWaypoints`)

请求：

- `poses` (`geometry_msgs/PoseStamped[]`)
- `policy`：同上
- `hard_preempt_first`：是否让第一点硬抢占

响应：

- `success`
- `message`
- `accepted`：成功接收数量

## 4. 参数说明

参数分为两类：

- 运行参数：导航框架、循环策略、重试和卡住判据
- 接口参数：外部加点默认策略等

兼容原节点核心参数：

- `~action_name`：MBF action 名称
- `~waypoint_file`：基础巡检点文件（csv）
- `~waypoint_frame`：路点坐标系
- `~base_frame`：机器人底盘坐标系
- `~traverse_mode`：基础路线遍历模式（`RETURN_IMMEDIATELY` 为立刻回原点）
- `~single_index`：单点模式索引（`-1` 关闭）
- `~retry_max`：单目标最大重试次数
- `~auto_start`：节点启动后是否自动开始
- `~tick_hz`：状态机调度频率
- `~progress_log_period`：进度日志周期
- `~odom_topic`：里程计输入话题
- `~cmd_vel_topic`：底盘速度输出话题（用于暂停/停止时发布 0 速度）
- `~enable_stuck_replan`：是否启用低速卡住处理
- `~stuck_timeout_sec`：低速持续超时阈值
- `~stuck_linear_vel_eps`：低速线速度阈值
- `~stuck_angular_vel_eps`：低速角速度阈值
- `~force_stop_on_pause`：暂停/停止/切模式取消目标时是否强制发布 0 速度
- `~force_stop_duration_sec`：强制发布 0 速度持续时间（秒）
- `~abnormal_log_file`：异常日志文件路径

扩展参数：

- `~default_add_policy`：`add_waypoint/add_waypoints` 中 `policy` 为空时使用（`INSERT_NEXT_ONCE/INSERT_NEXT_PERSISTENT/APPEND_ONCE/APPEND_PERSISTENT`）

## 5. 使用示例

### 5.1 基础控制（可走话题或 Trigger）

```bash
rostopic pub -1 /patrol_state_machine_ext/control_cmd std_msgs/String "data: 'START'"
rostopic pub -1 /patrol_state_machine_ext/control_cmd std_msgs/String "data: 'PAUSE'"
rostopic pub -1 /patrol_state_machine_ext/control_cmd std_msgs/String "data: 'RESUME'"
rostopic pub -1 /patrol_state_machine_ext/control_cmd std_msgs/String "data: 'SKIP_CURRENT'"
rostopic pub -1 /patrol_state_machine_ext/control_cmd std_msgs/String "data: 'STOP'"
```

或：

```bash
rosservice call /patrol_state_machine_ext/start
rosservice call /patrol_state_machine_ext/pause
rosservice call /patrol_state_machine_ext/resume
rosservice call /patrol_state_machine_ext/skip_current
rosservice call /patrol_state_machine_ext/stop
```

### 5.2 模式切换（service）

```bash
rosservice call /patrol_state_machine_ext/set_mode "mode: 'ONCE_AND_STOP'"
```

立刻返回原点示例：

```bash
rosservice call /patrol_state_machine_ext/set_mode "mode: 'RETURN_IMMEDIATELY'"
```

### 5.3 单点注入（service）

```bash
rosservice call /patrol_state_machine_ext/add_waypoint "
pose:
  header: {frame_id: 'map'}
  pose:
    position: {x: 2.0, y: 1.5, z: 0.0}
    orientation: {x: 0.0, y: 0.0, z: 0.7071, w: 0.7071}
policy: 'INSERT_NEXT_ONCE'
hard_preempt: false
"
```

### 5.4 硬抢占注入（service）

```bash
rosservice call /patrol_state_machine_ext/add_waypoint "
pose:
  header: {frame_id: 'map'}
  pose:
    position: {x: 4.0, y: 4.0, z: 0.0}
    orientation: {x: 0.0, y: 0.0, z: 0.0, w: 1.0}
policy: 'INSERT_NEXT_ONCE'
hard_preempt: true
"
```

`INSERT_NEXT_PERSISTENT` 说明：永久插入到“下一基础目标位”，后续循环持续有效。

### 5.5 批量注入（service）

```bash
rosservice call /patrol_state_machine_ext/add_waypoints "
poses:
- header: {frame_id: 'map'}
  pose:
    position: {x: 1.0, y: 2.0, z: 0.0}
    orientation: {x: 0.0, y: 0.0, z: 0.0, w: 1.0}
- header: {frame_id: 'map'}
  pose:
    position: {x: 2.0, y: 2.0, z: 0.0}
    orientation: {x: 0.0, y: 0.0, z: 0.7071, w: 0.7071}
policy: 'APPEND_ONCE'
hard_preempt_first: false
"
```

### 5.6 查看状态

```bash
rostopic echo /patrol_state_machine_ext/status
```

## 6. 注意事项

1. 如果你在 `control_cmd` 发了 `SET_MODE`、`ADD_WAYPOINT` 等复杂命令，节点会拒绝并打印提示，请改用 service。
2. 服务 `add_waypoint/add_waypoints` 的 `policy` 填错时会返回 `success=false` 和错误信息。
3. `hard_preempt=true` 会取消当前导航，紧急点优先执行。
4. `accepted` 只在 `add_waypoints` 返回，建议你据此做上位机侧重试或补发。

## 7. 运行参考

```bash
cd /home/wu/robo_ws/xj_robot0202
catkin build xjrobot_task
source devel/setup.bash
roslaunch xjrobot_task patrol_state_machine_ext.launch
```

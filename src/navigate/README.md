# navigate 目录说明（行为树与导航桥接）

本文档说明 `src/navigate` 下各功能包的职责，重点描述：
- 如何启动桥接导航
- 行为树如何调用导航接口
- 桥接节点如何把行为树请求转发到巡检状态机外部接口

## 1. 目录与包职责

`src/navigate` 目录当前包含多个导航相关包，核心链路涉及以下几个：

1. `bridge`
- 作用：行为树与导航系统之间的桥接层。
- 关键节点：`navigate_xj_bridge_server`（C++）。
- 关键 launch：`launch/navigate_xj_bridge.launch`。
- 对外提供：`navigate` action（供行为树调用）。
- 对内调用：`xjrobot_task` 的 `patrol_state_machine_ext` 外部服务与状态话题。

2. `task`（包名通常为 `xjrobot_task`）
- 作用：巡检状态机（包含 `patrol_state_machine_ext_node` 外部可控版本）。
- 关键 launch：`launch/patrol_state_machine_ext.launch`。
- 对外提供（由 ext 节点）：
  - 服务：`/patrol_state_machine_ext/start`
  - 服务：`/patrol_state_machine_ext/stop`
  - 服务：`/patrol_state_machine_ext/skip_current`
  - 服务：`/patrol_state_machine_ext/add_waypoint`
  - 话题：`/patrol_state_machine_ext/status`

3. `pnc / loc / slam / simu / thirdparty`
- 作用：底层定位、规划控制、仿真、第三方依赖等。
- 这些包为导航能力提供基础，不直接参与行为树 action 契约层。

## 2. 行为树与导航的接口契约

行为树与桥接层之间统一通过 `interfaces/Navigate.action` 通信：

- Goal
  - `string target`
  - `string nav_type`
- Result
  - `interfaces/ActionStatus status`
  - `string message`
- Feedback
  - `float32 progress`

状态码（`interfaces/ActionStatus.msg`）：
- `OK=0`
- `TIMEOUT=1`
- `PREEMPTED=2`
- `NO_PATH=3`
- `ABORTED=4`

## 3. 启动方式（推荐顺序）

### 3.1 启动巡检状态机 ext

先启动 `patrol_state_machine_ext_node`（必须先有它，桥接才能转发成功）：

```bash
roslaunch xjrobot_task patrol_state_machine_ext.launch
```

### 3.2 启动桥接节点

```bash
roslaunch bridge navigate_xj_bridge.launch
```

> 注意：以你的 `package.xml` 包名为准。
> 如果你的桥接包名不是 `bridge`（例如你改成了 `xjrobot_bridge`），命令应改为：
> `roslaunch xjrobot_bridge navigate_xj_bridge.launch`

### 3.3 启动行为树

再启动行为树运行节点（例如 `ros_bt_runner` 对应 launch）。
行为树中的 `NavgateTo` 节点会连接 action server 名称 `navigate`。

## 4. navigate_xj_bridge.launch 参数说明

文件：`src/navigate/bridge/launch/navigate_xj_bridge.launch`

1. `patrol_ext_ns`（默认 `/patrol_state_machine_ext`）
- ext 节点命名空间。
- 桥接会在该命名空间下访问服务和状态话题。

2. `ext_policy`（默认 `INSERT_NEXT_ONCE`）
- 转发到 `add_waypoint` 的注入策略。

3. `ext_hard_preempt`（默认 `true`）
- 注入 waypoint 时是否硬抢占当前目标。

4. `ext_auto_start`（默认 `true`）
- 每次处理行为树导航请求前，是否先调用 `start` 服务。

5. `wait_service_timeout`（默认 5.0 秒）
- 等待 ext 服务可用的超时时间。

6. `call_timeout`（默认 120.0 秒）
- 单次导航请求从注入到完成判定的总等待超时。

7. `target_map`（rosparam）
- 把行为树的 `target` 字符串映射为 `[x, y, yaw]`。
- 例如：
  - `charge_dock: [0.0, 0.0, 0.0]`
  - `bed_1: [1.0, 1.0, 1.5708]`

## 5. 行为树调用逻辑（端到端时序）

### 5.1 正常导航请求（`nav_type=goal/charge`）

1. 行为树 `NavgateTo` 构造 `NavigateGoal(target, nav_type)` 并发送到 action `navigate`。
2. `navigate_xj_bridge_server` 接收 goal：
- 解析 `target`：优先 `target_map`，其次支持字符串坐标格式 `"x,y"` 或 `"x,y,yaw"`。
- （可选）调用 `/patrol_state_machine_ext/start`。
- 调用 `/patrol_state_machine_ext/add_waypoint` 注入目标。
3. 桥接订阅 `/patrol_state_machine_ext/status` 持续判定进度与完成。
4. 桥接向行为树周期发布 `feedback.progress`。
5. 完成后返回 result：
- 成功：`status=OK`
- 超时：`status=TIMEOUT`
- 其他失败：`status=ABORTED`

### 5.2 停止请求（`nav_type=stop`）

1. 行为树发送 `target=current, nav_type=stop`（或任意 target，stop 语义与 target 无关）。
2. 桥接直接调用 `/patrol_state_machine_ext/stop`。
3. 服务成功：返回 `OK`；服务失败：返回 `ABORTED`。

### 5.3 抢占（行为树分支切换导致 halt）

1. 行为树对 action client 触发 cancel（`onHalted`）。
2. 桥接检测到 preempt 请求后：
- 调用 `/patrol_state_machine_ext/skip_current`
- 调用 `/patrol_state_machine_ext/stop`
3. 向行为树返回 `PREEMPTED`。

## 6. 状态判定与错误返回规则

桥接节点遵循“状态机服务/状态异常即向行为树明确失败”的策略：

1. `add_waypoint` 服务不可达或调用失败
- 返回 `ABORTED`

2. `add_waypoint` 回执 `success=false`
- 返回 `ABORTED`

3. ext 状态话题显示 `state=FAILED`
- 返回 `ABORTED`

4. 超过 `call_timeout`
- 先执行 `skip_current + stop` 收敛
- 返回 `TIMEOUT`

5. 目标无法解析（不在 `target_map` 且不是合法坐标字符串）
- 返回 `NO_PATH`

## 7. 调试与验证命令

### 7.1 看桥接是否起来

```bash
rosnode list | grep navigate_xj_bridge_server
rostopic list | grep /navigate
```

### 7.2 看行为树到桥接 action 是否连通

```bash
rostopic info /navigate/goal
rostopic info /navigate/result
```

### 7.3 看 ext 服务是否可用

```bash
rosservice list | grep /patrol_state_machine_ext
```

至少应看到：
- `/patrol_state_machine_ext/start`
- `/patrol_state_machine_ext/stop`
- `/patrol_state_machine_ext/skip_current`
- `/patrol_state_machine_ext/add_waypoint`

### 7.4 看 ext 状态

```bash
rostopic echo /patrol_state_machine_ext/status
```

### 7.5 手动发送一个导航 goal（不经过行为树）

```bash
rostopic pub -1 /navigate/goal interfaces/NavigateActionGoal "
header:
  seq: 0
  stamp: {secs: 0, nsecs: 0}
  frame_id: ''
goal_id: {stamp: {secs: 0, nsecs: 0}, id: 'manual_test_1'}
goal: {target: 'bed_1', nav_type: 'goal'}
"
```

## 8. 常见问题

1. 报 `service unavailable`
- 先确认 `patrol_state_machine_ext.launch` 已启动。
- 再检查 `patrol_ext_ns` 与真实命名空间一致。

2. 行为树报导航失败但底层在跑
- 检查 `target_map` 是否映射到正确点位。
- 检查 `call_timeout` 是否太短。

3. 包名不一致导致 launch 失败
- 以 `bridge/package.xml` 中 `<name>` 为准修改 `roslaunch <pkg> ...`。

## 9. 最小联调流程（建议）

1. `roslaunch xjrobot_task patrol_state_machine_ext.launch`
2. `roslaunch bridge navigate_xj_bridge.launch`
3. 启动行为树运行器
4. 观察：
- `/navigate/goal` 有请求
- `/patrol_state_machine_ext/add_waypoint` 被调用
- `/navigate/result` 返回 `OK/TIMEOUT/ABORTED`

以上即当前 `navigate` 目录中“行为树 -> 桥接 -> ext 状态机”主链路。

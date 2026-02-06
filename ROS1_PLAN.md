# ROS1 Integration Plan (Revised, Execution-Oriented)

Goal
- Produce a runnable ROS1 mock system that exercises the BT flow end-to-end.
- All interfaces compile, all example nodes run, and BT can tick through main paths.
- Generate a lightweight test log file that states pass/fail for each run.
- BT tick rate is fixed at 20Hz (50ms period) for tests and mock runners.

Scope Decisions
- Keep package set small and pragmatic; split only when it improves clarity or build hygiene.
- Use mock servers/publishers to simulate hardware/LLM/monitoring.
- Focus on "流程跑通": compile, launch, tick, cancel, and exit cleanly.

Packages (proposed)
- `behavior_tree`: BT.CPP runner, XML, BT node plugins, BT tests.
- `interfaces`: msg/srv/action definitions.
- `navigation`: mock Navigate action server + small client example.
- `dialog`: mock LLMInteraction action server + small client example.
- `monitor`: fault + battery mock publishers.
- `patrol`: patrol trigger service + example node (button/timer).
- `charge`: mock docking/charge services.
- `nurse_call`: mock CallNurse action server.

ROS Interfaces (minimal viable)
- Result status enum (shared across actions): `OK / TIMEOUT / PREEMPTED / NO_PATH / ABORTED`.
- `Navigate.action` (goal: target, nav_type; result: status, message; feedback: progress).
- `LLMInteraction.action` (goal: mode, person_id, context; feedback: partial; result: status, summary, need_call_nurse).
- `CallNurse.action` (goal: bed_id; result: status, message).
- `Fault.msg` (`fault_type`, `severity`, `details`).
- `Battery.msg` (`soc`, `charging`, `voltage`).
- `std_msgs/Bool` topic `call_signal` (external call button/trigger).
- `DetectAnomaly.srv` (req: mode, bed_id; res: is_anomaly, details, bed_ids[], urgencies[]).
- `PatrolTrigger.srv` (req: enable; res: ok).
- `Dock.srv` (req: start; res: ok).
- `ChargeUntil.srv` (req: soc_target; res: ok).

BT Node Mapping (high-level)
- `NavgateTo` -> `Navigate.action` client.
- `LLMInteraction` -> `LLMInteraction.action` client (supports cancel).
- `IsBatteryLow`/`IsFault` -> subscribe to `Battery.msg`/`Fault.msg`.
- `patrol_triggered` -> set by `patrol` node (service or topic).
- `CallDutyNurse` -> `CallNurse.action` client.
- Nurse-call decision -> output flag from `LLMInteraction` result; BT reads the flag and decides whether to trigger `CallDutyNurse`.
- `AnomalyDetect` -> unified node:
- Scan mode at patrol point: call `DetectAnomaly.srv` with `mode=scan` to return a bed list sorted by urgency.
- Bed mode near a bed: call `DetectAnomaly.srv` with `mode=bed` to return `is_anomaly` + details.

Mock Behavior Rules (for deterministic tests)
- Navigate action: succeed after N ticks; allow cancel.
- LLM action: stream 2-3 feedback messages, then succeed; allow cancel.
- LLM result includes `need_call_nurse` flag to simulate decision.
- Fault/battery: publishers expose CLI parameters for scenarios (normal/low/fault).
- Charge services: always ok and update battery publisher state.
- DetectAnomaly service:
- Scan mode returns ordered bed_ids (default bed_0, bed_2 with urgencies 2,1).
- Bed mode returns anomaly for bed_0 by default (configurable).
- Unified AnomalyDetect uses `beds_per_patrol` as fallback if scan service unavailable.

Examples (per package)
- Each package ships:
  - one minimal ROS node that implements the mock server/publisher.
  - a launch file to run the example alone.
  - a brief TODO block for real integration points.

Testing Plan (ROS)
- Build: `catkin_make` or `catkin_make --pkg ...`
- Smoke tests:
  - Run each package example in isolation.
  - Run `behavior_tree` with mocks to tick main paths.
- Scenario tests:
  - Normal patrol path completes and returns idle.
  - Low battery during patrol triggers charge and clears trigger.
  - Fault preempts patrol (if patrol_triggered).
  - CallInteraction preempts patrol, but blocked in passive_wakeup/abnormal_alert.
  - LLM sets `need_call_nurse=true` and BT triggers CallDutyNurse; `false` does not trigger.
- Each test appends a short status line to a log file:
  - `tests/ros1_smoke.log` (or under `src/behavior_tree/logs/` if preferred).

Deliverables
- ROS packages + interfaces + example nodes/launch files.
- Updated BT node plugins (clients/subscribers).
- `ros_bt_runner` executable to tick the tree with ROS topics/actions.
- Test log file with pass/fail lines and timestamps.

Notes
- `LLMInteraction` is Action (multi-turn + cancellable).
- `patrol_triggered` is external (button/timer/service).
- After patrol completion or low-battery, clear trigger to return to global IdleWait.

Example nodes live in `scripts/`.
- Server: `rosrun bridge navigate_mock_server.py`
- Client: `rosrun bridge navigate_mock_client.py --target p0 --nav-type goal`

Bridge to xj_robot0202:
- Start bridge server: `roslaunch bridge navigate_xj_bridge.launch`
- It exposes action `navigate` (for behavior_tree).
- It forwards only to `xjrobot_task` external interfaces (`/patrol_state_machine_ext/add_waypoint`, `/start`, `/stop`, `/skip_current`, `/status`).
- Configure target mapping in `navigate_xj_bridge.launch` (`target_map`).

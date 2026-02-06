Handoff Summary (Behavior Tree Work)

Context
- Project: /home/val/BIH_ws/Medical_Embodied
- Behavior tree files live in `src/behavior_tree/BH_xml/`
- Latest changes are in `src/behavior_tree/BH_xml/medical.xml` and `src/behavior_tree/BH_xml/Project_fixed.btproj`
- Test harness: `src/behavior_tree/src/medical_bt_test.cpp`
- Latest test log: `src/behavior_tree/logs/medical_bt_test.log`

Latest Intent (current behavior)
- MainReactiveTree priority order: Fault/low-battery (only if patrol_triggered) -> CallInteraction -> PatrolTriggered -> IdleWait
- patrol_triggered is set externally (button/timer). Not triggered inside IdleWait.
- Low battery during active patrol: go to charge, optionally clear trigger (as per current XML) so patrol stops and returns to idle.
- Normal patrol completion: GoChargeTree -> ClearPatrolTrigger (no local IdleWait; main tree returns to global IdleWait).
- CallInteraction is allowed when not in passive_wakeup/abnormal_alert modes; condition uses a list string in IsInteractionMode.

Key Nodes/Structures (current XML)
- MainReactiveTree:
  - FaultHandling has IsPatrolTriggered gate.
  - LowBatteryHandling: UnfinishedPatrolChargeResume (alert once + charge to threshold) OR NormalLowBatteryCharge (GoChargeTree + ClearPatrolTrigger).
  - CallInteraction uses Inverter + IsInteractionMode mode="[passive_wakeup,abnormal_alert]" to block interruptions.
  - PatrolTriggered -> PatrolTaskTree.
  - IdleWait fallback.
- PatrolTaskTree:
  - LoadPatrolPlan(route_id, cycles)
  - PatrolCompleteToChargeWait: GoChargeTree -> ClearPatrolTrigger
  - PatrolRun: HasRemainingPatrol -> NextPatrolPoint -> NavgateTo -> RequestAnomalyDetection -> BedProcessTree
- BedProcessTree:
  - Per-bed NavgateTo, AnomalyDetect, FaceIdentify + LLMInteraction, optional nurse call enqueue

Implementation Notes
- `IsInteractionMode` in the test harness was extended to parse list strings like "[a,b]".
- A separate nurse-call thread exists: `NurseCallTree` (ReactiveFallback root). It runs in parallel with MainReactiveTree.
- GoChargeTree/AlertChargeFault are referenced; ensure they are defined in XML (they exist in current file).

Latest Test (only newest)
- Ran `catkin_make --pkg behavior_tree`
- Ran `/home/val/BIH_ws/Medical_Embodied/devel/lib/behavior_tree/medical_bt_test`
- Log with Chinese stage annotations: `src/behavior_tree/logs/medical_bt_test.log`

What to verify next
- Confirm behavior around low battery near patrol completion: it should go to charge and clear trigger (so PatrolCompleteToChargeWait should not run afterwards).
- Confirm CallInteraction can preempt patrol but is blocked during passive_wakeup/abnormal_alert.

Example nodes live in `scripts/`.
- Server: `rosrun charge charge_services.py`
- Dock: `rosservice call /dock "start: true"`
- Charge: `rosservice call /charge_until "soc_target: 50"`

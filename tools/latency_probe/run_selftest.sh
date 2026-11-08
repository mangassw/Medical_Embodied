#!/usr/bin/env bash
# run_selftest.sh —— latency_probe 一键自测（ROS_DOMAIN_ID=42 隔离，无需生产节点/声卡）
#
#   bash run_selftest.sh                 # inject + asr 两套自测
#   bash run_selftest.sh --suite inject  # 只测 inject-text 触发
#   bash run_selftest.sh --suite asr -v  # 只测 wake-wav→ASR 触发
#
# 说明：inject 套件覆盖路由/LLM/TTS/统计全流程；asr 套件用 sleep 作为假播放命令，
# 覆盖 wake-wav→kws_detected→/start_asr→ASR 埋点链回溯（无需真实音频/声卡）。
set -eo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ -f /opt/ros/jazzy/setup.bash ]; then
    # shellcheck disable=SC1091
    source /opt/ros/jazzy/setup.bash
else
    echo "[error] 未找到 /opt/ros/jazzy/setup.bash" >&2
    exit 1
fi
if [ -f /home/artrc/medical_ws/setup_env.sh ]; then
    # shellcheck disable=SC1091
    source /home/artrc/medical_ws/setup_env.sh >/dev/null 2>&1 || true
fi

# 自测必须与生产域 0 隔离
if [ "${ROS_DOMAIN_ID:-}" = "" ] || [ "${ROS_DOMAIN_ID}" = "0" ]; then
    echo "[selftest] 设置 ROS_DOMAIN_ID=42（原值 '${ROS_DOMAIN_ID:-}'），与生产域 0 隔离"
    export ROS_DOMAIN_ID=42
fi

CONDA_BASE="${HOME}/miniconda3"
if [ -f "${CONDA_BASE}/etc/profile.d/conda.sh" ]; then
    # shellcheck disable=SC1091
    source "${CONDA_BASE}/etc/profile.d/conda.sh"
fi

cd "$HERE"
exec conda run -n qwen_tts_online --no-capture-output \
    python "${HERE}/fake_chain_test.py" "$@"

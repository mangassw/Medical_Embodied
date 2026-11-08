#!/bin/bash
# 用法: run_e2e_batch.sh <tag_prefix> [每轮间隔秒]
set -o pipefail
PREFIX="$1"; GAP="${2:-24}"
cd /home/artrc/medical_ws/Medical_Embodied_dev
source /opt/ros/jazzy/setup.bash
source /home/artrc/medical_ws/setup_env.sh >/dev/null 2>&1
PY=/home/artrc/miniconda3/envs/qwen_tts_online/bin/python
for spec in u01_first_dialysis u05_needle_water u16_catheter_shower; do
  tag="${PREFIX}_${spec%%_*}"
  echo "######## $tag  ($spec)"
  timeout 240 $PY tools/latency_probe/latency_probe.py \
    --record-dir tools/latency_probe/records --tag "$tag" --repeat 1 \
    --wake-wav tools/latency_probe/audio/wake_xiaoyi.wav \
    --question-wav "tools/latency_probe/audio/${spec}.wav" \
    --play-cmd "pw-play --target latency_sink {wav}" --pre-delay 0.3 \
    --warmup 10 --interval 25 --timeout 120 \
    --endpoint-silence "${ENDPOINT_SILENCE:-1.3}" 2>&1 \
    | grep -vE "ALSA lib|Jack|Cannot connect" \
    | grep -E "round 0 done|ASR: start_asr→endpoint|ASR: nurse_req|ASR: endpoint→probe|LLM TTFT|断句: LLM首块|TTS 首句合成|端到端:|语音结束|会话总时长|音频总时长"
  sleep "$GAP"
done

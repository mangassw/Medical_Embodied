#!/bin/bash
# 启动"精简版"语音链路（ASR→路由→LLM→断句→TTS），用于延迟测量。
# Phase 0 起同时启动本地 RAG 检索服务（tools/rag/rag_service.py，端口 8765），
# 医疗节点默认走 RAG + qwen-plus 直连；RAG_ENABLE=0 可关闭。
#
# 与 launch/launch_llm_nodes.sh 的区别：只启动语音链路必需节点，
# 跳过 UI / 行为树 / dialog_manager 等与本链路耗时无关的节点。
#
# 用法：
#   bash start_stack.sh          # 启动
#   bash start_stack.sh stop     # 停止
#
# 环境变量：
#   SHERPA_MODEL_ROOT   默认 /home/artrc/medical_ws/models/sherpa
#   LATENCY_PROBE       默认 1（打开 /latency_event 埋点）
#   ROS_DOMAIN_ID       默认 0
#   LOG_DIR             默认 /tmp/latency_logs
#   START_ASR           默认 1；=0 时不启动 C++ ASR 节点（只测 LLM+TTS）
set -e

REPO="/home/artrc/medical_ws/Medical_Embodied_dev"
PY_NODES_DIR="$REPO/src/llm_node_py/llm_node_py"
PY="$HOME/miniconda3/envs/qwen_tts_online/bin/python"
LOG_DIR="${LOG_DIR:-/tmp/latency_logs}"
STACK_DIR="/tmp/latency_stack"

export SHERPA_MODEL_ROOT="${SHERPA_MODEL_ROOT:-/home/artrc/medical_ws/models/sherpa}"
export LATENCY_PROBE="${LATENCY_PROBE:-1}"
export PUNCT_MODEL_PATH="${PUNCT_MODEL_PATH:-$SHERPA_MODEL_ROOT/sherpa-onnx-punct-model/model.onnx}"
# Python 节点日志实时刷盘（否则 print() 会缓冲，看不到播放日志）
export PYTHONUNBUFFERED=1
# ~/.local 里若装了另一个 numpy 会遮蔽 conda env 的（2026-10-30 曾因此让 transformers/scipy 导入失败）
export PYTHONNOUSERSITE=1
export ROS_DOMAIN_ID="${ROS_DOMAIN_ID:-0}"
START_ASR="${START_ASR:-1}"

# ---- RAG / 医疗 LLM（Phase 0） ----
export RAG_ENABLE="${RAG_ENABLE:-1}"
export RAG_URL="${RAG_URL:-http://127.0.0.1:8765}"
export RAG_DEVICE="${RAG_DEVICE:-cuda:0}"   # BGE-M3 放 GPU：CPU 化实测每问 +287 ms，显存够就不值得
export RAG_TOP_K="${RAG_TOP_K:-3}"
export RAG_PORT="${RAG_PORT:-8765}"
export MED_LLM_BACKEND="${MED_LLM_BACKEND:-direct}"
# 切到本机 ollama（qwen3:8b）：USE_OLLAMA=1 bash tools/latency_probe/start_stack.sh
if [ "${USE_OLLAMA:-0}" = "1" ]; then
  export MED_LLM_PROVIDER=ollama
  export MED_LLM_MODEL="${MED_LLM_MODEL:-qwen3:8b}"
  export MED_LLM_BASE_URL="${MED_LLM_BASE_URL:-http://127.0.0.1:11434/v1}"
  export MED_INTENT_PROVIDER=ollama
  export MED_INTENT_MODEL="${MED_INTENT_MODEL:-qwen3:8b}"
  export MED_CHAT_MODEL="${MED_CHAT_MODEL:-}"           # 2026-11-05 按用户要求改回单模型：闲聊共用主模型 qwen3:8b（省 ~2.8 GiB 显存）
  # 预热：把主模型装进显存并保持 30 分钟，避免首轮冷启动；设了 MED_CHAT_MODEL 才双模型
  for m in "${MED_LLM_MODEL}" ${MED_CHAT_MODEL:+"${MED_CHAT_MODEL}"}; do
    curl -s -m 180 http://127.0.0.1:11434/api/chat \
      -d '{"model":"'"$m"'","messages":[{"role":"user","content":"hi"}],"stream":false,"think":false,"keep_alive":"30m","options":{"num_predict":1}}' \
      > /dev/null 2>&1 &
  done
fi

export MED_LLM_PROVIDER="${MED_LLM_PROVIDER:-dashscope}"   # dashscope=SDK原生(MaaS部署) | openai=兼容接口
export MED_LLM_MODEL="${MED_LLM_MODEL:-deepseek-v4.1-flash-1017}"
export MED_LLM_BASE_URL="${MED_LLM_BASE_URL:-https://llm-wd6fnilu0sk3blx6.cn-beijing.maas.aliyuncs.com/api/v1}"
export MED_LLM_ENABLE_THINKING="${MED_LLM_ENABLE_THINKING:-0}" # 思考模型必须关，否则首句要等推理
export MED_LLM_TIMEOUT="${MED_LLM_TIMEOUT:-10}"       # 单次 LLM 超时；超时播兜底话术并结束本段
export MED_LLM_MAX_TOKENS="${MED_LLM_MAX_TOKENS:-400}"
export MED_CHAT_ENABLE="${MED_CHAT_ENABLE:-1}"        # 非医学问题正常闲聊回答；0=回到只依据资料
export MED_INTENT_JUDGE="${MED_INTENT_JUDGE:-1}"   # 关键词判闲聊后让快模型复核意图
export MED_INTENT_TIMEOUT="${MED_INTENT_TIMEOUT:-1.2}" # judge 超时，超时按闲聊走
export MED_INTENT_MODEL="${MED_INTENT_MODEL:-qwen-flash}"
export RAG_CHUNK_MAX_CHARS="${RAG_CHUNK_MAX_CHARS:-300}"

# ---- 优化开关（2026-10-29） ----
export NURSE_PARALLEL="${NURSE_PARALLEL:-1}"          # nurse_alert 与主 LLM 并行
export ASR_ENDPOINT_RULE2="${ASR_ENDPOINT_RULE2:-1.2}" # 说完话→判端点的静音阈值（秒）
export FIRST_FRAGMENT_MAX_CHARS="${FIRST_FRAGMENT_MAX_CHARS:-0}"  # 首句提前切分字数，0=关
export SPLITTER_EXTRA_PUNCT="${SPLITTER_EXTRA_PUNCT:-1}"  # 断句扩展标点 ：、！？，0=恢复原规则

# ---------- 依赖环境 ----------
source /opt/ros/jazzy/setup.bash
source /home/artrc/medical_ws/setup_env.sh >/dev/null 2>&1 || true

stop_stack() {
    echo "[stop] killing latency stack processes..."
    if [ -f "$STACK_DIR/pids.txt" ]; then
        while read -r pid; do
            [ -n "$pid" ] && kill "$pid" 2>/dev/null || true
        done < "$STACK_DIR/pids.txt"
    fi
    pkill -f "llm_node_py/ros2_node_" 2>/dev/null || true
    pkill -f "ros_node_keyword_asr" 2>/dev/null || true
    pkill -f "tools/rag/rag_service.py" 2>/dev/null || true
    pkill -f "tools/latency_probe/wake_bridge.py" 2>/dev/null || true
    rm -f "$STACK_DIR/pids.txt"
    sleep 1
    echo "[stop] done."
}

if [ "${1:-start}" = "stop" ]; then
    stop_stack
    exit 0
fi

mkdir -p "$LOG_DIR" "$STACK_DIR"
stop_stack >/dev/null 2>&1 || true

echo "[start] SHERPA_MODEL_ROOT=$SHERPA_MODEL_ROOT"
echo "[start] LATENCY_PROBE=$LATENCY_PROBE ROS_DOMAIN_ID=$ROS_DOMAIN_ID"
echo "[start] logs: $LOG_DIR"

# 需要检查的模型文件（缺失则直接报错退出）
ASR_DIR="$SHERPA_MODEL_ROOT/sherpa-onnx-streaming-zipformer-bilingual-zh-en"
KWS_DIR="$SHERPA_MODEL_ROOT/kws-zipformer-wenetspeech"
PUNCT="$SHERPA_MODEL_ROOT/sherpa-onnx-punct-model/model.onnx"
if [ "$START_ASR" = "1" ]; then
    for f in "$ASR_DIR/encoder-epoch-99-avg-1.onnx" "$ASR_DIR/tokens.txt" \
             "$KWS_DIR/encoder-epoch-12-avg-2-chunk-16-left-64.int8.onnx" \
             "$KWS_DIR/outputMykeywords.txt"; do
        if [ ! -f "$f" ]; then
            echo "[start][ERROR] missing model file: $f"
            exit 1
        fi
    done
fi

# ---------- 启动本地 RAG 检索服务 ----------
RAG_SERVICE="$REPO/tools/rag/rag_service.py"
RAG_PID=""
if [ "$RAG_ENABLE" = "1" ] && [ -f "$RAG_SERVICE" ]; then
    if [ ! -f "$REPO/tools/rag/index/embeddings.npy" ]; then
        echo "[start][WARN] RAG 索引不存在（tools/rag/index/embeddings.npy），跳过 rag_service；"
        echo "               先运行：$PY tools/rag/build_index.py --docs tools/rag/docs --out tools/rag/index --model $HOME/medical_ws/models/bge-m3"
    else
        echo "[start] rag_service"
        nohup setsid "$PY" "$RAG_SERVICE" > "$LOG_DIR/rag_service.log" 2>&1 < /dev/null &
        RAG_PID=$!
        echo $RAG_PID >> "$STACK_DIR/pids.txt"
    fi
elif [ "$RAG_ENABLE" = "1" ]; then
    echo "[start][WARN] 未找到 $RAG_SERVICE，跳过 RAG 服务"
fi

# ---------- 启动 Python 节点 ----------
start_py_node() {
    local name="$1"; shift
    local script="$1"; shift
    echo "[start] $name"
    nohup setsid "$PY" "$script" "$@" > "$LOG_DIR/$name.log" 2>&1 < /dev/null &
    echo $! >> "$STACK_DIR/pids.txt"
}

start_py_node dialog_history   "$PY_NODES_DIR/ros2_node_dialog_history.py"
start_py_node nurse_alert      "$PY_NODES_DIR/ros2_node_nurse_alert.py"
start_py_node question_router  "$PY_NODES_DIR/ros2_node_question_router.py"
start_py_node llm_medical      "$PY_NODES_DIR/ros2_node_llm_medical.py"
start_py_node llm_network      "$PY_NODES_DIR/ros2_node_llm_network.py"
start_py_node stream_text      "$PY_NODES_DIR/ros2_node_stream_text.py"
start_py_node tts_local        "$PY_NODES_DIR/ros2_node_tts_local.py"

# 唤醒桥：精简栈没有行为树，/call_signal -> /start_asr 才能开始收音（真人对话必需）
start_py_node wake_bridge      "$REPO/tools/latency_probe/wake_bridge.py"

# ---------- 启动 C++ ASR 节点 ----------
ASR_BIN="$REPO/install/llm_node_cpp/lib/llm_node_cpp/ros_node_keyword_asr"
if [ "$START_ASR" != "1" ]; then
    echo "[start] START_ASR=0，跳过 C++ ASR 节点"
    ASR_BIN=""
fi
if [ -n "$ASR_BIN" ] && [ -x "$ASR_BIN" ]; then
    echo "[start] asr (C++)"
    nohup setsid "$ASR_BIN" > "$LOG_DIR/asr.log" 2>&1 < /dev/null &
    echo $! >> "$STACK_DIR/pids.txt"
else
    echo "[start][WARN] ASR binary not found: $ASR_BIN（只测 LLM+TTS 时可忽略）"
fi

# ---------- 等待就绪 ----------
wait_for_log() {
    local name="$1" pattern="$2" timeout_s="${3:-120}"
    local i=0
    while [ $i -lt "$timeout_s" ]; do
        if grep -qE "$pattern" "$LOG_DIR/$name.log" 2>/dev/null; then
            echo "[ready] $name (${i}s)"
            return 0
        fi
        sleep 1; i=$((i+1))
    done
    echo "[ready][TIMEOUT] $name 未在 ${timeout_s}s 内就绪，日志尾部："
    tail -15 "$LOG_DIR/$name.log" 2>/dev/null
    return 1
}

wait_for_rag() {
    local timeout_s="${1:-180}"
    local i=0
    if [ -z "$RAG_PID" ]; then
        return 0
    fi
    while [ $i -lt "$timeout_s" ]; do
        if curl -s --max-time 1 "$RAG_URL/health" 2>/dev/null | grep -q '\"ok\"[[:space:]]*:[[:space:]]*true'; then
            echo "[ready] rag_service (${i}s)"
            return 0
        fi
        sleep 1; i=$((i+1))
    done
    echo "[ready][TIMEOUT] rag_service 未在 ${timeout_s}s 内就绪，日志尾部："
    tail -20 "$LOG_DIR/rag_service.log" 2>/dev/null
    return 1
}

wait_for_rag 180
wait_for_log stream_text "Sentence Splitter Node ready" 30
wait_for_log question_router "分流节点启动成功" 30
wait_for_log nurse_alert "Nurse Alert Service is ready" 30
wait_for_log dialog_history "dialog_history" 30 || true
wait_for_log tts_local "模型预热完成" 180
if [ -n "$ASR_BIN" ] && [ -x "$ASR_BIN" ]; then
    wait_for_log asr "ros node keyword asr init done" 120 || true
fi

echo
echo "[start] 节点进程："
cat "$STACK_DIR/pids.txt" 2>/dev/null | while read -r pid; do
    ps -o pid=,cmd= -p "$pid" 2>/dev/null | cut -c1-120
done
echo "[start] 完成。停止：bash $0 stop"

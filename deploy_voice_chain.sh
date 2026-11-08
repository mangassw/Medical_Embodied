#!/usr/bin/env bash
# =====================================================================
# 语音链路（ASR→LLM→TTS）部署脚本 —— 打包 / 安装 / 验证 三合一
#
# 用法：
#   打包（源机）:   ./deploy_voice_chain.sh pack [输出目录]
#                   产出 deploy_bundle_<日期>.tar.zst（源码+模型+运行时+密钥）
#   安装（目标机）: ./deploy_voice_chain.sh install <bundle.tar.zst> [目标根目录]
#                   默认解到 ~/medical_ws，可选 --build 强制 colcon 重编译
#   验证（目标机）: ./deploy_voice_chain.sh verify
#
# 依赖：源机 zstd；目标机 ROS2 Jazzy + conda（env qwen_tts_online 或联网建 env）。
# 注意：TRT 引擎 .plan 与 GPU 绑定，换 GPU 后删除
#   CosyVoice2-0.5B/flow.decoder.estimator.fp16.mygpu.plan 重跑 verify 即重建（78s）。
# =====================================================================
set -euo pipefail

SRC_WS="/home/artrc/medical_ws/Medical_Embodied_dev"
SRC_MODELS="/home/artrc/medical_ws/models"
SRC_RUNTIME="/home/artrc/cosyvoice_runtime"
SRC_KEY="$SRC_WS/src/llm_node_py/key/dashscope.key"
CONDA_PY="/home/artrc/miniconda3/envs/qwen_tts_online/bin/python"

info() { echo "[deploy] $*"; }
die()  { echo "[deploy][错误] $*" >&2; exit 1; }

# ---------------- pack：源机打包 ----------------
do_pack() {
    local out_dir="${1:-/home/artrc/medical_ws}"
    local stamp work
    stamp=$(date +%Y%m%d)
    work=$(mktemp -d "/tmp/deploy_bundle.XXXXXX")
    info "收集资产到 $work ..."

    # 1) 源码（干净树 + install 预编译产物，排除 build/log/.git）
    mkdir -p "$work/bundle"
    tar -cf - -C "$(dirname "$SRC_WS")" \
        --exclude='Medical_Embodied_dev/build' \
        --exclude='Medical_Embodied_dev/log' \
        --exclude='Medical_Embodied_dev/.git' \
        --exclude='*.pyc' --exclude='__pycache__' \
        Medical_Embodied_dev | tar -xf - -C "$work/bundle"

    # 2) 模型与运行时（同文件系统用硬链接暂存，避免 15GB 双份拷贝）
    mkdir -p "$work/bundle/models"
    cp -al "$SRC_MODELS/sherpa" "$work/bundle/models/sherpa"
    cp -al "$SRC_WS/src/llm_node_py/Qwen3-TTS-12Hz-0.6B-CustomVoice" \
          "$work/bundle/models/qwen3_tts"
    cp -al "$SRC_RUNTIME" "$work/bundle/cosyvoice_runtime"

    # 3) 密钥（注意：压缩包含密钥，转发需谨慎）
    mkdir -p "$work/bundle/key"
    cp "$SRC_KEY" "$work/bundle/key/dashscope.key"

    # 4) 清单 + 校验和
    ( cd "$work/bundle" && find . -type f -print0 | sort -z | xargs -0 sha256sum ) \
        > "$work/bundle/MANIFEST.sha256"
    du -sh "$work/bundle" | awk '{print "bundle 大小: " $1}' | tee "$work/bundle/MANIFEST.txt"
    git -C "$SRC_WS" log --oneline -1 >> "$work/bundle/MANIFEST.txt" 2>/dev/null || true

    info "压缩（zstd）..."
    local out="$out_dir/deploy_bundle_$stamp.tar.zst"
    tar -I 'zstd -T0 -3' -cf "$out" -C "$work" bundle
    sha256sum "$out" > "$out.sha256"
    rm -rf "$work"
    info "完成: $out"
    info "校验: $out.sha256"
    info "安装: $0 install $out"
}

# ---------------- install：目标机安装 ----------------
do_install() {
    local bundle="${1:?用法: $0 install <bundle.tar.zst> [目标根目录] [--build]}"
    shift || true
    local dest="$HOME/medical_ws" force_build=0
    while [ $# -gt 0 ]; do
        case "$1" in
            --build) force_build=1 ;;
            *) dest="$1" ;;
        esac
        shift
    done
    [ -f "$bundle" ] || die "找不到压缩包: $bundle"
    info "校验压缩包..."
    [ -f "$bundle.sha256" ] && sha256sum -c "$bundle.sha256" || info "（无 .sha256，跳过校验）"

    info "解压到 $dest ..."
    mkdir -p "$dest"
    tar -I 'zstd -T0' -xf "$bundle" -C "$dest"
    [ -d "$dest/bundle" ] && { mv "$dest/bundle"/* "$dest/"; rmdir "$dest/bundle"; }
    # 归位后布局：<dest>/Medical_Embodied_dev + <dest>/models + <dest>/cosyvoice_runtime + <dest>/key

    # 密钥归位
    if [ -f "$dest/key/dashscope.key" ]; then
        mkdir -p "$dest/Medical_Embodied_dev/src/llm_node_py/key"
        cp "$dest/key/dashscope.key" "$dest/Medical_Embodied_dev/src/llm_node_py/key/"
        chmod 600 "$dest/Medical_Embodied_dev/src/llm_node_py/key/dashscope.key"
    fi

    # conda 环境：有则用，无则联网创建
    if [ ! -x "$HOME/miniconda3/envs/qwen_tts_online/bin/python" ]; then
        info "未找到 conda env qwen_tts_online，尝试创建（需要网络）..."
        source "$(conda info --base)/etc/profile.d/conda.sh"
        conda create -y -n qwen_tts_online python=3.12
        conda activate qwen_tts_online
        pip install -r "$dest/Medical_Embodied_dev/llm_python_requirements.txt"
    fi

    # 编译（install/ 已随包携带时可跳过；--build 强制重编）
    if [ "$force_build" = 1 ] || [ ! -d "$dest/Medical_Embodied_dev/install/llm_node_comm" ]; then
        info "colcon 编译 llm_node_cpp / llm_node_comm ..."
        source /opt/ros/jazzy/setup.bash
        ( cd "$dest/Medical_Embodied_dev" && colcon build \
            --packages-select llm_node_cpp llm_node_comm )
    fi

    # 环境变量文件（每次开终端 source）
    cat > "$dest/voice_env.sh" <<ENV
# 语音链路环境（source 本文件后再启动 launch）
export PYTHONNOUSERSITE=1
export LD_LIBRARY_PATH=\$LD_LIBRARY_PATH:$dest/cosyvoice_runtime/deps/nvidia/cuda_runtime/lib:$dest/cosyvoice_runtime/deps/nvidia/cublas/lib:$dest/cosyvoice_runtime/deps/nvidia/cudnn/lib:$dest/cosyvoice_runtime/deps/tensorrt_cu12/libs
source /opt/ros/jazzy/setup.bash
source "$dest/Medical_Embodied_dev/install/setup.bash"
ENV
    info "环境文件: $dest/voice_env.sh"
    info "安装完成。启动：source $dest/voice_env.sh 后执行"
    info "  cd $dest/Medical_Embodied_dev/launch && ./launch_llm_nodes.sh"
    info "验证：$0 verify"
}

# ---------------- verify：部署验证 ----------------
do_verify() {
    local dest="${1:-$HOME/medical_ws}"
    local ws="$dest/Medical_Embodied_dev"
    local py="$HOME/miniconda3/envs/qwen_tts_online/bin/python"
    [ -x "$py" ] || die "找不到 $py"
    export PYTHONNOUSERSITE=1
    info "1) RAG BM25 自测（必须 Recall@4=100%）"
    ( cd "$ws" && "$py" src/llm_node_py/llm_node_py/rag_bm25.py | tail -1 )
    info "2) 文本规范化自测（11 用例）"
    "$py" "$ws/src/llm_node_py/llm_node_py/tts_text_norm.py" | tail -1
    info "3) 关键文件存在性"
    local f
    for f in src/llm_node_py/Qwen3-TTS-12Hz-0.6B-CustomVoice/config.json \
             src/llm_node_py/key/dashscope.key \
             tools/rag/index/chunks.jsonl \
             install/llm_node_cpp/lib/llm_node_cpp/ros_node_keyword_asr \
             ../cosyvoice_runtime/CosyVoice2-0.5B/llm.pt; do
        [ -e "$ws/$f" ] && info "  OK  $f" || die "  缺失 $f"
    done
    info "4) TRT 引擎检查（换 GPU 后删 .plan 重建）"
    local plan="$dest/cosyvoice_runtime/CosyVoice2-0.5B/flow.decoder.estimator.fp16.mygpu.plan"
    if [ -f "$plan" ]; then
        "$py" - <<PYEOF
import tensorrt as trt
import os, sys
sys.path.insert(0, "$dest/cosyvoice_runtime/deps")
try:
    trt.Runtime(trt.Logger(trt.Logger.WARNING)).deserialize_cuda_engine(open("$plan","rb").read())
    print("  TRT 引擎可加载")
except Exception as e:
    print(f"  [警告] TRT 引擎加载失败（换 GPU？）: {e}")
    print("  修复: rm $plan 后启动节点自动重建（78s）")
PYEOF
    else
        info "  .plan 不存在，首次启用 TTS_LOAD_TRT=1 时将自动构建"
    fi
    info "全部通过 ✓"
}

case "${1:-}" in
    pack)    shift; do_pack "$@" ;;
    install) shift; do_install "$@" ;;
    verify)  shift; do_verify "$@" ;;
    *) sed -n '2,16p' "$0" ;;
esac

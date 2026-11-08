# 语音链路（ASR→LLM→TTS）部署说明 —— 精简版 v9（2026-11-09：13→6 进程）

> 本文是当前生产链路的**唯一有效说明**。历史过程见 git log 与各 note 报告；
> 回归工具：`tools/voice_regression/real_pipeline_record_v7.py`。

## 一、当前链路形态

```
问题语音(注入/麦克风) ──► ros_node_keyword_asr (C++, sherpa 流式 zipformer)
                          · 4 线程；语义端点 0.55s / rule2 1.2s
                          ▼
                    /question_asr
                          ▼
              ros2_node_llm_medical（网关，2026-11 并入 4 个节点）
                          · 入口纠错：typo_map v2 + 句首称呼正则
                          · 分流：医学关键词表 + 意图复核（qwen-flash）→ 医疗/闲聊
                          · RAG：进程内 BM25（rag_bm25.py，零 GPU，~1ms）
                          · LLM：云端 qwen-plus-latest（禁思考，流式）；
                            天气闲聊走 dashscope 联网搜索（原 llm_network 能力）
                          · 内嵌 dialog_history（list+服务，口径不变）
                          · 内嵌 /check_end_dialog 服务（原 check_end_session）
                          ▼
                    /ali_llm_output
                          ▼
              ros2_node_tts_cosyvoice（CosyVoice2-0.5B，权重 fp16，占用 1.6GB）
                          · 内嵌断句器（原 stream_text，按标点子句切分）
                          · tts_text_norm + wetext、填充词、首单元流式
                          · tts_text_norm：符号/单位转口语（=、+、%、3-5、mmol/L）
                          · wetext：阿拉伯数字 → 汉语读法（35→三十五）
                          · 填充词"好的，"：[START] 时 0 延迟播预合成缓存
                          · 首单元流式合成（首块即入播）
                          ▼
                    扬声器（/tts_session_finished 结束信号）
```

实测（回归录音 v8）：说完→听到"好的，" **0.66~1.2s**；说完→正式回答 2.4~3.0s；
数字朗读/符号规范化/纠错全链路生效。
**GPU 占用 ~1.6GB（仅 TTS 单进程）；ASR 4 线程；RAG 纯 CPU。
进程数 13 Python → 6（gateway/tts/nurse_alert/dialog_manager/bt_hci + ASR C++），
retired 节点在 src/llm_node_py/llm_node_py/legacy/（含 UI 文字问答通道，UI 移除后停用）。**

## 二、启动方式

```bash
# 0) 关键环境（必须）：PYTHONNOUSERSITE=1
#    原因：2026-10-30 起 ~/.local 里有 numpy 1.26.4 会遮蔽 conda 环境的 2.4.2，
#    导致 transformers 导入即崩。所有启动脚本/终端都要带。
export PYTHONNOUSERSITE=1

# 1) ROS + 工作空间 overlay（rclpy、llm_node_comm 类型支持）
source /opt/ros/jazzy/setup.bash
source ~/medical_ws/Medical_Embodied_dev/install/setup.bash

# 2) 一键拉起（tmux 三会话，脚本内部用 conda env qwen_tts_online 的 python）
cd ~/medical_ws/Medical_Embodied_dev/launch
./launch_llm_nodes.sh        # = depends_asr_question + depends_ui_question + dialog_manager
```

- 启动后各节点自动预热（TTS 约 3s、ASR 即时），首个真实请求无冷启动。
- **不再需要**：`tools/rag/rag_service.py`（检索已进程内化）；ollama 服务。
- 回归验证（改完任何环节后必跑）：

```bash
PYTHONNOUSERSITE=1 /home/artrc/miniconda3/envs/qwen_tts_online/bin/python \
  src/llm_node_py/llm_node_py/rag_bm25.py          # Recall@4 必须 100%
PYTHONNOUSERSITE=1 /home/artrc/miniconda3/envs/qwen_tts_online/bin/python \
  src/llm_node_py/llm_node_py/tts_text_norm.py     # 11 用例必须全过
PYTHONNOUSERSITE=1 /home/artrc/miniconda3/envs/qwen_tts_online/bin/python \
  tools/voice_regression/real_pipeline_record_v8.py  # 全链路录音回归（CosyVoice2 版）
```

## 三、配置方式（环境变量）

| 变量 | 默认 | 说明 |
| --- | --- | --- |
| `RAG_MODE` | `bm25` | `http` 回退旧 127.0.0.1:8765 服务（排障/对比用，需另起 rag_service） |
| `RAG_INDEX_DIR` | `tools/rag/index` | BM25 索引目录（chunks.jsonl + bm25.json） |
| `RAG_TOP_K` / `RAG_CHUNK_MAX_CHARS` | 4 / 350 | 注入 prompt 的资料条数与每块字数 |
| `TTS_FILLER_TEXT` | `好的，` | 固定首句；置空禁用 |
| `TTS_FP16_WEIGHTS` | 1 | CosyVoice2 权重真 fp16（2946→1600MiB，RTF 0.60→0.44；hift 保持 fp32） |
| `PYTORCH_CUDA_ALLOC_CONF` | expandable_segments:True | 节点内自动设置，勿覆盖 |
| `TTS_SPK_ID` / `TTS_PROMPT_WAV` / `TTS_PROMPT_TEXT` / `TTS_SPEED` | xiaoyi / 项目资产 / — / 1.0 | 克隆音色定义（换音色只需换提示音+文本） |
| `TTS_LOAD_TRT` | 0 | TRT 加速 flow 解码（引擎已缓存 .plan）：RTF 0.44→0.39，显存 +436MiB；速度优先时置 1 |
| `TTS_FLOW_FP16` | TRT 关=1 / TRT 开=0 | flow 编码器权重 fp16（与 TRT 的 dtype 兼容默认值已按实测配好） |
| `MED_LLM_PROVIDER` / `MED_LLM_MODEL` | openai / qwen-plus-latest | 可切 dashscope 原生（deepseek-v4.1-flash-1017）或 ollama（qwen3:8b，降级用） |
| `MED_LLM_ENABLE_THINKING` | 0 | 必须保持 0，否则首句晚数秒 |
| `MED_INTENT_JUDGE` / `MED_INTENT_MODEL` | 1 / qwen-flash | 闲聊关键词复核（云端） |
| `ASR_ENDPOINT_RULE1/2/3` | 2.4 / 1.2 / 20 | rule2 **勿低于 1.2**：0.8s 实测会在句中停顿误截断（详见 asr_stream.cpp 注释） |
| `ASR_SEMANTIC_ENDPOINT` / `ASR_SEMANTIC_RULE` | 开 / 0.55 | 语义端点开关与文本稳定阈值 |
| `LATENCY_PROBE` | 0 | 1 时发布 /latency_event 埋点 |

文档更新流程：改 `tools/rag/docs/*.md` → `build_index.py` 重建 `tools/rag/index/` →
跑 `rag_bm25.py` 自测 → 重启 llm_medical（BM25 索引进程内懒加载）。

## 四、部署方式（换机/机器人）

1. **代码**：`git clone https://github.com/valeriangcz/Medical_Embodied.git`
   （或拷贝工作空间），进入 `Medical_Embodied_dev`：

   ```bash
   source /opt/ros/jazzy/setup.bash
   colcon build --packages-select llm_node_cpp llm_node_comm
   ```

2. **模型资产**（不在 git 里，需单独拷贝）：
   - CosyVoice2 运行时 `/home/artrc/cosyvoice_runtime/`（~12GB：模型 4.9GB + deps
     含 tensorrt-cu12 10.13.3.9 与 CUDA12 运行库；节点自举路径，无需 PYTHONPATH）
   - TRT 引擎缓存 `CosyVoice2-0.5B/flow.decoder.estimator.fp16.mygpu.plan`（170MB，
     GPU 相关，换卡后删掉重建，构建仅 78s）与 `src/llm_node_py/cosyvoice_assets/`（提示音）
   - `src/llm_node_py/Qwen3-TTS-12Hz-0.6B-CustomVoice/`（2.4GB，备用引擎）
   - ASR/KWS sherpa 模型目录（`asr_stream.cpp` 的 `asrBasePath` 指向处，
     含 `hotwords_cn.txt`——注：热词在该模型 BPE 配置下实测无效，保留仅作记录）
   - `tools/rag/index/`（无则在本机跑 `tools/rag/build_index.py` 生成）
3. **Python 环境**：conda env `qwen_tts_online`（依赖清单
   `llm_python_requirements.txt`，torch 2.14+cu130 / transformers 4.57.3 / numpy==2.4.2）；
   **部署后务必 `pip install --user` 的包清点一遍，严禁用户级 numpy 遮蔽**。
4. **密钥**：`src/llm_node_py/key/dashscope.key`（单行文本）。
5. **音频设备**：播放走 PortAudio 默认输出；离线联调用 PipeWire 回环
   （见 `models/audio_setup.md`）。
6. **验收**：跑第二节的三条自测/回归，并人工试听
   `models/testaudio/qa_fullchain_v7_qwen3tts.wav` 对照效果。

## 五、备用路径（当前链路之外的保留项）

- **Qwen3-TTS 回切**：launch 里把 `ros2_node_tts_cosyvoice.py` 换回
  `ros2_node_tts_local.py`（显存 2.6GB，数字朗读原生正确；无克隆能力）。
- **ollama 本地 LLM 降级**：`MED_LLM_PROVIDER=ollama`（qwen3:8b，断网兜底；
  注意资料约束偏松，只建议闲聊路径）。
- `RAG_MODE=http`：旧 GPU 混合检索（BGE-M3），排障时对比用。


## 六、一键部署包与脚本（deploy_voice_chain.sh）

仓库根目录的 `deploy_voice_chain.sh` 提供三个子命令：

### 1) 打包（源机）
```bash
cd ~/medical_ws/Medical_Embodied_dev && ./deploy_voice_chain.sh pack
# 产出 ~/medical_ws/deploy_bundle_<日期>.tar.zst（约 17GB）+ .sha256
# 内容：源码树（含 install 预编译产物）+ models/sherpa + Qwen3-TTS 备用引擎
#       + cosyvoice_runtime（含 TRT 引擎与 CUDA 库）+ rag index + dashscope key
# 注：bge-m3 不再打包（BM25 检索不需要；仅 RAG_MODE=http 回退时自备）
```

### 2) 安装（目标机）
```bash
./deploy_voice_chain.sh install deploy_bundle_<日期>.tar.zst [目标根目录] [--build]
# 自动：校验 sha256 → 解压 → 密钥归位 → conda env 检查/创建 → colcon 编译
#       → 生成 voice_env.sh（PYTHONNOUSERSITE + ROS source + TRT 库路径）
```

### 3) 验证（目标机）
```bash
./deploy_voice_chain.sh verify
# 依次检查：BM25 自测(Recall@4=100%) / 规范化 11 用例 / 关键文件 / TRT 引擎可加载
```

### 4) 启动（目标机）
```bash
source ~/medical_ws/voice_env.sh
cd ~/medical_ws/Medical_Embodied_dev/launch && ./launch_llm_nodes.sh
```

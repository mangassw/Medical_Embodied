# latency_probe —— ROS2 语音全链路延迟测量工具

面向 `Medical_Embodied` 语音交互链路（麦克风 → C++ KWS/ASR → question_router →
LLM 流式 → stream_text 断句 → 本地 Qwen3-TTS → 播放）的延迟探针：单进程同钟采集所有
话题到达时刻，拼接生产节点内的 `/latency_event` 埋点，按“一次提问 = 一轮”输出
独立 JSON、统计 summary、CSV 与 Markdown 报告。

---

## 1. 文件清单

| 文件 | 说明 |
| --- | --- |
| `latency_probe.py` | 探针节点（rclpy）。订阅链路话题 + `/latency_event`，负责触发、采集、逐轮 JSON、统计 summary/CSV/表 |
| `latency_stats.py` | 指标定义与统计/表格/CSV 公共函数（无 ROS 依赖） |
| `report_latency.py` | 读取 summary JSON → 输出 Markdown 报告（分 tag 表 + 端到端透视） |
| `fake_chain_test.py` | 假链路发布器（publish 模式）＋ 一键自测校验（selftest 模式） |
| `run_selftest.sh` | 自测入口：source ROS 环境 + 强制 `ROS_DOMAIN_ID=42` + conda env |
| `records/` | 默认输出目录（run JSON / summary / CSV / 表格） |

---

## 2. 环境与运行

运行前必须同时加载 ROS 2 Jazzy 与工作空间（`setup_env.sh` 会加载 dev/sdk 工作空间），
Python 用 conda 环境 `qwen_tts_online`（该 env 为 Python 3.12，source ROS 后
`PYTHONPATH` 可 import rclpy）：

```bash
source /opt/ros/jazzy/setup.bash
source /home/artrc/medical_ws/setup_env.sh
source /home/artrc/miniconda3/etc/profile.d/conda.sh

# 示例：inject-text 方式测 3 轮
cd /home/artrc/medical_ws/Medical_Embodied_dev/tools/latency_probe
conda run -n qwen_tts_online --no-capture-output python latency_probe.py \
    --record-dir ./records --tag demo --repeat 3 \
    --inject-text "天气今天如何？" --interval 2
```

> 也可直接用解释器绝对路径：`/home/artrc/miniconda3/envs/qwen_tts_online/bin/python latency_probe.py ...`

---

## 3. 触发方式与轮次 t0

| 方式 | CLI | t0（各段延迟起点） | 说明 |
| --- | --- | --- | --- |
| 文本注入 | `--inject-text "..."` | 探针向 `/question_asr` 的**发布时刻** | 绕过 ASR，直接测 router→LLM→TTS→播放；此时不关联 ASR 事件链 |
| 问题音频 | `--question-wav q.wav` | `/question_asr` 的**到达时刻**（ASR 节点发布） | 探针用 `--play-cmd` 播放问题音频并记录 t_play_start/t_play_end |
| 唤醒+问题音频 | `--wake-wav w.wav --question-wav q.wav` | 同问题音频 | 先播唤醒词 → 等 `/latency_event` 的 `kws_detected`（超时 `--kws-timeout`）→ 向 `/start_asr` 发 `Bool true` → `--pre-delay` 后播问题音频 |

- `--play-cmd` 是命令模板，默认 `pw-play --target latency_sink {wav}`；`{wav}` 会替换成 wav 路径。
  模板中无 `{wav}` 时原样执行（便于用 `sleep 1` 之类命令做无声自测）。
- 其它参数：`--record-dir --tag --repeat --interval --timeout --pre-delay
  --endpoint-silence（默认 2.4s）--kws-timeout --start-asr --event-lookback（默认 60s）`。
- `--repeat 0` 表示一直测到 Ctrl-C（已完成的轮次仍会写 summary/CSV）。

### 订阅/发布话题

订阅（`std_msgs/String`，QoS reliable/depth=500）：

```
/question_asr  /llm_to_openai  /llm_to_dashapp  /ali_llm_output
/tts_realtime_data  /tts_session_finished  /latency_event
```

发布：`/question_asr`（inject 注入）、`/start_asr`（Bool，唤醒后启动 ASR）、
`/latency_probe_round`（探针内部轮次标记，供自测使用，生产可忽略）。

---

## 4. `/latency_event` 埋点契约

`std_msgs/String`，`data` 为单行 JSON：

```json
{"node":"asr|tts_local","stage":"<stage>","t_ns":<epoch纳秒整数>,"detail":{...}}
```

| node | stage |
| --- | --- |
| `asr` | `init_done`, `kws_detected`, `start_asr`, `endpoint`, `asr_callback`, `nurse_req`, `nurse_resp`, `publish_question` |
| `tts_local` | `model_warmup`, `rx`, `rx_control`（`detail.control=[START]/[DONE]`）, `synth_start`, `synth_end`（`detail.dur_ms/audio_sec/rtf`）, `play_start`, `play_end`（`detail.dur_ms`）, `session_finished` |

解析是**宽容**的：未知 stage/未知字段原样保留；非 JSON、缺 `t_ns` 不会崩溃
（缺 `t_ns` 时以探针到达时刻兜底）；同一 stage 多次出现全部保留为 list
（`events` / `events_by_stage`，TTS 每句一组）。

**关键设计（ASR 事件早于 `/question_asr` 到达）**：探针常驻缓存全部 `/latency_event`。
question-wav / wake-wav 模式每轮结束时按 `--event-lookback`（默认 60s）回溯：
取该轮触发前最近的一条 `asr.publish_question` 为锚点，向前回溯窗口内各 stage 最近事件，
拼出本轮 ASR 链 `endpoint → asr_callback → nurse_req → nurse_resp → publish_question`，
避免把上一轮事件算进来。inject-text 模式不关联 ASR 链（问题由探针直接注入）。

---

## 5. 每轮 JSON（`records/<tag>_roundNN.json`）

单位约定：**时间戳 epoch 纳秒**（字段后缀 `_ns`）；**延迟毫秒**（字段后缀 `_ms`）。

主要字段：

| 字段 | 内容 |
| --- | --- |
| `status` | `ok` / `timeout` / `kws_timeout` / `no_t0` |
| `trigger` | 触发方式、t0 来源、注入文本/wav、每次播放的 cmd/start/end/错误 |
| `timestamps_ns` | `t0`、触发时刻、`probe_rx_ns`（各话题到达）、`node_event_ns`（各埋点 t_ns） |
| `segments_ms` / `segments_ns` | 全部派生段（见下） |
| `derived` | Lead 约定命名的派生指标，每项含 `ns` + `ms`（`audio_total_sec` 另含 `sec`） |
| `asr_chain` / `asr_chain_meta` | 回溯得到的 ASR 事件链与锚点/是否找到 publish_question |
| `events` / `events_by_stage` | 原始埋点（含 `source=round|event_cache`、`arrival_ns`、`raw`） |
| `streams` | LLM 分块（时间+文本）、TTS 分句文本列表（便于核对断句） |
| `counts` | 事件数、播放配对数、忽略的旧消息计数 |

### 派生指标（`segments_ms`，同值有 `_ns` 版本）

- **ASR**（节点内 `t_ns` 拼接）：`asr_kws_to_start_asr`、`asr_start_asr_to_endpoint`、
  `asr_endpoint_to_nurse_req`、`asr_nurse_req_to_nurse_resp`（= `derived.nurse_dur`）、
  `asr_nurse_resp_to_publish_question`、`asr_endpoint_to_question_rx`、`asr_publish_question_to_rx`
- **路由/LLM/断句**（probe 同钟到达）：`question_to_router`、`router_to_llm_first`、
  `question_to_llm_first`（LLM TTFT）、`llm_first_to_done`、`question_to_llm_done`、
  `llm_first_to_tts_first_text`、`question_to_tts_first_text`、`first_tts_text_to_first_play_start`
- **TTS**：`tts_rx_to_synth_end`（首句合成）、`tts_rx_to_play_start`（首音）、
  `tts_synth_end_to_play_start`
- **端到端/会话**：`question_to_first_audio`（t0→首个 `play_start`）、
  `speech_end_to_first_audio`（`endpoint - endpoint_silence` → 首个 `play_start`）、
  `playback_end_to_first_audio`（问句 wav 播放结束 → 首个 `play_start`）、
  `question_to_session_finished`、`audio_total_sum`（各段 `play_start→play_end` 之和）、
  `audio_first_to_last`（首个 `play_start` → 末个 `play_end`）

`derived` 中与 Lead 对接的别名：`asr_endpoint_to_nurse_req`、`nurse_dur`、
`nurse_resp_to_question`、`question_to_first_llm_chunk`、`first_llm_chunk_to_done`、
`first_llm_chunk_to_first_tts_text`、`first_tts_text_to_first_play_start`、
`question_to_first_play_start`、`speech_end_to_first_play_start`、
`question_to_session_finished`、`audio_total_sec`。

---

## 6. 统计输出

全部轮次结束后 stdout 打印 markdown 表，并生成：

- `records/<tag>_summary.json`：`{count, mean_ms, median_ms, p95_ms, min_ms, max_ms, stdev_ms}`
  （每指标一项）+ 每轮摘要 + 配置
- `records/<tag>_stats.csv`：指标统计（Excel/pandas 直接读）
- `records/<tag>_rounds.csv`：每轮一行、每指标一列的宽表
- `records/<tag>_stats.md`：markdown 统计表
- `records/<tag>_roundNN.json`：每轮明细

统计口径：count 为有效轮次数；median 偶数取均值；p95 为 nearest-rank；
stdev 为样本标准差。缺失数据的指标（未埋点/未触发）不会出现在表里，并在末尾列出。

`report_latency.py` 把 summary 渲染成报告（分 tag 明细 + 端到端透视）：

```bash
python report_latency.py --record-dir ./records            # 打印
python report_latency.py --record-dir ./records --tags demo,prod \
    --out records/report_latency.md                        # 写文件
python report_latency.py records/demo_summary.json         # 直接指定 summary
```

---

## 7. 自测（不需要生产节点/声卡，强制 ROS_DOMAIN_ID=42）

```bash
cd /home/artrc/medical_ws/Medical_Embodied_dev/tools/latency_probe
bash run_selftest.sh                  # inject + asr 两套，默认各 3 轮
bash run_selftest.sh --suite inject -v
bash run_selftest.sh --suite asr --repeat 2
```

自测内容：

- **inject 套件**：假链路按理论时序发布
  `/llm_to_dashapp`(+30ms) → `/ali_llm_output` 首块(+150ms)/每 80ms 一块/[DONE](+600ms)
  → `/tts_realtime_data`(+200ms 断句) → TTS 埋点 `rx/synth_start/synth_end(+800ms)/
  play_start(+1200ms)/play_end(+1200ms)` → `/tts_session_finished`；
  校验 router/TTFT/LLM/断句/首音/端到端/会话总时长/音频总时长等数值与统计文件。
- **asr 套件**：`--wake-wav` 触发，假链路模拟 `kws_detected → start_asr → endpoint
  → nurse_req(+50ms) → nurse_resp(+300ms) → publish_question → /question_asr`，
  校验 ASR 段、`speech_end_to_first_audio`、ASR 链回溯（`publish_question_found=true`）。
- 每个数值与理论值逐项比对，打印 PASS/FAIL 表；退出码 0=全部通过。

手动跑假链路（调试用）：

```bash
export ROS_DOMAIN_ID=42
python fake_chain_test.py --mode publish --trigger inject
# 另一个终端跑探针 --inject-text ...
```

---

## 8. 生产实测示例（域 0，与生产节点同域）

```bash
source /opt/ros/jazzy/setup.bash && source /home/artrc/medical_ws/setup_env.sh
cd /home/artrc/medical_ws/Medical_Embodied_dev/tools/latency_probe

# 方式一：文本注入（快速验证 router/LLM/TTS/首音，不经过 ASR）
conda run -n qwen_tts_online --no-capture-output python latency_probe.py \
    --record-dir records --tag prod_inject --repeat 5 \
    --inject-text "天气今天如何？" --interval 3

# 方式二：真实语音链路（唤醒 → ASR → 回答），需要 latency_sink 虚拟 sink
conda run -n qwen_tts_online --no-capture-output python latency_probe.py \
    --record-dir records --tag prod_voice --repeat 5 \
    --wake-wav /path/to/wake.wav --question-wav /tmp/tts_local_test.wav \
    --pre-delay 0.3 --interval 5

python report_latency.py --record-dir records --out records/report_latency.md
```

注意：

- 生产域为 `ROS_DOMAIN_ID=0`（默认），**不要**为生产实测设置 42；自测脚本会自动隔离到 42。
- 探针节点名唯一（`latency_probe_<pid>`），不会与生产节点冲突。
- 探针不启动/不修改任何生产节点；只订阅话题、可选发布 `/question_asr`、`/start_asr`。

---

## 9. 已知限制

1. **TTS 首音/端到端依赖 `/latency_event` 的 `tts_local.play_start`**：无该埋点时只能测到
   LLM 文本与 `/tts_session_finished`，`question_to_first_audio` 为 null（JSON 中明确为 null，
   不会伪造数值）。
2. `audio_total_sum` 用各段 `play_start→play_end` 配对求和；若 `play_end` 事件缺失则退化为
   `末 play_end - 首 play_start` 不可用（null）。
3. `speech_end_to_first_audio` 用 `endpoint - --endpoint-silence` 近似“人声结束时刻”；
   ASR 端点规则若调整（默认 2.4s），需同步改 `--endpoint-silence`。
4. `playback_end_to_first_audio` 中 t_play_end 为播放子进程退出时刻（含音频缓冲/进程退出开销），
   不是最后一个采样点写入声卡的时刻，误差约数十 ms。
5. 跨节点拼接假设同一台机器、`time.time_ns()` 与节点埋点 `t_ns` 都是 epoch 纳秒（已核实）。
6. ASR 链回溯为启发式：要求本轮存在 `asr.publish_question` 事件；若埋点缺失会退回按触发时刻
   关联（`asr_chain_meta.publish_question_found=false` 可判）。
7. 同一 tag 重复运行会覆盖同名文件；不同批次请用不同 `--tag` 或不同 `--record-dir`。

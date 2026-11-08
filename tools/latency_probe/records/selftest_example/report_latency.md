# 语音全链路延迟测量报告

- 生成时间：2026-10-27T15:38:57
- 测量工具：latency_probe（单进程同钟 + /latency_event 埋点拼接）
- 报告 tag 数：2

> 表中数值单位均为 **毫秒(ms)**；`mean (p95)` 表示均值与 95 分位。端到端透视按 tag 汇总每轮 mean/p95。

## 端到端透视

| tag | mode | n | LLM TTFT | LLM 总时长 | first_llm_chunk_to_first_tts_text | 断句→首音 | 端到端(首音) | 语音结束→首音 | 会话总时长 | 音频总时长 |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| selftest_asr | wake-wav | 3 | 149.6 (150.1) | 599.9 (600.0) | 201.1 | 1203.5 (1203.9) | 1554.1 (1554.7) | 4262.0 (4262.0) | 3404.5 (3405.2) | 2400.0 (2400.0) |
| selftest_inject | inject-text | 3 | 151.1 (151.9) | 599.7 (600.1) | 199.6 | 1204.6 (1204.7) | 1555.4 (1555.4) | — | 3405.8 (3405.8) | 2400.0 (2400.0) |

## tag: `selftest_asr`（mode=wake-wav, n=3, ok=3）

- summary: `/home/artrc/medical_ws/Medical_Embodied_dev/tools/latency_probe/records/selftest_example/selftest_asr_summary.json`
- 触发参数：inject_text=None, question_wav='/tmp/fake_question.wav', wake_wav='/tmp/fake_wake.wav'
- endpoint 静音阈值：2.4s；事件回溯窗口：60.0s；轮间隔：0.8s

| 阶段(stage) | n | mean | median | p95 | min | max | stdev |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| ASR: kws_detected→start_asr | 3 | 0.9 | 0.9 | 1.0 | 0.9 | 1.0 | 0.1 |
| ASR: start_asr→endpoint | 3 | 800.0 | 800.0 | 800.0 | 800.0 | 800.0 | 0.0 |
| ASR: endpoint→nurse_req | 3 | 50.0 | 50.0 | 50.0 | 50.0 | 50.0 | 0.0 |
| ASR: nurse_req→nurse_resp | 3 | 250.0 | 250.0 | 250.0 | 250.0 | 250.0 | 0.0 |
| ASR: nurse_resp→publish_question | 3 | 5.0 | 5.0 | 5.0 | 5.0 | 5.0 | 0.0 |
| ASR: endpoint→probe收到question | 3 | 307.9 | 308.0 | 308.4 | 307.3 | 308.4 | 0.6 |
| ASR: publish_question→probe收到 | 3 | 2.9 | 3.0 | 3.4 | 2.3 | 3.4 | 0.6 |
| Router: question→路由话题到达 | 3 | 29.7 | 29.8 | 30.2 | 29.1 | 30.2 | 0.6 |
| LLM: 路由话题→首块 | 3 | 119.9 | 120.0 | 120.1 | 119.7 | 120.1 | 0.2 |
| LLM TTFT: question→首块 | 3 | 149.6 | 149.4 | 150.1 | 149.2 | 150.1 | 0.5 |
| LLM 总时长: 首块→[DONE] | 3 | 599.9 | 599.9 | 600.0 | 599.9 | 600.0 | 0.1 |
| LLM: question→[DONE] | 3 | 749.5 | 749.4 | 750.2 | 749.1 | 750.2 | 0.6 |
| 断句: LLM首块→首个TTS文本 | 3 | 201.1 | 200.8 | 201.9 | 200.5 | 201.9 | 0.7 |
| 断句: question→首个TTS文本 | 3 | 350.6 | 350.2 | 352.0 | 349.7 | 352.0 | 1.2 |
| 断句: 首个TTS文本→首音 | 3 | 1203.5 | 1203.8 | 1203.9 | 1202.7 | 1203.9 | 0.7 |
| TTS 首句合成: rx→synth_end | 3 | 800.0 | 800.0 | 800.0 | 800.0 | 800.0 | 0.0 |
| TTS 首音: rx→play_start | 3 | 1200.0 | 1200.0 | 1200.0 | 1200.0 | 1200.0 | 0.0 |
| TTS 首句: synth_end→play_start | 3 | 400.0 | 400.0 | 400.0 | 400.0 | 400.0 | 0.0 |
| 端到端: question→首个play_start | 3 | 1554.1 | 1554.0 | 1554.7 | 1553.6 | 1554.7 | 0.6 |
| 语音结束→首音 (endpoint-静音阈值) | 3 | 4262.0 | 4262.0 | 4262.0 | 4262.0 | 4262.0 | 0.0 |
| 问句播放结束→首音 (wav触发) | 3 | 2205.5 | 2205.0 | 2207.8 | 2203.8 | 2207.8 | 2.0 |
| 会话总时长: question→session_finished | 3 | 3404.5 | 3404.4 | 3405.2 | 3404.0 | 3405.2 | 0.6 |
| 音频总时长(各段play相加) | 3 | 2400.0 | 2400.0 | 2400.0 | 2400.0 | 2400.0 | 0.0 |
| 音频跨距: 首play_start→末play_end | 3 | 1800.0 | 1800.0 | 1800.0 | 1800.0 | 1800.0 | 0.0 |

<details><summary>每轮端到端（question→首音 / 会话总时长 / 状态）</summary>

| round | t0_source | question→首音(ms) | 语音结束→首音(ms) | 会话总时长(ms) | status |
| ---: | --- | ---: | ---: | ---: | --- |
| 0 | question_asr_rx | 1553.6 | 4262.0 | 3404.0 | ok |
| 1 | question_asr_rx | 1554.0 | 4262.0 | 3404.4 | ok |
| 2 | question_asr_rx | 1554.7 | 4262.0 | 3405.2 | ok |

</details>

## tag: `selftest_inject`（mode=inject-text, n=3, ok=3）

- summary: `/home/artrc/medical_ws/Medical_Embodied_dev/tools/latency_probe/records/selftest_example/selftest_inject_summary.json`
- 触发参数：inject_text='病人血压偏高怎么办？', question_wav=None, wake_wav=None
- endpoint 静音阈值：2.4s；事件回溯窗口：60.0s；轮间隔：0.8s

| 阶段(stage) | n | mean | median | p95 | min | max | stdev |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Router: question→路由话题到达 | 3 | 30.8 | 30.8 | 30.8 | 30.7 | 30.8 | 0.0 |
| LLM: 路由话题→首块 | 3 | 120.4 | 120.0 | 121.1 | 120.0 | 121.1 | 0.6 |
| LLM TTFT: question→首块 | 3 | 151.1 | 150.8 | 151.9 | 150.7 | 151.9 | 0.7 |
| LLM 总时长: 首块→[DONE] | 3 | 599.7 | 600.0 | 600.1 | 598.9 | 600.1 | 0.7 |
| LLM: question→[DONE] | 3 | 750.8 | 750.8 | 750.8 | 750.8 | 750.8 | 0.0 |
| 断句: LLM首块→首个TTS文本 | 3 | 199.6 | 200.0 | 200.0 | 198.8 | 200.0 | 0.7 |
| 断句: question→首个TTS文本 | 3 | 350.7 | 350.7 | 350.7 | 350.7 | 350.7 | 0.0 |
| 断句: 首个TTS文本→首音 | 3 | 1204.6 | 1204.7 | 1204.7 | 1204.6 | 1204.7 | 0.0 |
| TTS 首句合成: rx→synth_end | 3 | 800.0 | 800.0 | 800.0 | 800.0 | 800.0 | 0.0 |
| TTS 首音: rx→play_start | 3 | 1200.0 | 1200.0 | 1200.0 | 1200.0 | 1200.0 | 0.0 |
| TTS 首句: synth_end→play_start | 3 | 400.0 | 400.0 | 400.0 | 400.0 | 400.0 | 0.0 |
| 端到端: question→首个play_start | 3 | 1555.4 | 1555.4 | 1555.4 | 1555.3 | 1555.4 | 0.0 |
| 会话总时长: question→session_finished | 3 | 3405.8 | 3405.8 | 3405.8 | 3405.7 | 3405.8 | 0.0 |
| 音频总时长(各段play相加) | 3 | 2400.0 | 2400.0 | 2400.0 | 2400.0 | 2400.0 | 0.0 |
| 音频跨距: 首play_start→末play_end | 3 | 1800.0 | 1800.0 | 1800.0 | 1800.0 | 1800.0 | 0.0 |

> 未采集到数据的指标（通常表示该链路环节未带埋点/未触发）：ASR: kws_detected→start_asr、ASR: start_asr→endpoint、ASR: endpoint→nurse_req、ASR: nurse_req→nurse_resp、ASR: nurse_resp→publish_question、ASR: endpoint→probe收到question、ASR: publish_question→probe收到、语音结束→首音 (endpoint-静音阈值)、问句播放结束→首音 (wav触发)


<details><summary>每轮端到端（question→首音 / 会话总时长 / 状态）</summary>

| round | t0_source | question→首音(ms) | 语音结束→首音(ms) | 会话总时长(ms) | status |
| ---: | --- | ---: | ---: | ---: | --- |
| 0 | publish_question | 1555.4 | — | 3405.8 | ok |
| 1 | publish_question | 1555.4 | — | 3405.8 | ok |
| 2 | publish_question | 1555.3 | — | 3405.7 | ok |

</details>


---

### 指标说明

- `asr_kws_to_start_asr_ms`：ASR: kws_detected→start_asr
- `asr_start_asr_to_endpoint_ms`：ASR: start_asr→endpoint
- `asr_endpoint_to_nurse_req_ms`：ASR: endpoint→nurse_req
- `asr_nurse_req_to_nurse_resp_ms`：ASR: nurse_req→nurse_resp
- `asr_nurse_resp_to_publish_question_ms`：ASR: nurse_resp→publish_question
- `asr_endpoint_to_question_rx_ms`：ASR: endpoint→probe收到question
- `asr_publish_question_to_rx_ms`：ASR: publish_question→probe收到
- `question_to_router_ms`：Router: question→路由话题到达
- `router_to_llm_first_ms`：LLM: 路由话题→首块
- `question_to_llm_first_ms`：LLM TTFT: question→首块
- `llm_first_to_done_ms`：LLM 总时长: 首块→[DONE]
- `question_to_llm_done_ms`：LLM: question→[DONE]
- `llm_first_to_tts_first_text_ms`：断句: LLM首块→首个TTS文本
- `question_to_tts_first_text_ms`：断句: question→首个TTS文本
- `first_tts_text_to_first_play_start_ms`：断句: 首个TTS文本→首音
- `tts_rx_to_synth_end_ms`：TTS 首句合成: rx→synth_end
- `tts_rx_to_play_start_ms`：TTS 首音: rx→play_start
- `tts_synth_end_to_play_start_ms`：TTS 首句: synth_end→play_start
- `question_to_first_audio_ms`：端到端: question→首个play_start
- `speech_end_to_first_audio_ms`：语音结束→首音 (endpoint-静音阈值)
- `playback_end_to_first_audio_ms`：问句播放结束→首音 (wav触发)
- `question_to_session_finished_ms`：会话总时长: question→session_finished
- `audio_total_sum_ms`：音频总时长(各段play相加)
- `audio_first_to_last_ms`：音频跨距: 首play_start→末play_end


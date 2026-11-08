# 语音全链路延迟测量报告

- 生成时间：2026-10-27T15:54:38
- 测量工具：latency_probe（单进程同钟 + /latency_event 埋点拼接）
- 报告 tag 数：4

> 表中数值单位均为 **毫秒(ms)**；`mean (p95)` 表示均值与 95 分位。端到端透视按 tag 汇总每轮 mean/p95。

## 端到端透视

| tag | mode | n | LLM TTFT | LLM 总时长 | first_llm_chunk_to_first_tts_text | 断句→首音 | 端到端(首音) | 语音结束→首音 | 会话总时长 | 音频总时长 |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| inj_weather | inject-text | 3 | 914.2 (1289.5) | 1124.5 (1148.6) | 72.2 | 936.7 (1017.2) | 1923.1 (2374.6) | — | 17576.2 (18113.0) | 15645.8 (15729.3) |
| inj_medical | inject-text | 3 | 6687.4 (7077.1) | 149.5 (378.9) | 11.6 | 1120.4 (1139.1) | 7819.3 (8174.2) | — | 14651.3 (22801.8) | 6829.8 (14623.1) |
| voice_weather | wake-wav | 3 | 1103.4 (1560.7) | 1359.1 (1531.6) | 133.2 | 1122.0 (1224.1) | 2358.5 (3031.0) | 5341.0 (5806.3) | 20412.4 (20832.4) | 18043.8 (18844.9) |
| voice_medical | wake-wav | 2 | 8258.3 (10925.2) | 940.6 (1388.2) | 43.7 | 1145.5 (1208.1) | 9447.4 (12220.3) | 12379.7 (14772.6) | 35693.5 (53655.2) | 26236.6 (41420.3) |

## tag: `inj_weather`（mode=inject-text, n=3, ok=3）

- summary: `/home/artrc/medical_ws/Medical_Embodied_dev/tools/latency_probe/records/inj_weather_summary.json`
- 触发参数：inject_text='天气今天如何？', question_wav=None, wake_wav=None
- endpoint 静音阈值：2.4s；事件回溯窗口：60.0s；轮间隔：3.0s

| 阶段(stage) | n | mean | median | p95 | min | max | stdev |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Router: question→路由话题到达 | 3 | 1.2 | 1.2 | 1.4 | 0.9 | 1.4 | 0.3 |
| LLM: 路由话题→首块 | 3 | 913.0 | 739.0 | 1288.1 | 712.0 | 1288.1 | 325.1 |
| LLM TTFT: question→首块 | 3 | 914.2 | 740.2 | 1289.5 | 712.9 | 1289.5 | 325.3 |
| LLM 总时长: 首块→[DONE] | 3 | 1124.5 | 1127.7 | 1148.6 | 1097.2 | 1148.6 | 25.8 |
| LLM: question→[DONE] | 3 | 2038.7 | 1867.9 | 2438.1 | 1810.1 | 2438.1 | 347.1 |
| 断句: LLM首块→首个TTS文本 | 3 | 72.2 | 67.9 | 98.4 | 50.2 | 98.4 | 24.4 |
| 断句: question→首个TTS文本 | 3 | 986.4 | 838.6 | 1357.4 | 763.1 | 1357.4 | 323.5 |
| 断句: 首个TTS文本→首音 | 3 | 936.7 | 916.3 | 1017.2 | 876.6 | 1017.2 | 72.5 |
| TTS 首句合成: rx→synth_end | 3 | 863.1 | 872.8 | 925.1 | 791.3 | 925.1 | 67.4 |
| TTS 首音: rx→play_start | 3 | 935.7 | 913.8 | 1016.9 | 876.5 | 1016.9 | 72.7 |
| TTS 首句: synth_end→play_start | 3 | 72.6 | 85.1 | 91.8 | 41.0 | 91.8 | 27.6 |
| 端到端: question→首个play_start | 3 | 1923.1 | 1754.9 | 2374.6 | 1639.8 | 2374.6 | 395.2 |
| 会话总时长: question→session_finished | 3 | 17576.2 | 17411.2 | 18113.0 | 17204.4 | 18113.0 | 476.2 |
| 音频总时长(各段play相加) | 3 | 15645.8 | 15650.1 | 15729.3 | 15558.2 | 15729.3 | 85.7 |
| 音频跨距: 首play_start→末play_end | 3 | 15651.6 | 15655.3 | 15736.1 | 15563.3 | 15736.1 | 86.5 |

> 未采集到数据的指标（通常表示该链路环节未带埋点/未触发）：ASR: kws_detected→start_asr、ASR: start_asr→endpoint、ASR: endpoint→nurse_req、ASR: nurse_req→nurse_resp、ASR: nurse_resp→publish_question、ASR: endpoint→probe收到question、ASR: publish_question→probe收到、语音结束→首音 (endpoint-静音阈值)、问句播放结束→首音 (wav触发)


<details><summary>每轮端到端（question→首音 / 会话总时长 / 状态）</summary>

| round | t0_source | question→首音(ms) | 语音结束→首音(ms) | 会话总时长(ms) | status |
| ---: | --- | ---: | ---: | ---: | --- |
| 0 | publish_question | 2374.6 | — | 18113.0 | ok |
| 1 | publish_question | 1754.9 | — | 17411.2 | ok |
| 2 | publish_question | 1639.8 | — | 17204.4 | ok |

</details>

## tag: `inj_medical`（mode=inject-text, n=3, ok=3）

- summary: `/home/artrc/medical_ws/Medical_Embodied_dev/tools/latency_probe/records/inj_medical_summary.json`
- 触发参数：inject_text='肾病患者运动时有哪些注意事项？', question_wav=None, wake_wav=None
- endpoint 静音阈值：2.4s；事件回溯窗口：60.0s；轮间隔：3.0s

| 阶段(stage) | n | mean | median | p95 | min | max | stdev |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Router: question→路由话题到达 | 3 | 1.3 | 1.2 | 1.5 | 1.1 | 1.5 | 0.2 |
| LLM: 路由话题→首块 | 3 | 6686.1 | 6680.8 | 7076.0 | 6301.5 | 7076.0 | 387.3 |
| LLM TTFT: question→首块 | 3 | 6687.4 | 6682.3 | 7077.1 | 6302.7 | 7077.1 | 387.2 |
| LLM 总时长: 首块→[DONE] | 3 | 149.5 | 68.9 | 378.9 | 0.5 | 378.9 | 201.7 |
| LLM: question→[DONE] | 3 | 6836.8 | 6682.8 | 7456.1 | 6371.6 | 7456.1 | 558.4 |
| 断句: LLM首块→首个TTS文本 | 3 | 11.6 | 0.4 | 34.1 | 0.2 | 34.1 | 19.6 |
| 断句: question→首个TTS文本 | 3 | 6698.9 | 6682.4 | 7077.5 | 6336.8 | 7077.5 | 370.6 |
| 断句: 首个TTS文本→首音 | 3 | 1120.4 | 1125.5 | 1139.1 | 1096.7 | 1139.1 | 21.6 |
| TTS 首句合成: rx→synth_end | 3 | 1083.5 | 1102.2 | 1119.3 | 1029.0 | 1119.3 | 48.0 |
| TTS 首音: rx→play_start | 3 | 1120.2 | 1125.3 | 1138.7 | 1096.6 | 1138.7 | 21.5 |
| TTS 首句: synth_end→play_start | 3 | 36.7 | 23.1 | 67.6 | 19.5 | 67.6 | 26.8 |
| 端到端: question→首个play_start | 3 | 7819.3 | 7821.5 | 8174.2 | 7462.3 | 8174.2 | 355.9 |
| 会话总时长: question→session_finished | 3 | 14651.3 | 10938.4 | 22801.8 | 10213.9 | 22801.8 | 7067.8 |
| 音频总时长(各段play相加) | 3 | 6829.8 | 3115.9 | 14623.1 | 2750.3 | 14623.1 | 6751.7 |
| 音频跨距: 首play_start→末play_end | 3 | 6830.9 | 3115.9 | 14626.4 | 2750.3 | 14626.4 | 6753.6 |

> 未采集到数据的指标（通常表示该链路环节未带埋点/未触发）：ASR: kws_detected→start_asr、ASR: start_asr→endpoint、ASR: endpoint→nurse_req、ASR: nurse_req→nurse_resp、ASR: nurse_resp→publish_question、ASR: endpoint→probe收到question、ASR: publish_question→probe收到、语音结束→首音 (endpoint-静音阈值)、问句播放结束→首音 (wav触发)


<details><summary>每轮端到端（question→首音 / 会话总时长 / 状态）</summary>

| round | t0_source | question→首音(ms) | 语音结束→首音(ms) | 会话总时长(ms) | status |
| ---: | --- | ---: | ---: | ---: | --- |
| 0 | publish_question | 7821.5 | — | 10938.4 | ok |
| 1 | publish_question | 8174.2 | — | 22801.8 | ok |
| 2 | publish_question | 7462.3 | — | 10213.9 | ok |

</details>

## tag: `voice_weather`（mode=wake-wav, n=3, ok=3）

- summary: `/home/artrc/medical_ws/Medical_Embodied_dev/tools/latency_probe/records/voice_weather_summary.json`
- 触发参数：inject_text=None, question_wav='audio/q_weather.wav', wake_wav='audio/wake_xiaoyi.wav'
- endpoint 静音阈值：1.3s；事件回溯窗口：60.0s；轮间隔：18.0s

| 阶段(stage) | n | mean | median | p95 | min | max | stdev |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| ASR: kws_detected→start_asr | 3 | 2.6 | 1.9 | 4.1 | 1.8 | 4.1 | 1.3 |
| ASR: start_asr→endpoint | 3 | 3662.6 | 3579.7 | 3836.3 | 3571.9 | 3836.3 | 150.4 |
| ASR: endpoint→nurse_req | 3 | 13.4 | 12.5 | 15.8 | 11.8 | 15.8 | 2.2 |
| ASR: nurse_req→nurse_resp | 3 | 1668.4 | 1613.8 | 2288.8 | 1102.7 | 2288.8 | 594.9 |
| ASR: nurse_resp→publish_question | 3 | 0.1 | 0.1 | 0.1 | 0.1 | 0.1 | 0.0 |
| ASR: endpoint→probe收到question | 3 | 1682.4 | 1630.4 | 2301.8 | 1115.0 | 2301.8 | 595.1 |
| ASR: publish_question→probe收到 | 3 | 0.5 | 0.5 | 0.6 | 0.4 | 0.6 | 0.1 |
| Router: question→路由话题到达 | 3 | 0.8 | 0.7 | 0.9 | 0.7 | 0.9 | 0.1 |
| LLM: 路由话题→首块 | 3 | 1102.6 | 1114.0 | 1560.1 | 633.8 | 1560.1 | 463.2 |
| LLM TTFT: question→首块 | 3 | 1103.4 | 1114.8 | 1560.7 | 634.5 | 1560.7 | 463.2 |
| LLM 总时长: 首块→[DONE] | 3 | 1359.1 | 1276.8 | 1531.6 | 1268.9 | 1531.6 | 149.5 |
| LLM: question→[DONE] | 3 | 2462.5 | 2383.8 | 3092.4 | 1911.3 | 3092.4 | 594.4 |
| 断句: LLM首块→首个TTS文本 | 3 | 133.2 | 79.2 | 246.2 | 74.3 | 246.2 | 97.8 |
| 断句: question→首个TTS文本 | 3 | 1236.6 | 1194.0 | 1806.9 | 708.8 | 1806.9 | 550.3 |
| 断句: 首个TTS文本→首音 | 3 | 1122.0 | 1131.4 | 1224.1 | 1010.4 | 1224.1 | 107.2 |
| TTS 首句合成: rx→synth_end | 3 | 1083.7 | 1086.7 | 1158.1 | 1006.2 | 1158.1 | 76.0 |
| TTS 首音: rx→play_start | 3 | 1121.8 | 1131.3 | 1223.9 | 1010.1 | 1223.9 | 107.2 |
| TTS 首句: synth_end→play_start | 3 | 38.1 | 44.6 | 65.7 | 3.8 | 65.7 | 31.5 |
| 端到端: question→首个play_start | 3 | 2358.5 | 2204.4 | 3031.0 | 1840.2 | 3031.0 | 610.2 |
| 语音结束→首音 (endpoint-静音阈值) | 3 | 5341.0 | 5446.0 | 5806.3 | 4770.6 | 5806.3 | 525.7 |
| 问句播放结束→首音 (wav触发) | 3 | 5251.8 | 5257.3 | 5655.8 | 4842.3 | 5655.8 | 406.8 |
| 会话总时长: question→session_finished | 3 | 20412.4 | 20694.3 | 20832.4 | 19710.5 | 20832.4 | 611.7 |
| 音频总时长(各段play相加) | 3 | 18043.8 | 18614.5 | 18844.9 | 16672.0 | 18844.9 | 1193.6 |
| 音频跨距: 首play_start→末play_end | 3 | 18051.3 | 18623.6 | 18852.6 | 16677.7 | 18852.6 | 1195.1 |

<details><summary>每轮端到端（question→首音 / 会话总时长 / 状态）</summary>

| round | t0_source | question→首音(ms) | 语音结束→首音(ms) | 会话总时长(ms) | status |
| ---: | --- | ---: | ---: | ---: | --- |
| 0 | question_asr_rx | 3031.0 | 5446.0 | 19710.5 | ok |
| 1 | question_asr_rx | 2204.4 | 5806.3 | 20832.4 | ok |
| 2 | question_asr_rx | 1840.2 | 4770.6 | 20694.3 | ok |

</details>

## tag: `voice_medical`（mode=wake-wav, n=2, ok=2）

- summary: `/home/artrc/medical_ws/Medical_Embodied_dev/tools/latency_probe/records/voice_medical_summary.json`
- 触发参数：inject_text=None, question_wav='audio/q_medical_exercise.wav', wake_wav='audio/wake_xiaoyi.wav'
- endpoint 静音阈值：1.3s；事件回溯窗口：60.0s；轮间隔：30.0s

| 阶段(stage) | n | mean | median | p95 | min | max | stdev |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| ASR: kws_detected→start_asr | 2 | 6.3 | 6.3 | 7.1 | 5.4 | 7.1 | 1.2 |
| ASR: start_asr→endpoint | 2 | 5109.6 | 5109.6 | 5161.9 | 5057.4 | 5161.9 | 73.9 |
| ASR: endpoint→nurse_req | 2 | 17.7 | 17.7 | 22.5 | 12.8 | 22.5 | 6.9 |
| ASR: nurse_req→nurse_resp | 2 | 1613.9 | 1613.9 | 1988.9 | 1239.0 | 1988.9 | 530.3 |
| ASR: nurse_resp→publish_question | 2 | 0.1 | 0.1 | 0.2 | 0.1 | 0.2 | 0.1 |
| ASR: endpoint→probe收到question | 2 | 1632.2 | 1632.2 | 2012.1 | 1252.4 | 2012.1 | 537.2 |
| ASR: publish_question→probe收到 | 2 | 0.5 | 0.5 | 0.5 | 0.5 | 0.5 | 0.1 |
| Router: question→路由话题到达 | 2 | 0.7 | 0.7 | 0.8 | 0.6 | 0.8 | 0.1 |
| LLM: 路由话题→首块 | 2 | 8257.6 | 8257.6 | 10924.5 | 5590.7 | 10924.5 | 3771.5 |
| LLM TTFT: question→首块 | 2 | 8258.3 | 8258.3 | 10925.2 | 5591.3 | 10925.2 | 3771.6 |
| LLM 总时长: 首块→[DONE] | 2 | 940.6 | 940.6 | 1388.2 | 493.1 | 1388.2 | 632.9 |
| LLM: question→[DONE] | 2 | 9198.9 | 9198.9 | 12313.4 | 6084.4 | 12313.4 | 4404.5 |
| 断句: LLM首块→首个TTS文本 | 2 | 43.7 | 43.7 | 86.9 | 0.4 | 86.9 | 61.1 |
| 断句: question→首个TTS文本 | 2 | 8302.0 | 8302.0 | 11012.1 | 5591.8 | 11012.1 | 3832.8 |
| 断句: 首个TTS文本→首音 | 2 | 1145.5 | 1145.5 | 1208.1 | 1082.8 | 1208.1 | 88.6 |
| TTS 首句合成: rx→synth_end | 2 | 1059.2 | 1059.2 | 1120.9 | 997.5 | 1120.9 | 87.3 |
| TTS 首音: rx→play_start | 2 | 1145.2 | 1145.2 | 1207.9 | 1082.5 | 1207.9 | 88.7 |
| TTS 首句: synth_end→play_start | 2 | 86.0 | 86.0 | 87.0 | 85.0 | 87.0 | 1.4 |
| 端到端: question→首个play_start | 2 | 9447.4 | 9447.4 | 12220.3 | 6674.6 | 12220.3 | 3921.4 |
| 语音结束→首音 (endpoint-静音阈值) | 2 | 12379.7 | 12379.7 | 14772.6 | 9986.7 | 14772.6 | 3384.1 |
| 问句播放结束→首音 (wav触发) | 2 | 12437.9 | 12437.9 | 14921.2 | 9954.6 | 14921.2 | 3511.9 |
| 会话总时长: question→session_finished | 2 | 35693.5 | 35693.5 | 53655.2 | 17731.7 | 53655.2 | 25401.7 |
| 音频总时长(各段play相加) | 2 | 26236.6 | 26236.6 | 41420.3 | 11053.0 | 41420.3 | 21472.9 |
| 音频跨距: 首play_start→末play_end | 2 | 26245.0 | 26245.0 | 41433.7 | 11056.2 | 41433.7 | 21480.2 |

<details><summary>每轮端到端（question→首音 / 会话总时长 / 状态）</summary>

| round | t0_source | question→首音(ms) | 语音结束→首音(ms) | 会话总时长(ms) | status |
| ---: | --- | ---: | ---: | ---: | --- |
| 0 | question_asr_rx | 6674.6 | 9986.7 | 17731.7 | ok |
| 1 | question_asr_rx | 12220.3 | 14772.6 | 53655.2 | ok |

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


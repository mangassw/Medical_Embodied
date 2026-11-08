## 延迟统计 tag=judge2 mode=inject-text (n=1, 单位 ms)

| 阶段(stage) | n | mean | median | p95 | min | max | stdev |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Router: question→路由话题到达 | 1 | 0.8 | 0.8 | 0.8 | 0.8 | 0.8 | 0.0 |
| LLM: 路由话题→首块 | 1 | 1419.8 | 1419.8 | 1419.8 | 1419.8 | 1419.8 | 0.0 |
| LLM TTFT: question→首块 | 1 | 1420.6 | 1420.6 | 1420.6 | 1420.6 | 1420.6 | 0.0 |
| LLM 总时长: 首块→[DONE] | 1 | 614.0 | 614.0 | 614.0 | 614.0 | 614.0 | 0.0 |
| LLM: question→[DONE] | 1 | 2034.6 | 2034.6 | 2034.6 | 2034.6 | 2034.6 | 0.0 |
| 断句: LLM首块→首个TTS文本 | 1 | 515.0 | 515.0 | 515.0 | 515.0 | 515.0 | 0.0 |
| 断句: question→首个TTS文本 | 1 | 1935.7 | 1935.7 | 1935.7 | 1935.7 | 1935.7 | 0.0 |
| 断句: 首个TTS文本→首音 | 1 | 1532.2 | 1532.2 | 1532.2 | 1532.2 | 1532.2 | 0.0 |
| TTS 首句合成: rx→synth_end | 1 | 1511.4 | 1511.4 | 1511.4 | 1511.4 | 1511.4 | 0.0 |
| TTS 首音: rx→play_start | 1 | 1531.8 | 1531.8 | 1531.8 | 1531.8 | 1531.8 | 0.0 |
| TTS 首句: synth_end→play_start | 1 | 20.4 | 20.4 | 20.4 | 20.4 | 20.4 | 0.0 |
| 端到端: question→首个play_start | 1 | 3467.8 | 3467.8 | 3467.8 | 3467.8 | 3467.8 | 0.0 |
| 会话总时长: question→session_finished | 1 | 9342.3 | 9342.3 | 9342.3 | 9342.3 | 9342.3 | 0.0 |
| 音频总时长(各段play相加) | 1 | 5872.3 | 5872.3 | 5872.3 | 5872.3 | 5872.3 | 0.0 |
| 音频跨距: 首play_start→末play_end | 1 | 5873.1 | 5873.1 | 5873.1 | 5873.1 | 5873.1 | 0.0 |

> 未采集到数据的指标（通常表示该链路环节未带埋点/未触发）：ASR: kws_detected→start_asr、ASR: start_asr→endpoint、ASR: endpoint→nurse_req、ASR: nurse_req→nurse_resp、ASR: nurse_resp→publish_question、ASR: endpoint→probe收到question、ASR: publish_question→probe收到、语音结束→首音 (endpoint-静音阈值)、问句播放结束→首音 (wav触发)

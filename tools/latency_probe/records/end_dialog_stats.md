## 延迟统计 tag=end_dialog mode=wake-wav (n=1, 单位 ms)

| 阶段(stage) | n | mean | median | p95 | min | max | stdev |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| ASR: kws_detected→start_asr | 1 | 1.2 | 1.2 | 1.2 | 1.2 | 1.2 | 0.0 |
| ASR: start_asr→endpoint | 1 | 4545.4 | 4545.4 | 4545.4 | 4545.4 | 4545.4 | 0.0 |
| ASR: endpoint→nurse_req | 1 | 10.6 | 10.6 | 10.6 | 10.6 | 10.6 | 0.0 |
| ASR: endpoint→probe收到question | 1 | 10.9 | 10.9 | 10.9 | 10.9 | 10.9 | 0.0 |
| ASR: publish_question→probe收到 | 1 | 0.3 | 0.3 | 0.3 | 0.3 | 0.3 | 0.0 |
| Router: question→路由话题到达 | 1 | 8.9 | 8.9 | 8.9 | 8.9 | 8.9 | 0.0 |
| LLM: 路由话题→首块 | 1 | 1197.2 | 1197.2 | 1197.2 | 1197.2 | 1197.2 | 0.0 |
| LLM TTFT: question→首块 | 1 | 1206.1 | 1206.1 | 1206.1 | 1206.1 | 1206.1 | 0.0 |
| LLM 总时长: 首块→[DONE] | 1 | 232.7 | 232.7 | 232.7 | 232.7 | 232.7 | 0.0 |
| LLM: question→[DONE] | 1 | 1438.8 | 1438.8 | 1438.8 | 1438.8 | 1438.8 | 0.0 |
| 断句: LLM首块→首个TTS文本 | 1 | 203.6 | 203.6 | 203.6 | 203.6 | 203.6 | 0.0 |
| 断句: question→首个TTS文本 | 1 | 1409.8 | 1409.8 | 1409.8 | 1409.8 | 1409.8 | 0.0 |
| TTS 首句合成: rx→synth_end | 1 | 1073.5 | 1073.5 | 1073.5 | 1073.5 | 1073.5 | 0.0 |

> 未采集到数据的指标（通常表示该链路环节未带埋点/未触发）：ASR: nurse_req→nurse_resp、ASR: nurse_resp→publish_question、断句: 首个TTS文本→首音、TTS 首音: rx→play_start、TTS 首句: synth_end→play_start、端到端: question→首个play_start、语音结束→首音 (endpoint-静音阈值)、问句播放结束→首音 (wav触发)、会话总时长: question→session_finished、音频总时长(各段play相加)、音频跨距: 首play_start→末play_end

## 延迟统计 tag=inj_medical mode=inject-text (n=3, 单位 ms)

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

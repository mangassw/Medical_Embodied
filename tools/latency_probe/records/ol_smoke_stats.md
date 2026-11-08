## 延迟统计 tag=ol_smoke mode=inject-text (n=3, 单位 ms)

| 阶段(stage) | n | mean | median | p95 | min | max | stdev |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Router: question→路由话题到达 | 3 | 1.9 | 1.0 | 3.9 | 0.9 | 3.9 | 1.7 |
| LLM: 路由话题→首块 | 3 | 1476.7 | 382.1 | 3754.1 | 294.0 | 3754.1 | 1972.8 |
| LLM TTFT: question→首块 | 3 | 1478.7 | 383.1 | 3758.1 | 294.9 | 3758.1 | 1974.5 |
| LLM 总时长: 首块→[DONE] | 3 | 769.0 | 799.8 | 803.2 | 703.9 | 803.2 | 56.4 |
| LLM: question→[DONE] | 3 | 2247.6 | 1098.1 | 4557.8 | 1086.9 | 4557.8 | 2000.7 |
| 断句: LLM首块→首个TTS文本 | 3 | 98.4 | 93.2 | 134.4 | 67.7 | 134.4 | 33.6 |
| 断句: question→首个TTS文本 | 3 | 1577.1 | 450.8 | 3892.4 | 388.2 | 3892.4 | 2005.3 |
| 断句: 首个TTS文本→首音 | 3 | 1054.1 | 923.2 | 1337.8 | 901.3 | 1337.8 | 245.9 |
| TTS 首句合成: rx→synth_end | 3 | 1016.6 | 887.0 | 1300.2 | 862.8 | 1300.2 | 245.8 |
| TTS 首音: rx→play_start | 3 | 1053.6 | 922.8 | 1337.6 | 900.5 | 1337.6 | 246.2 |
| TTS 首句: synth_end→play_start | 3 | 37.0 | 37.4 | 37.7 | 35.8 | 37.7 | 1.0 |
| 端到端: question→首个play_start | 3 | 2631.2 | 1374.0 | 5230.2 | 1289.4 | 5230.2 | 2251.2 |
| 会话总时长: question→session_finished | 3 | 9824.6 | 8748.5 | 12522.7 | 8202.5 | 12522.7 | 2352.5 |
| 音频总时长(各段play相加) | 3 | 7190.7 | 7290.0 | 7372.0 | 6910.0 | 7372.0 | 246.5 |
| 音频跨距: 首play_start→末play_end | 3 | 7192.3 | 7291.4 | 7373.4 | 6912.1 | 7373.4 | 246.1 |

> 未采集到数据的指标（通常表示该链路环节未带埋点/未触发）：ASR: kws_detected→start_asr、ASR: start_asr→endpoint、ASR: endpoint→nurse_req、ASR: nurse_req→nurse_resp、ASR: nurse_resp→publish_question、ASR: endpoint→probe收到question、ASR: publish_question→probe收到、语音结束→首音 (endpoint-静音阈值)、问句播放结束→首音 (wav触发)

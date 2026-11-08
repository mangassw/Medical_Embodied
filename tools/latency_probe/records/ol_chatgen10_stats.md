## 延迟统计 tag=ol_chatgen10 mode=inject-text (n=10, 单位 ms)

| 阶段(stage) | n | mean | median | p95 | min | max | stdev |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Router: question→路由话题到达 | 10 | 1.7 | 1.0 | 5.5 | 0.8 | 5.5 | 1.6 |
| LLM: 路由话题→首块 | 10 | 1048.0 | 1173.2 | 1348.9 | 541.7 | 1348.9 | 337.4 |
| LLM TTFT: question→首块 | 10 | 1049.7 | 1174.1 | 1352.4 | 542.6 | 1352.4 | 338.2 |
| LLM 总时长: 首块→[DONE] | 10 | 390.6 | 399.0 | 696.6 | 104.3 | 696.6 | 187.0 |
| LLM: question→[DONE] | 10 | 1440.2 | 1520.8 | 1920.0 | 848.8 | 1920.0 | 351.7 |
| 断句: LLM首块→首个TTS文本 | 10 | 100.7 | 94.8 | 156.1 | 60.4 | 156.1 | 29.6 |
| 断句: question→首个TTS文本 | 10 | 1150.4 | 1246.9 | 1498.6 | 658.6 | 1498.6 | 338.2 |
| 断句: 首个TTS文本→首音 | 10 | 983.2 | 973.9 | 1291.5 | 733.5 | 1291.5 | 172.0 |
| TTS 首句合成: rx→synth_end | 10 | 926.8 | 900.6 | 1138.2 | 683.6 | 1138.2 | 152.9 |
| TTS 首音: rx→play_start | 10 | 970.5 | 973.7 | 1166.5 | 733.2 | 1166.5 | 150.4 |
| TTS 首句: synth_end→play_start | 10 | 43.7 | 47.1 | 75.5 | 10.4 | 75.5 | 23.9 |
| 端到端: question→首个play_start | 10 | 2133.6 | 2041.1 | 2778.8 | 1612.2 | 2778.8 | 389.4 |
| 会话总时长: question→session_finished | 10 | 6819.6 | 6440.9 | 9230.5 | 4182.4 | 9230.5 | 1559.1 |
| 音频总时长(各段play相加) | 10 | 4683.8 | 4585.0 | 7308.5 | 2242.6 | 7308.5 | 1474.6 |
| 音频跨距: 首play_start→末play_end | 10 | 4684.7 | 4585.7 | 7310.3 | 2242.6 | 7310.3 | 1475.1 |

> 未采集到数据的指标（通常表示该链路环节未带埋点/未触发）：ASR: kws_detected→start_asr、ASR: start_asr→endpoint、ASR: endpoint→nurse_req、ASR: nurse_req→nurse_resp、ASR: nurse_resp→publish_question、ASR: endpoint→probe收到question、ASR: publish_question→probe收到、语音结束→首音 (endpoint-静音阈值)、问句播放结束→首音 (wav触发)

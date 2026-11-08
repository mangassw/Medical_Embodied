## 延迟统计 tag=ol3_chatgen10 mode=inject-text (n=10, 单位 ms)

| 阶段(stage) | n | mean | median | p95 | min | max | stdev |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Router: question→路由话题到达 | 10 | 1.9 | 1.1 | 5.8 | 1.0 | 5.8 | 1.5 |
| LLM: 路由话题→首块 | 10 | 181.5 | 178.1 | 238.9 | 165.8 | 238.9 | 20.8 |
| LLM TTFT: question→首块 | 10 | 183.4 | 179.7 | 240.2 | 166.8 | 240.2 | 20.9 |
| LLM 总时长: 首块→[DONE] | 10 | 237.9 | 242.4 | 357.0 | 115.4 | 357.0 | 79.9 |
| LLM: question→[DONE] | 10 | 421.3 | 433.4 | 532.1 | 285.5 | 532.1 | 79.6 |
| 断句: LLM首块→首个TTS文本 | 10 | 74.2 | 51.8 | 157.0 | 44.2 | 157.0 | 38.4 |
| 断句: question→首个TTS文本 | 10 | 257.6 | 237.0 | 339.6 | 214.8 | 339.6 | 42.5 |
| 断句: 首个TTS文本→首音 | 10 | 959.6 | 888.0 | 1504.9 | 694.4 | 1504.9 | 246.6 |
| TTS 首句合成: rx→synth_end | 10 | 910.3 | 848.3 | 1448.9 | 686.9 | 1448.9 | 240.6 |
| TTS 首音: rx→play_start | 10 | 954.3 | 883.4 | 1502.2 | 694.2 | 1502.2 | 246.1 |
| TTS 首句: synth_end→play_start | 10 | 44.0 | 52.1 | 71.4 | 3.1 | 71.4 | 23.7 |
| 端到端: question→首个play_start | 10 | 1217.1 | 1131.4 | 1810.3 | 925.1 | 1810.3 | 272.6 |
| 会话总时长: question→session_finished | 10 | 5196.0 | 5125.3 | 6871.3 | 3332.5 | 6871.3 | 1002.8 |
| 音频总时长(各段play相加) | 10 | 3976.5 | 3843.1 | 5766.0 | 2169.7 | 5766.0 | 959.4 |
| 音频跨距: 首play_start→末play_end | 10 | 3977.4 | 3844.0 | 5768.0 | 2169.7 | 5768.0 | 959.8 |

> 未采集到数据的指标（通常表示该链路环节未带埋点/未触发）：ASR: kws_detected→start_asr、ASR: start_asr→endpoint、ASR: endpoint→nurse_req、ASR: nurse_req→nurse_resp、ASR: nurse_resp→publish_question、ASR: endpoint→probe收到question、ASR: publish_question→probe收到、语音结束→首音 (endpoint-静音阈值)、问句播放结束→首音 (wav触发)

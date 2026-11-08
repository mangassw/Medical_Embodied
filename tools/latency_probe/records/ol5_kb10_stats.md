## 延迟统计 tag=ol5_kb10 mode=inject-text (n=10, 单位 ms)

| 阶段(stage) | n | mean | median | p95 | min | max | stdev |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Router: question→路由话题到达 | 10 | 1.8 | 1.2 | 4.7 | 0.9 | 4.7 | 1.2 |
| LLM: 路由话题→首块 | 10 | 503.6 | 519.1 | 633.3 | 344.8 | 633.3 | 97.9 |
| LLM TTFT: question→首块 | 10 | 505.4 | 520.1 | 634.3 | 345.9 | 634.3 | 98.1 |
| LLM 总时长: 首块→[DONE] | 10 | 927.0 | 881.2 | 1268.0 | 667.9 | 1268.0 | 234.1 |
| LLM: question→[DONE] | 10 | 1432.4 | 1363.8 | 1807.0 | 1066.6 | 1807.0 | 263.3 |
| 断句: LLM首块→首个TTS文本 | 10 | 118.5 | 81.7 | 309.1 | 45.0 | 309.1 | 86.6 |
| 断句: question→首个TTS文本 | 10 | 623.9 | 618.3 | 792.7 | 516.1 | 792.7 | 84.2 |
| 断句: 首个TTS文本→首音 | 10 | 1212.4 | 1204.3 | 2127.9 | 739.5 | 2127.9 | 439.3 |
| TTS 首句合成: rx→synth_end | 10 | 1151.4 | 1132.5 | 2038.2 | 715.4 | 2038.2 | 429.6 |
| TTS 首音: rx→play_start | 10 | 1211.6 | 1204.0 | 2122.0 | 739.3 | 2122.0 | 438.0 |
| TTS 首句: synth_end→play_start | 10 | 60.2 | 71.3 | 103.2 | 3.2 | 103.2 | 34.6 |
| 端到端: question→首个play_start | 10 | 1836.4 | 1768.3 | 2782.8 | 1255.6 | 2782.8 | 471.4 |
| 会话总时长: question→session_finished | 10 | 10715.6 | 10762.5 | 13926.6 | 7102.7 | 13926.6 | 2231.4 |
| 音频总时长(各段play相加) | 10 | 8872.0 | 8700.8 | 11929.4 | 5844.4 | 11929.4 | 2043.4 |
| 音频跨距: 首play_start→末play_end | 10 | 8876.4 | 8705.6 | 11933.8 | 5845.9 | 11933.8 | 2045.2 |

> 未采集到数据的指标（通常表示该链路环节未带埋点/未触发）：ASR: kws_detected→start_asr、ASR: start_asr→endpoint、ASR: endpoint→nurse_req、ASR: nurse_req→nurse_resp、ASR: nurse_resp→publish_question、ASR: endpoint→probe收到question、ASR: publish_question→probe收到、语音结束→首音 (endpoint-静音阈值)、问句播放结束→首音 (wav触发)

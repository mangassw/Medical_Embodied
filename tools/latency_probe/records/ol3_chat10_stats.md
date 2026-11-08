## 延迟统计 tag=ol3_chat10 mode=inject-text (n=10, 单位 ms)

| 阶段(stage) | n | mean | median | p95 | min | max | stdev |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Router: question→路由话题到达 | 10 | 2.8 | 1.4 | 5.7 | 0.9 | 5.7 | 2.2 |
| LLM: 路由话题→首块 | 10 | 154.1 | 146.6 | 246.8 | 129.6 | 246.8 | 33.7 |
| LLM TTFT: question→首块 | 10 | 156.9 | 149.7 | 247.7 | 131.1 | 247.7 | 33.3 |
| LLM 总时长: 首块→[DONE] | 10 | 419.5 | 325.6 | 1232.5 | 152.1 | 1232.5 | 318.9 |
| LLM: question→[DONE] | 10 | 576.4 | 474.4 | 1378.5 | 341.4 | 1378.5 | 311.7 |
| 断句: LLM首块→首个TTS文本 | 10 | 39.4 | 38.9 | 100.6 | 5.7 | 100.6 | 25.1 |
| 断句: question→首个TTS文本 | 10 | 196.3 | 189.7 | 269.7 | 151.6 | 269.7 | 37.2 |
| 断句: 首个TTS文本→首音 | 10 | 904.9 | 754.2 | 1957.0 | 573.5 | 1957.0 | 424.0 |
| TTS 首句合成: rx→synth_end | 10 | 854.6 | 727.1 | 1906.3 | 516.8 | 1906.3 | 430.2 |
| TTS 首音: rx→play_start | 10 | 903.9 | 752.8 | 1956.0 | 572.6 | 1956.0 | 423.9 |
| TTS 首句: synth_end→play_start | 10 | 49.4 | 47.7 | 98.1 | 8.9 | 98.1 | 28.4 |
| 端到端: question→首个play_start | 10 | 1101.2 | 943.9 | 2108.7 | 767.0 | 2108.7 | 416.7 |
| 会话总时长: question→session_finished | 10 | 6817.5 | 5567.9 | 17629.9 | 3609.3 | 17629.9 | 4173.0 |
| 音频总时长(各段play相加) | 10 | 5691.8 | 4667.0 | 15504.7 | 2763.3 | 15504.7 | 3784.1 |
| 音频跨距: 首play_start→末play_end | 10 | 5713.8 | 4670.0 | 15519.4 | 2764.7 | 15519.4 | 3784.8 |

> 未采集到数据的指标（通常表示该链路环节未带埋点/未触发）：ASR: kws_detected→start_asr、ASR: start_asr→endpoint、ASR: endpoint→nurse_req、ASR: nurse_req→nurse_resp、ASR: nurse_resp→publish_question、ASR: endpoint→probe收到question、ASR: publish_question→probe收到、语音结束→首音 (endpoint-静音阈值)、问句播放结束→首音 (wav触发)

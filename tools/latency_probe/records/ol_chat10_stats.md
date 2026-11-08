## 延迟统计 tag=ol_chat10 mode=inject-text (n=10, 单位 ms)

| 阶段(stage) | n | mean | median | p95 | min | max | stdev |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Router: question→路由话题到达 | 10 | 1.0 | 1.0 | 1.7 | 0.8 | 1.7 | 0.3 |
| LLM: 路由话题→首块 | 10 | 180.3 | 171.2 | 327.7 | 131.4 | 327.7 | 56.7 |
| LLM TTFT: question→首块 | 10 | 181.3 | 172.1 | 328.7 | 132.4 | 328.7 | 56.6 |
| LLM 总时长: 首块→[DONE] | 10 | 782.4 | 644.5 | 1734.6 | 289.8 | 1734.6 | 468.7 |
| LLM: question→[DONE] | 10 | 963.7 | 777.3 | 1903.7 | 591.6 | 1903.7 | 457.1 |
| 断句: LLM首块→首个TTS文本 | 10 | 53.4 | 49.2 | 91.0 | 17.7 | 91.0 | 22.8 |
| 断句: question→首个TTS文本 | 10 | 234.7 | 235.2 | 362.6 | 174.0 | 362.6 | 58.5 |
| 断句: 首个TTS文本→首音 | 10 | 829.2 | 760.8 | 1390.2 | 591.1 | 1390.2 | 229.0 |
| TTS 首句合成: rx→synth_end | 10 | 780.0 | 725.8 | 1368.6 | 499.2 | 1368.6 | 247.7 |
| TTS 首音: rx→play_start | 10 | 827.9 | 758.3 | 1385.7 | 590.7 | 1385.7 | 228.0 |
| TTS 首句: synth_end→play_start | 10 | 47.9 | 39.7 | 91.5 | 11.0 | 91.5 | 30.1 |
| 端到端: question→首个play_start | 10 | 1064.0 | 963.9 | 1650.3 | 782.3 | 1650.3 | 244.0 |
| 会话总时长: question→session_finished | 10 | 8220.5 | 6949.2 | 17452.2 | 4007.6 | 17452.2 | 4044.0 |
| 音频总时长(各段play相加) | 10 | 7132.8 | 5883.7 | 15796.1 | 3035.6 | 15796.1 | 3884.2 |
| 音频跨距: 首play_start→末play_end | 10 | 7155.3 | 5885.2 | 15800.9 | 3036.3 | 15800.9 | 3915.2 |

> 未采集到数据的指标（通常表示该链路环节未带埋点/未触发）：ASR: kws_detected→start_asr、ASR: start_asr→endpoint、ASR: endpoint→nurse_req、ASR: nurse_req→nurse_resp、ASR: nurse_resp→publish_question、ASR: endpoint→probe收到question、ASR: publish_question→probe收到、语音结束→首音 (endpoint-静音阈值)、问句播放结束→首音 (wav触发)

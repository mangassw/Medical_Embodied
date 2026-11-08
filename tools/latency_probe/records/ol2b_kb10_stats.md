## 延迟统计 tag=ol2b_kb10 mode=inject-text (n=10, 单位 ms)

| 阶段(stage) | n | mean | median | p95 | min | max | stdev |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Router: question→路由话题到达 | 10 | 1.1 | 1.0 | 1.3 | 0.9 | 1.3 | 0.2 |
| LLM: 路由话题→首块 | 10 | 510.8 | 506.1 | 654.0 | 356.4 | 654.0 | 102.0 |
| LLM TTFT: question→首块 | 10 | 511.9 | 507.2 | 654.9 | 357.5 | 654.9 | 102.0 |
| LLM 总时长: 首块→[DONE] | 10 | 942.0 | 928.4 | 1298.7 | 701.4 | 1298.7 | 168.2 |
| LLM: question→[DONE] | 10 | 1453.8 | 1430.1 | 1828.3 | 1214.8 | 1828.3 | 180.4 |
| 断句: LLM首块→首个TTS文本 | 10 | 114.3 | 87.6 | 300.9 | 47.6 | 300.9 | 83.3 |
| 断句: question→首个TTS文本 | 10 | 626.2 | 635.6 | 788.1 | 545.6 | 788.1 | 77.5 |
| 断句: 首个TTS文本→首音 | 10 | 1156.9 | 1050.1 | 1847.6 | 750.9 | 1847.6 | 415.3 |
| TTS 首句合成: rx→synth_end | 10 | 1112.7 | 1012.5 | 1775.0 | 710.3 | 1775.0 | 421.9 |
| TTS 首音: rx→play_start | 10 | 1156.5 | 1049.9 | 1847.2 | 750.7 | 1847.2 | 415.1 |
| TTS 首句: synth_end→play_start | 10 | 43.9 | 41.8 | 82.0 | 5.6 | 82.0 | 24.5 |
| 端到端: question→首个play_start | 10 | 1783.1 | 1666.2 | 2453.3 | 1298.2 | 2453.3 | 433.2 |
| 会话总时长: question→session_finished | 10 | 11172.7 | 11194.2 | 13114.0 | 9369.2 | 13114.0 | 1250.9 |
| 音频总时长(各段play相加) | 10 | 9374.0 | 9403.4 | 11039.9 | 7837.2 | 11039.9 | 1121.0 |
| 音频跨距: 首play_start→末play_end | 10 | 9388.2 | 9406.4 | 11041.5 | 7838.7 | 11041.5 | 1133.9 |

> 未采集到数据的指标（通常表示该链路环节未带埋点/未触发）：ASR: kws_detected→start_asr、ASR: start_asr→endpoint、ASR: endpoint→nurse_req、ASR: nurse_req→nurse_resp、ASR: nurse_resp→publish_question、ASR: endpoint→probe收到question、ASR: publish_question→probe收到、语音结束→首音 (endpoint-静音阈值)、问句播放结束→首音 (wav触发)

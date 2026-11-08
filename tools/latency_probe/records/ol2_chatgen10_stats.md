## 延迟统计 tag=ol2_chatgen10 mode=inject-text (n=10, 单位 ms)

| 阶段(stage) | n | mean | median | p95 | min | max | stdev |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Router: question→路由话题到达 | 10 | 1.0 | 1.0 | 1.1 | 0.9 | 1.1 | 0.1 |
| LLM: 路由话题→首块 | 10 | 222.5 | 225.3 | 240.6 | 199.7 | 240.6 | 17.1 |
| LLM TTFT: question→首块 | 10 | 223.4 | 226.3 | 241.7 | 200.7 | 241.7 | 17.1 |
| LLM 总时长: 首块→[DONE] | 10 | 375.0 | 436.5 | 556.9 | 139.9 | 556.9 | 170.2 |
| LLM: question→[DONE] | 10 | 598.4 | 657.7 | 796.4 | 360.8 | 796.4 | 176.0 |
| 断句: LLM首块→首个TTS文本 | 10 | 115.3 | 104.2 | 275.8 | 52.8 | 275.8 | 63.9 |
| 断句: question→首个TTS文本 | 10 | 338.7 | 335.5 | 517.4 | 253.5 | 517.4 | 74.4 |
| 断句: 首个TTS文本→首音 | 10 | 947.8 | 947.7 | 1388.7 | 641.7 | 1388.7 | 210.9 |
| TTS 首句合成: rx→synth_end | 10 | 883.9 | 869.8 | 1338.0 | 527.0 | 1338.0 | 252.7 |
| TTS 首音: rx→play_start | 10 | 941.8 | 947.8 | 1388.5 | 596.4 | 1388.5 | 246.4 |
| TTS 首句: synth_end→play_start | 10 | 57.9 | 67.1 | 84.3 | 15.7 | 84.3 | 25.3 |
| 端到端: question→首个play_start | 10 | 1286.5 | 1300.1 | 1672.1 | 895.2 | 1672.1 | 240.8 |
| 会话总时长: question→session_finished | 10 | 6111.0 | 6288.9 | 10298.5 | 3542.3 | 10298.5 | 1841.9 |
| 音频总时长(各段play相加) | 10 | 4822.5 | 5087.4 | 8624.0 | 2270.5 | 8624.0 | 1727.9 |
| 音频跨距: 首play_start→末play_end | 10 | 4823.4 | 5088.6 | 8625.4 | 2270.5 | 8625.4 | 1728.3 |

> 未采集到数据的指标（通常表示该链路环节未带埋点/未触发）：ASR: kws_detected→start_asr、ASR: start_asr→endpoint、ASR: endpoint→nurse_req、ASR: nurse_req→nurse_resp、ASR: nurse_resp→publish_question、ASR: endpoint→probe收到question、ASR: publish_question→probe收到、语音结束→首音 (endpoint-静音阈值)、问句播放结束→首音 (wav触发)

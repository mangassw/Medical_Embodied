## 延迟统计 tag=chat_fix3 mode=inject-text (n=3, 单位 ms)

| 阶段(stage) | n | mean | median | p95 | min | max | stdev |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Router: question→路由话题到达 | 3 | 1.0 | 0.9 | 1.2 | 0.8 | 1.2 | 0.2 |
| LLM: 路由话题→首块 | 3 | 158.3 | 106.6 | 270.7 | 97.7 | 270.7 | 97.4 |
| LLM TTFT: question→首块 | 3 | 159.3 | 107.5 | 271.9 | 98.5 | 271.9 | 97.6 |
| LLM 总时长: 首块→[DONE] | 3 | 679.2 | 446.2 | 1279.2 | 312.1 | 1279.2 | 523.9 |
| LLM: question→[DONE] | 3 | 838.5 | 584.0 | 1377.7 | 553.7 | 1377.7 | 467.2 |
| 断句: LLM首块→首个TTS文本 | 3 | 38.3 | 39.9 | 40.6 | 34.4 | 40.6 | 3.4 |
| 断句: question→首个TTS文本 | 3 | 197.6 | 148.1 | 311.8 | 132.9 | 311.8 | 99.2 |
| 断句: 首个TTS文本→首音 | 3 | 646.0 | 671.2 | 674.0 | 592.8 | 674.0 | 46.1 |
| TTS 首句合成: rx→synth_end | 3 | 564.5 | 592.7 | 593.3 | 507.5 | 593.3 | 49.4 |
| TTS 首音: rx→play_start | 3 | 638.2 | 671.2 | 673.9 | 569.4 | 673.9 | 59.5 |
| TTS 首句: synth_end→play_start | 3 | 73.7 | 78.5 | 80.5 | 62.0 | 80.5 | 10.2 |
| 端到端: question→首个play_start | 3 | 843.6 | 822.1 | 904.6 | 804.1 | 904.6 | 53.6 |
| 会话总时长: question→session_finished | 3 | 9771.6 | 6246.0 | 17374.7 | 5694.1 | 17374.7 | 6590.3 |
| 音频总时长(各段play相加) | 3 | 8923.3 | 5421.2 | 16561.5 | 4787.1 | 16561.5 | 6622.5 |
| 音频跨距: 首play_start→末play_end | 3 | 8926.9 | 5422.7 | 16569.4 | 4788.5 | 16569.4 | 6626.2 |

> 未采集到数据的指标（通常表示该链路环节未带埋点/未触发）：ASR: kws_detected→start_asr、ASR: start_asr→endpoint、ASR: endpoint→nurse_req、ASR: nurse_req→nurse_resp、ASR: nurse_resp→publish_question、ASR: endpoint→probe收到question、ASR: publish_question→probe收到、语音结束→首音 (endpoint-静音阈值)、问句播放结束→首音 (wav触发)

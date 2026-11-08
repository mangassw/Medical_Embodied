## 延迟统计 tag=chat mode=inject-text (n=1, 单位 ms)

| 阶段(stage) | n | mean | median | p95 | min | max | stdev |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Router: question→路由话题到达 | 1 | 1.1 | 1.1 | 1.1 | 1.1 | 1.1 | 0.0 |
| LLM: 路由话题→首块 | 1 | 882.1 | 882.1 | 882.1 | 882.1 | 882.1 | 0.0 |
| LLM TTFT: question→首块 | 1 | 883.2 | 883.2 | 883.2 | 883.2 | 883.2 | 0.0 |
| LLM 总时长: 首块→[DONE] | 1 | 505.3 | 505.3 | 505.3 | 505.3 | 505.3 | 0.0 |
| LLM: question→[DONE] | 1 | 1388.4 | 1388.4 | 1388.4 | 1388.4 | 1388.4 | 0.0 |
| 断句: LLM首块→首个TTS文本 | 1 | 224.4 | 224.4 | 224.4 | 224.4 | 224.4 | 0.0 |
| 断句: question→首个TTS文本 | 1 | 1107.6 | 1107.6 | 1107.6 | 1107.6 | 1107.6 | 0.0 |
| 断句: 首个TTS文本→首音 | 1 | 973.2 | 973.2 | 973.2 | 973.2 | 973.2 | 0.0 |
| TTS 首句合成: rx→synth_end | 1 | 942.8 | 942.8 | 942.8 | 942.8 | 942.8 | 0.0 |
| TTS 首音: rx→play_start | 1 | 973.0 | 973.0 | 973.0 | 973.0 | 973.0 | 0.0 |
| TTS 首句: synth_end→play_start | 1 | 30.1 | 30.1 | 30.1 | 30.1 | 30.1 | 0.0 |
| 端到端: question→首个play_start | 1 | 2080.8 | 2080.8 | 2080.8 | 2080.8 | 2080.8 | 0.0 |
| 会话总时长: question→session_finished | 1 | 8824.0 | 8824.0 | 8824.0 | 8824.0 | 8824.0 | 0.0 |
| 音频总时长(各段play相加) | 1 | 6740.8 | 6740.8 | 6740.8 | 6740.8 | 6740.8 | 0.0 |
| 音频跨距: 首play_start→末play_end | 1 | 6742.2 | 6742.2 | 6742.2 | 6742.2 | 6742.2 | 0.0 |

> 未采集到数据的指标（通常表示该链路环节未带埋点/未触发）：ASR: kws_detected→start_asr、ASR: start_asr→endpoint、ASR: endpoint→nurse_req、ASR: nurse_req→nurse_resp、ASR: nurse_resp→publish_question、ASR: endpoint→probe收到question、ASR: publish_question→probe收到、语音结束→首音 (endpoint-静音阈值)、问句播放结束→首音 (wav触发)

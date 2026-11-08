## 延迟统计 tag=judge1 mode=inject-text (n=1, 单位 ms)

| 阶段(stage) | n | mean | median | p95 | min | max | stdev |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Router: question→路由话题到达 | 1 | 1.2 | 1.2 | 1.2 | 1.2 | 1.2 | 0.0 |
| LLM: 路由话题→首块 | 1 | 1029.5 | 1029.5 | 1029.5 | 1029.5 | 1029.5 | 0.0 |
| LLM TTFT: question→首块 | 1 | 1030.7 | 1030.7 | 1030.7 | 1030.7 | 1030.7 | 0.0 |
| LLM 总时长: 首块→[DONE] | 1 | 540.9 | 540.9 | 540.9 | 540.9 | 540.9 | 0.0 |
| LLM: question→[DONE] | 1 | 1571.6 | 1571.6 | 1571.6 | 1571.6 | 1571.6 | 0.0 |
| 断句: LLM首块→首个TTS文本 | 1 | 157.0 | 157.0 | 157.0 | 157.0 | 157.0 | 0.0 |
| 断句: question→首个TTS文本 | 1 | 1187.8 | 1187.8 | 1187.8 | 1187.8 | 1187.8 | 0.0 |
| 断句: 首个TTS文本→首音 | 1 | 919.7 | 919.7 | 919.7 | 919.7 | 919.7 | 0.0 |
| TTS 首句合成: rx→synth_end | 1 | 873.1 | 873.1 | 873.1 | 873.1 | 873.1 | 0.0 |
| TTS 首音: rx→play_start | 1 | 919.4 | 919.4 | 919.4 | 919.4 | 919.4 | 0.0 |
| TTS 首句: synth_end→play_start | 1 | 46.2 | 46.2 | 46.2 | 46.2 | 46.2 | 0.0 |
| 端到端: question→首个play_start | 1 | 2107.5 | 2107.5 | 2107.5 | 2107.5 | 2107.5 | 0.0 |
| 会话总时长: question→session_finished | 1 | 18289.2 | 18289.2 | 18289.2 | 18289.2 | 18289.2 | 0.0 |
| 音频总时长(各段play相加) | 1 | 16179.4 | 16179.4 | 16179.4 | 16179.4 | 16179.4 | 0.0 |
| 音频跨距: 首play_start→末play_end | 1 | 16180.7 | 16180.7 | 16180.7 | 16180.7 | 16180.7 | 0.0 |

> 未采集到数据的指标（通常表示该链路环节未带埋点/未触发）：ASR: kws_detected→start_asr、ASR: start_asr→endpoint、ASR: endpoint→nurse_req、ASR: nurse_req→nurse_resp、ASR: nurse_resp→publish_question、ASR: endpoint→probe收到question、ASR: publish_question→probe收到、语音结束→首音 (endpoint-静音阈值)、问句播放结束→首音 (wav触发)

## 延迟统计 tag=emoji5 mode=inject-text (n=1, 单位 ms)

| 阶段(stage) | n | mean | median | p95 | min | max | stdev |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Router: question→路由话题到达 | 1 | 0.9 | 0.9 | 0.9 | 0.9 | 0.9 | 0.0 |
| LLM: 路由话题→首块 | 1 | 1761.2 | 1761.2 | 1761.2 | 1761.2 | 1761.2 | 0.0 |
| LLM TTFT: question→首块 | 1 | 1762.1 | 1762.1 | 1762.1 | 1762.1 | 1762.1 | 0.0 |
| LLM 总时长: 首块→[DONE] | 1 | 809.7 | 809.7 | 809.7 | 809.7 | 809.7 | 0.0 |
| LLM: question→[DONE] | 1 | 2571.8 | 2571.8 | 2571.8 | 2571.8 | 2571.8 | 0.0 |
| 断句: LLM首块→首个TTS文本 | 1 | 117.0 | 117.0 | 117.0 | 117.0 | 117.0 | 0.0 |
| 断句: question→首个TTS文本 | 1 | 1879.1 | 1879.1 | 1879.1 | 1879.1 | 1879.1 | 0.0 |
| 断句: 首个TTS文本→首音 | 1 | 750.4 | 750.4 | 750.4 | 750.4 | 750.4 | 0.0 |
| TTS 首句合成: rx→synth_end | 1 | 668.1 | 668.1 | 668.1 | 668.1 | 668.1 | 0.0 |
| TTS 首音: rx→play_start | 1 | 745.4 | 745.4 | 745.4 | 745.4 | 745.4 | 0.0 |
| TTS 首句: synth_end→play_start | 1 | 77.2 | 77.2 | 77.2 | 77.2 | 77.2 | 0.0 |
| 端到端: question→首个play_start | 1 | 2629.5 | 2629.5 | 2629.5 | 2629.5 | 2629.5 | 0.0 |
| 会话总时长: question→session_finished | 1 | 26154.9 | 26154.9 | 26154.9 | 26154.9 | 26154.9 | 0.0 |
| 音频总时长(各段play相加) | 1 | 23520.5 | 23520.5 | 23520.5 | 23520.5 | 23520.5 | 0.0 |
| 音频跨距: 首play_start→末play_end | 1 | 23524.4 | 23524.4 | 23524.4 | 23524.4 | 23524.4 | 0.0 |

> 未采集到数据的指标（通常表示该链路环节未带埋点/未触发）：ASR: kws_detected→start_asr、ASR: start_asr→endpoint、ASR: endpoint→nurse_req、ASR: nurse_req→nurse_resp、ASR: nurse_resp→publish_question、ASR: endpoint→probe收到question、ASR: publish_question→probe收到、语音结束→首音 (endpoint-静音阈值)、问句播放结束→首音 (wav触发)

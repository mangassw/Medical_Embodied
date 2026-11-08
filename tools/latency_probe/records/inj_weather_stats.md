## 延迟统计 tag=inj_weather mode=inject-text (n=3, 单位 ms)

| 阶段(stage) | n | mean | median | p95 | min | max | stdev |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Router: question→路由话题到达 | 3 | 1.2 | 1.2 | 1.4 | 0.9 | 1.4 | 0.3 |
| LLM: 路由话题→首块 | 3 | 913.0 | 739.0 | 1288.1 | 712.0 | 1288.1 | 325.1 |
| LLM TTFT: question→首块 | 3 | 914.2 | 740.2 | 1289.5 | 712.9 | 1289.5 | 325.3 |
| LLM 总时长: 首块→[DONE] | 3 | 1124.5 | 1127.7 | 1148.6 | 1097.2 | 1148.6 | 25.8 |
| LLM: question→[DONE] | 3 | 2038.7 | 1867.9 | 2438.1 | 1810.1 | 2438.1 | 347.1 |
| 断句: LLM首块→首个TTS文本 | 3 | 72.2 | 67.9 | 98.4 | 50.2 | 98.4 | 24.4 |
| 断句: question→首个TTS文本 | 3 | 986.4 | 838.6 | 1357.4 | 763.1 | 1357.4 | 323.5 |
| 断句: 首个TTS文本→首音 | 3 | 936.7 | 916.3 | 1017.2 | 876.6 | 1017.2 | 72.5 |
| TTS 首句合成: rx→synth_end | 3 | 863.1 | 872.8 | 925.1 | 791.3 | 925.1 | 67.4 |
| TTS 首音: rx→play_start | 3 | 935.7 | 913.8 | 1016.9 | 876.5 | 1016.9 | 72.7 |
| TTS 首句: synth_end→play_start | 3 | 72.6 | 85.1 | 91.8 | 41.0 | 91.8 | 27.6 |
| 端到端: question→首个play_start | 3 | 1923.1 | 1754.9 | 2374.6 | 1639.8 | 2374.6 | 395.2 |
| 会话总时长: question→session_finished | 3 | 17576.2 | 17411.2 | 18113.0 | 17204.4 | 18113.0 | 476.2 |
| 音频总时长(各段play相加) | 3 | 15645.8 | 15650.1 | 15729.3 | 15558.2 | 15729.3 | 85.7 |
| 音频跨距: 首play_start→末play_end | 3 | 15651.6 | 15655.3 | 15736.1 | 15563.3 | 15736.1 | 86.5 |

> 未采集到数据的指标（通常表示该链路环节未带埋点/未触发）：ASR: kws_detected→start_asr、ASR: start_asr→endpoint、ASR: endpoint→nurse_req、ASR: nurse_req→nurse_resp、ASR: nurse_resp→publish_question、ASR: endpoint→probe收到question、ASR: publish_question→probe收到、语音结束→首音 (endpoint-静音阈值)、问句播放结束→首音 (wav触发)

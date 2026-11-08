## 延迟统计 tag=p0_medical_app mode=inject-text (n=3, 单位 ms)

| 阶段(stage) | n | mean | median | p95 | min | max | stdev |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Router: question→路由话题到达 | 3 | 1.4 | 1.3 | 1.8 | 1.0 | 1.8 | 0.4 |
| LLM: 路由话题→首块 | 3 | 6851.1 | 6339.8 | 8169.7 | 6043.7 | 8169.7 | 1151.6 |
| LLM TTFT: question→首块 | 3 | 6852.5 | 6341.6 | 8171.1 | 6044.7 | 8171.1 | 1151.5 |
| LLM 总时长: 首块→[DONE] | 3 | 33.1 | 36.8 | 61.6 | 0.7 | 61.6 | 30.6 |
| LLM: question→[DONE] | 3 | 6885.5 | 6378.4 | 8171.8 | 6106.3 | 8171.8 | 1122.2 |
| 断句: LLM首块→首个TTS文本 | 3 | 33.0 | 36.5 | 62.2 | 0.4 | 62.2 | 31.0 |
| 断句: question→首个TTS文本 | 3 | 6885.5 | 6378.0 | 8171.5 | 6106.9 | 8171.5 | 1121.9 |
| 断句: 首个TTS文本→首音 | 3 | 1197.8 | 1175.3 | 1248.3 | 1169.9 | 1248.3 | 43.8 |
| TTS 首句合成: rx→synth_end | 3 | 1162.8 | 1149.4 | 1238.8 | 1100.2 | 1238.8 | 70.3 |
| TTS 首音: rx→play_start | 3 | 1198.0 | 1174.9 | 1248.1 | 1170.9 | 1248.1 | 43.5 |
| TTS 首句: synth_end→play_start | 3 | 35.2 | 21.5 | 74.8 | 9.3 | 74.8 | 34.8 |
| 端到端: question→首个play_start | 3 | 8083.3 | 7626.3 | 9346.8 | 7276.8 | 9346.8 | 1108.1 |
| 会话总时长: question→session_finished | 3 | 10787.2 | 10443.0 | 11910.5 | 10008.2 | 11910.5 | 996.8 |
| 音频总时长(各段play相加) | 3 | 2702.7 | 2730.2 | 2815.3 | 2562.5 | 2815.3 | 128.6 |
| 音频跨距: 首play_start→末play_end | 3 | 2702.7 | 2730.2 | 2815.3 | 2562.5 | 2815.3 | 128.6 |

> 未采集到数据的指标（通常表示该链路环节未带埋点/未触发）：ASR: kws_detected→start_asr、ASR: start_asr→endpoint、ASR: endpoint→nurse_req、ASR: nurse_req→nurse_resp、ASR: nurse_resp→publish_question、ASR: endpoint→probe收到question、ASR: publish_question→probe收到、语音结束→首音 (endpoint-静音阈值)、问句播放结束→首音 (wav触发)

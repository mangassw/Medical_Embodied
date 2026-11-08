## 延迟统计 tag=postfix mode=inject-text (n=5, 单位 ms)

| 阶段(stage) | n | mean | median | p95 | min | max | stdev |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Router: question→路由话题到达 | 5 | 1.1 | 1.1 | 1.6 | 0.9 | 1.6 | 0.3 |
| LLM: 路由话题→首块 | 5 | 1078.0 | 1114.9 | 1319.5 | 714.4 | 1319.5 | 240.0 |
| LLM TTFT: question→首块 | 5 | 1079.1 | 1115.7 | 1320.4 | 715.5 | 1320.4 | 240.1 |
| LLM 总时长: 首块→[DONE] | 5 | 825.9 | 819.4 | 918.1 | 719.6 | 918.1 | 72.8 |
| LLM: question→[DONE] | 5 | 1905.0 | 2033.8 | 2180.2 | 1435.1 | 2180.2 | 295.2 |
| 断句: LLM首块→首个TTS文本 | 5 | 68.7 | 83.3 | 105.4 | 30.6 | 105.4 | 34.7 |
| 断句: question→首个TTS文本 | 5 | 1147.8 | 1207.1 | 1425.7 | 748.3 | 1425.7 | 257.3 |
| 断句: 首个TTS文本→首音 | 5 | 635.5 | 620.9 | 722.2 | 581.7 | 722.2 | 53.8 |
| TTS 首句合成: rx→synth_end | 5 | 577.6 | 557.7 | 648.1 | 495.9 | 648.1 | 65.5 |
| TTS 首音: rx→play_start | 5 | 633.4 | 620.7 | 722.0 | 571.8 | 722.0 | 56.3 |
| TTS 首句: synth_end→play_start | 5 | 55.7 | 63.1 | 76.0 | 4.3 | 76.0 | 29.5 |
| 端到端: question→首个play_start | 5 | 1783.3 | 1853.1 | 2046.6 | 1330.0 | 2046.6 | 270.0 |
| 会话总时长: question→session_finished | 5 | 14211.5 | 13186.6 | 16686.2 | 12731.0 | 16686.2 | 1816.8 |
| 音频总时长(各段play相加) | 5 | 12421.4 | 11395.7 | 14788.1 | 11041.4 | 14788.1 | 1732.1 |
| 音频跨距: 首play_start→末play_end | 5 | 12427.1 | 11399.8 | 14793.6 | 11045.7 | 14793.6 | 1731.6 |

> 未采集到数据的指标（通常表示该链路环节未带埋点/未触发）：ASR: kws_detected→start_asr、ASR: start_asr→endpoint、ASR: endpoint→nurse_req、ASR: nurse_req→nurse_resp、ASR: nurse_resp→publish_question、ASR: endpoint→probe收到question、ASR: publish_question→probe收到、语音结束→首音 (endpoint-静音阈值)、问句播放结束→首音 (wav触发)

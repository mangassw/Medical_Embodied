## 延迟统计 tag=selftest_inject mode=inject-text (n=3, 单位 ms)

| 阶段(stage) | n | mean | median | p95 | min | max | stdev |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Router: question→路由话题到达 | 3 | 30.8 | 30.8 | 30.8 | 30.7 | 30.8 | 0.0 |
| LLM: 路由话题→首块 | 3 | 120.4 | 120.0 | 121.1 | 120.0 | 121.1 | 0.6 |
| LLM TTFT: question→首块 | 3 | 151.1 | 150.8 | 151.9 | 150.7 | 151.9 | 0.7 |
| LLM 总时长: 首块→[DONE] | 3 | 599.7 | 600.0 | 600.1 | 598.9 | 600.1 | 0.7 |
| LLM: question→[DONE] | 3 | 750.8 | 750.8 | 750.8 | 750.8 | 750.8 | 0.0 |
| 断句: LLM首块→首个TTS文本 | 3 | 199.6 | 200.0 | 200.0 | 198.8 | 200.0 | 0.7 |
| 断句: question→首个TTS文本 | 3 | 350.7 | 350.7 | 350.7 | 350.7 | 350.7 | 0.0 |
| 断句: 首个TTS文本→首音 | 3 | 1204.6 | 1204.7 | 1204.7 | 1204.6 | 1204.7 | 0.0 |
| TTS 首句合成: rx→synth_end | 3 | 800.0 | 800.0 | 800.0 | 800.0 | 800.0 | 0.0 |
| TTS 首音: rx→play_start | 3 | 1200.0 | 1200.0 | 1200.0 | 1200.0 | 1200.0 | 0.0 |
| TTS 首句: synth_end→play_start | 3 | 400.0 | 400.0 | 400.0 | 400.0 | 400.0 | 0.0 |
| 端到端: question→首个play_start | 3 | 1555.4 | 1555.4 | 1555.4 | 1555.3 | 1555.4 | 0.0 |
| 会话总时长: question→session_finished | 3 | 3405.8 | 3405.8 | 3405.8 | 3405.7 | 3405.8 | 0.0 |
| 音频总时长(各段play相加) | 3 | 2400.0 | 2400.0 | 2400.0 | 2400.0 | 2400.0 | 0.0 |
| 音频跨距: 首play_start→末play_end | 3 | 1800.0 | 1800.0 | 1800.0 | 1800.0 | 1800.0 | 0.0 |

> 未采集到数据的指标（通常表示该链路环节未带埋点/未触发）：ASR: kws_detected→start_asr、ASR: start_asr→endpoint、ASR: endpoint→nurse_req、ASR: nurse_req→nurse_resp、ASR: nurse_resp→publish_question、ASR: endpoint→probe收到question、ASR: publish_question→probe收到、语音结束→首音 (endpoint-静音阈值)、问句播放结束→首音 (wav触发)

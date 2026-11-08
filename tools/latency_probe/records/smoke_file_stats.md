## 延迟统计 tag=smoke_file mode=inject-text (n=2, 单位 ms)

| 阶段(stage) | n | mean | median | p95 | min | max | stdev |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Router: question→路由话题到达 | 2 | 2.9 | 2.9 | 4.8 | 1.1 | 4.8 | 2.6 |
| LLM: 路由话题→首块 | 2 | 970.1 | 970.1 | 1508.4 | 431.7 | 1508.4 | 761.3 |
| LLM TTFT: question→首块 | 2 | 973.0 | 973.0 | 1513.2 | 432.8 | 1513.2 | 763.9 |
| LLM 总时长: 首块→[DONE] | 2 | 774.4 | 774.4 | 822.5 | 726.3 | 822.5 | 68.0 |
| LLM: question→[DONE] | 2 | 1747.4 | 1747.4 | 2335.6 | 1159.1 | 2335.6 | 831.9 |
| 断句: LLM首块→首个TTS文本 | 2 | 54.0 | 54.0 | 77.4 | 30.6 | 77.4 | 33.1 |
| 断句: question→首个TTS文本 | 2 | 1027.0 | 1027.0 | 1590.5 | 463.4 | 1590.5 | 797.0 |
| 断句: 首个TTS文本→首音 | 2 | 488.5 | 488.5 | 560.9 | 416.1 | 560.9 | 102.3 |
| TTS 首句合成: rx→synth_end | 2 | 448.8 | 448.8 | 522.9 | 374.6 | 522.9 | 104.9 |
| TTS 首音: rx→play_start | 2 | 488.1 | 488.1 | 560.6 | 415.7 | 560.6 | 102.5 |
| TTS 首句: synth_end→play_start | 2 | 39.4 | 39.4 | 41.1 | 37.6 | 41.1 | 2.4 |
| 端到端: question→首个play_start | 2 | 1515.5 | 1515.5 | 2151.4 | 879.5 | 2151.4 | 899.3 |
| 会话总时长: question→session_finished | 2 | 13419.7 | 13419.7 | 13761.2 | 13078.3 | 13761.2 | 482.9 |
| 音频总时长(各段play相加) | 2 | 11679.7 | 11679.7 | 11754.6 | 11604.7 | 11754.6 | 106.0 |
| 音频跨距: 首play_start→末play_end | 2 | 11903.3 | 11903.3 | 12197.8 | 11608.7 | 12197.8 | 416.5 |

> 未采集到数据的指标（通常表示该链路环节未带埋点/未触发）：ASR: kws_detected→start_asr、ASR: start_asr→endpoint、ASR: endpoint→nurse_req、ASR: nurse_req→nurse_resp、ASR: nurse_resp→publish_question、ASR: endpoint→probe收到question、ASR: publish_question→probe收到、语音结束→首音 (endpoint-静音阈值)、问句播放结束→首音 (wav触发)

## 延迟统计 tag=single_smoke mode=inject-text (n=2, 单位 ms)

| 阶段(stage) | n | mean | median | p95 | min | max | stdev |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Router: question→路由话题到达 | 2 | 1.0 | 1.0 | 1.0 | 1.0 | 1.0 | 0.0 |
| LLM: 路由话题→首块 | 2 | 609.4 | 609.4 | 782.6 | 436.2 | 782.6 | 245.0 |
| LLM TTFT: question→首块 | 2 | 610.4 | 610.4 | 783.7 | 437.2 | 783.7 | 245.0 |
| LLM 总时长: 首块→[DONE] | 2 | 934.8 | 934.8 | 1163.3 | 706.3 | 1163.3 | 323.2 |
| LLM: question→[DONE] | 2 | 1545.2 | 1545.2 | 1600.5 | 1489.9 | 1600.5 | 78.2 |
| 断句: LLM首块→首个TTS文本 | 2 | 30.0 | 30.0 | 57.2 | 2.8 | 57.2 | 38.5 |
| 断句: question→首个TTS文本 | 2 | 640.4 | 640.4 | 840.9 | 440.0 | 840.9 | 283.5 |
| 断句: 首个TTS文本→首音 | 2 | 786.8 | 786.8 | 877.0 | 696.6 | 877.0 | 127.6 |
| TTS 首句合成: rx→synth_end | 2 | 726.0 | 726.0 | 787.4 | 664.7 | 787.4 | 86.8 |
| TTS 首音: rx→play_start | 2 | 775.0 | 775.0 | 853.5 | 696.5 | 853.5 | 111.0 |
| TTS 首句: synth_end→play_start | 2 | 48.9 | 48.9 | 66.0 | 31.8 | 66.0 | 24.2 |
| 端到端: question→首个play_start | 2 | 1427.3 | 1427.3 | 1717.9 | 1136.6 | 1717.9 | 411.1 |
| 会话总时长: question→session_finished | 2 | 10600.6 | 10600.6 | 12263.6 | 8937.6 | 12263.6 | 2351.8 |
| 音频总时长(各段play相加) | 2 | 9165.4 | 9165.4 | 11116.6 | 7214.1 | 11116.6 | 2759.5 |
| 音频跨距: 首play_start→末play_end | 2 | 9172.4 | 9172.4 | 11126.1 | 7218.7 | 11126.1 | 2763.0 |

> 未采集到数据的指标（通常表示该链路环节未带埋点/未触发）：ASR: kws_detected→start_asr、ASR: start_asr→endpoint、ASR: endpoint→nurse_req、ASR: nurse_req→nurse_resp、ASR: nurse_resp→publish_question、ASR: endpoint→probe收到question、ASR: publish_question→probe收到、语音结束→首音 (endpoint-静音阈值)、问句播放结束→首音 (wav触发)

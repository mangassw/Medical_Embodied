## 延迟统计 tag=ds_kb10 mode=inject-text (n=10, 单位 ms)

| 阶段(stage) | n | mean | median | p95 | min | max | stdev |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Router: question→路由话题到达 | 10 | 0.9 | 0.9 | 1.1 | 0.8 | 1.1 | 0.1 |
| LLM: 路由话题→首块 | 10 | 839.4 | 604.8 | 1681.9 | 447.0 | 1681.9 | 439.0 |
| LLM TTFT: question→首块 | 10 | 840.3 | 605.7 | 1682.7 | 447.8 | 1682.7 | 439.0 |
| LLM 总时长: 首块→[DONE] | 10 | 320.1 | 273.4 | 741.5 | 142.9 | 741.5 | 195.0 |
| LLM: question→[DONE] | 10 | 1160.4 | 929.4 | 2112.2 | 590.7 | 2112.2 | 566.3 |
| 断句: LLM首块→首个TTS文本 | 10 | 52.9 | 51.9 | 87.3 | 19.4 | 87.3 | 21.8 |
| 断句: question→首个TTS文本 | 10 | 893.2 | 641.3 | 1755.3 | 504.7 | 1755.3 | 448.7 |
| 断句: 首个TTS文本→首音 | 10 | 743.7 | 770.9 | 893.9 | 538.9 | 893.9 | 137.8 |
| TTS 首句合成: rx→synth_end | 10 | 678.9 | 706.5 | 793.9 | 480.2 | 793.9 | 123.8 |
| TTS 首音: rx→play_start | 10 | 741.8 | 770.5 | 892.3 | 541.4 | 892.3 | 135.2 |
| TTS 首句: synth_end→play_start | 10 | 62.9 | 60.9 | 99.3 | 19.8 | 99.3 | 25.5 |
| 端到端: question→首个play_start | 10 | 1636.9 | 1447.1 | 2409.5 | 1156.6 | 2409.5 | 414.9 |
| 会话总时长: question→session_finished | 10 | 10472.1 | 12139.3 | 15132.0 | 5482.7 | 15132.0 | 4135.2 |
| 音频总时长(各段play相加) | 10 | 8827.7 | 10352.0 | 13351.1 | 4080.2 | 13351.1 | 4024.7 |
| 音频跨距: 首play_start→末play_end | 10 | 8834.0 | 10354.5 | 13394.3 | 4082.7 | 13394.3 | 4030.7 |

> 未采集到数据的指标（通常表示该链路环节未带埋点/未触发）：ASR: kws_detected→start_asr、ASR: start_asr→endpoint、ASR: endpoint→nurse_req、ASR: nurse_req→nurse_resp、ASR: nurse_resp→publish_question、ASR: endpoint→probe收到question、ASR: publish_question→probe收到、语音结束→首音 (endpoint-静音阈值)、问句播放结束→首音 (wav触发)

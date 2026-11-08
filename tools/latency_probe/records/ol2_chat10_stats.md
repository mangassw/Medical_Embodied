## 延迟统计 tag=ol2_chat10 mode=inject-text (n=10, 单位 ms)

| 阶段(stage) | n | mean | median | p95 | min | max | stdev |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Router: question→路由话题到达 | 10 | 1.0 | 0.9 | 1.3 | 0.8 | 1.3 | 0.1 |
| LLM: 路由话题→首块 | 10 | 185.0 | 169.6 | 353.4 | 152.2 | 353.4 | 59.5 |
| LLM TTFT: question→首块 | 10 | 186.0 | 170.6 | 354.2 | 153.1 | 354.2 | 59.5 |
| LLM 总时长: 首块→[DONE] | 10 | 618.8 | 617.5 | 1046.0 | 235.6 | 1046.0 | 236.5 |
| LLM: question→[DONE] | 10 | 804.8 | 779.8 | 1215.8 | 572.5 | 1215.8 | 208.5 |
| 断句: LLM首块→首个TTS文本 | 10 | 66.9 | 52.3 | 139.3 | 35.2 | 139.3 | 30.7 |
| 断句: question→首个TTS文本 | 10 | 253.0 | 226.7 | 389.4 | 205.2 | 389.4 | 56.3 |
| 断句: 首个TTS文本→首音 | 10 | 893.0 | 843.5 | 1265.8 | 611.4 | 1265.8 | 196.5 |
| TTS 首句合成: rx→synth_end | 10 | 825.9 | 814.2 | 1244.0 | 532.3 | 1244.0 | 213.7 |
| TTS 首音: rx→play_start | 10 | 887.9 | 820.4 | 1265.5 | 611.1 | 1265.5 | 198.3 |
| TTS 首句: synth_end→play_start | 10 | 62.0 | 72.4 | 99.0 | 4.1 | 99.0 | 37.6 |
| 端到端: question→首个play_start | 10 | 1145.9 | 1053.2 | 1569.7 | 908.9 | 1569.7 | 202.7 |
| 会话总时长: question→session_finished | 10 | 7148.7 | 7248.7 | 11058.8 | 4078.2 | 11058.8 | 2013.4 |
| 音频总时长(各段play相加) | 10 | 5999.7 | 6118.2 | 9701.2 | 3075.7 | 9701.2 | 1891.2 |
| 音频跨距: 首play_start→末play_end | 10 | 6001.8 | 6119.9 | 9704.3 | 3076.4 | 9704.3 | 1891.7 |

> 未采集到数据的指标（通常表示该链路环节未带埋点/未触发）：ASR: kws_detected→start_asr、ASR: start_asr→endpoint、ASR: endpoint→nurse_req、ASR: nurse_req→nurse_resp、ASR: nurse_resp→publish_question、ASR: endpoint→probe收到question、ASR: publish_question→probe收到、语音结束→首音 (endpoint-静音阈值)、问句播放结束→首音 (wav触发)

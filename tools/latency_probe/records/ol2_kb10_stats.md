## 延迟统计 tag=ol2_kb10 mode=inject-text (n=10, 单位 ms)

| 阶段(stage) | n | mean | median | p95 | min | max | stdev |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Router: question→路由话题到达 | 9 | 1.0 | 0.9 | 1.5 | 0.8 | 1.5 | 0.2 |
| LLM: 路由话题→首块 | 9 | 558.4 | 478.2 | 1062.0 | 270.1 | 1062.0 | 237.0 |
| LLM TTFT: question→首块 | 9 | 559.4 | 479.1 | 1063.4 | 270.9 | 1063.4 | 237.1 |
| LLM 总时长: 首块→[DONE] | 9 | 975.2 | 909.4 | 1202.9 | 688.9 | 1202.9 | 170.8 |
| LLM: question→[DONE] | 9 | 1534.6 | 1487.0 | 1972.8 | 1306.0 | 1972.8 | 231.0 |
| 断句: LLM首块→首个TTS文本 | 9 | 99.9 | 55.2 | 214.8 | 51.9 | 214.8 | 65.7 |
| 断句: question→首个TTS文本 | 9 | 659.3 | 571.8 | 1149.3 | 408.5 | 1149.3 | 229.1 |
| 断句: 首个TTS文本→首音 | 9 | 1157.6 | 938.8 | 1952.3 | 771.9 | 1952.3 | 403.0 |
| TTS 首句合成: rx→synth_end | 9 | 1115.2 | 894.5 | 1873.1 | 760.3 | 1873.1 | 396.7 |
| TTS 首音: rx→play_start | 9 | 1155.4 | 936.2 | 1952.1 | 771.5 | 1952.1 | 402.5 |
| TTS 首句: synth_end→play_start | 9 | 40.2 | 18.5 | 98.7 | 5.6 | 98.7 | 38.1 |
| 端到端: question→首个play_start | 9 | 1816.9 | 1682.2 | 2524.1 | 1277.9 | 2524.1 | 482.6 |
| 会话总时长: question→session_finished | 9 | 11273.1 | 11072.8 | 13982.7 | 9276.7 | 13982.7 | 1629.3 |
| 音频总时长(各段play相加) | 9 | 9416.8 | 9284.7 | 11686.7 | 7916.2 | 11686.7 | 1418.9 |
| 音频跨距: 首play_start→末play_end | 9 | 9454.9 | 9287.2 | 12010.2 | 7918.6 | 12010.2 | 1485.5 |

> 未采集到数据的指标（通常表示该链路环节未带埋点/未触发）：ASR: kws_detected→start_asr、ASR: start_asr→endpoint、ASR: endpoint→nurse_req、ASR: nurse_req→nurse_resp、ASR: nurse_resp→publish_question、ASR: endpoint→probe收到question、ASR: publish_question→probe收到、语音结束→首音 (endpoint-静音阈值)、问句播放结束→首音 (wav触发)

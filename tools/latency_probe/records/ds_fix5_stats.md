## 延迟统计 tag=ds_fix5 mode=inject-text (n=5, 单位 ms)

| 阶段(stage) | n | mean | median | p95 | min | max | stdev |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Router: question→路由话题到达 | 5 | 1.0 | 1.0 | 1.3 | 0.8 | 1.3 | 0.2 |
| LLM: 路由话题→首块 | 5 | 711.4 | 759.8 | 810.6 | 546.1 | 810.6 | 115.4 |
| LLM TTFT: question→首块 | 5 | 712.4 | 760.8 | 811.5 | 547.1 | 811.5 | 115.5 |
| LLM 总时长: 首块→[DONE] | 5 | 281.7 | 326.3 | 422.2 | 129.6 | 422.2 | 129.7 |
| LLM: question→[DONE] | 5 | 994.1 | 964.7 | 1233.7 | 707.7 | 1233.7 | 214.1 |
| 断句: LLM首块→首个TTS文本 | 5 | 55.1 | 56.3 | 85.7 | 21.3 | 85.7 | 26.9 |
| 断句: question→首个TTS文本 | 5 | 767.4 | 796.7 | 860.4 | 623.2 | 860.4 | 95.5 |
| 断句: 首个TTS文本→首音 | 5 | 812.0 | 824.3 | 1045.1 | 614.7 | 1045.1 | 156.2 |
| TTS 首句合成: rx→synth_end | 5 | 736.0 | 746.9 | 968.5 | 586.5 | 968.5 | 150.6 |
| TTS 首音: rx→play_start | 5 | 803.1 | 803.2 | 1043.1 | 614.3 | 1043.1 | 157.6 |
| TTS 首句: synth_end→play_start | 5 | 67.1 | 67.9 | 109.1 | 27.8 | 109.1 | 29.5 |
| 端到端: question→首个play_start | 5 | 1579.5 | 1621.0 | 1769.2 | 1372.2 | 1769.2 | 165.6 |
| 会话总时长: question→session_finished | 5 | 11896.4 | 11839.6 | 18727.8 | 5391.3 | 18727.8 | 6253.0 |
| 音频总时长(各段play相加) | 5 | 10312.4 | 10066.3 | 17033.2 | 4017.2 | 17033.2 | 6209.2 |
| 音频跨距: 首play_start→末play_end | 5 | 10315.8 | 10069.1 | 17039.4 | 4017.9 | 17039.4 | 6211.9 |

> 未采集到数据的指标（通常表示该链路环节未带埋点/未触发）：ASR: kws_detected→start_asr、ASR: start_asr→endpoint、ASR: endpoint→nurse_req、ASR: nurse_req→nurse_resp、ASR: nurse_resp→publish_question、ASR: endpoint→probe收到question、ASR: publish_question→probe收到、语音结束→首音 (endpoint-静音阈值)、问句播放结束→首音 (wav触发)

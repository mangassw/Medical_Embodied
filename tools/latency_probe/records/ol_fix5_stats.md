## 延迟统计 tag=ol_fix5 mode=inject-text (n=5, 单位 ms)

| 阶段(stage) | n | mean | median | p95 | min | max | stdev |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Router: question→路由话题到达 | 5 | 2.7 | 1.0 | 9.8 | 0.8 | 9.8 | 4.0 |
| LLM: 路由话题→首块 | 5 | 695.6 | 492.7 | 1345.5 | 314.6 | 1345.5 | 449.6 |
| LLM TTFT: question→首块 | 5 | 698.3 | 493.6 | 1346.5 | 315.4 | 1346.5 | 451.1 |
| LLM 总时长: 首块→[DONE] | 5 | 383.6 | 241.4 | 784.6 | 124.9 | 784.6 | 293.5 |
| LLM: question→[DONE] | 5 | 1081.9 | 952.6 | 2131.1 | 556.8 | 2131.1 | 634.9 |
| 断句: LLM首块→首个TTS文本 | 5 | 134.9 | 143.2 | 225.4 | 53.2 | 225.4 | 62.9 |
| 断句: question→首个TTS文本 | 5 | 833.2 | 600.5 | 1489.7 | 368.5 | 1489.7 | 463.0 |
| 断句: 首个TTS文本→首音 | 5 | 1157.8 | 1080.7 | 1756.2 | 689.6 | 1756.2 | 397.5 |
| TTS 首句合成: rx→synth_end | 5 | 1123.4 | 1043.2 | 1727.9 | 621.1 | 1727.9 | 409.1 |
| TTS 首音: rx→play_start | 5 | 1157.9 | 1077.7 | 1755.9 | 689.3 | 1755.9 | 398.0 |
| TTS 首句: synth_end→play_start | 5 | 34.5 | 28.0 | 68.2 | 15.3 | 68.2 | 20.1 |
| 端到端: question→首个play_start | 5 | 1991.0 | 2213.3 | 2774.9 | 1058.1 | 2774.9 | 674.6 |
| 会话总时长: question→session_finished | 5 | 6941.9 | 5103.6 | 11519.9 | 4139.5 | 11519.9 | 3458.0 |
| 音频总时长(各段play相加) | 5 | 4949.3 | 3117.4 | 8742.2 | 2560.5 | 8742.2 | 2910.4 |
| 音频跨距: 首play_start→末play_end | 5 | 4949.9 | 3118.1 | 8743.8 | 2560.5 | 8743.8 | 2911.0 |

> 未采集到数据的指标（通常表示该链路环节未带埋点/未触发）：ASR: kws_detected→start_asr、ASR: start_asr→endpoint、ASR: endpoint→nurse_req、ASR: nurse_req→nurse_resp、ASR: nurse_resp→publish_question、ASR: endpoint→probe收到question、ASR: publish_question→probe收到、语音结束→首音 (endpoint-静音阈值)、问句播放结束→首音 (wav触发)

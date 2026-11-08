## 延迟统计 tag=ds_chat10 mode=inject-text (n=10, 单位 ms)

| 阶段(stage) | n | mean | median | p95 | min | max | stdev |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Router: question→路由话题到达 | 10 | 1.3 | 0.9 | 3.6 | 0.8 | 3.6 | 0.9 |
| LLM: 路由话题→首块 | 10 | 853.6 | 782.4 | 1331.3 | 456.9 | 1331.3 | 249.8 |
| LLM TTFT: question→首块 | 10 | 854.9 | 783.7 | 1332.4 | 457.8 | 1332.4 | 249.5 |
| LLM 总时长: 首块→[DONE] | 10 | 304.5 | 283.4 | 745.5 | 46.8 | 745.5 | 192.9 |
| LLM: question→[DONE] | 10 | 1159.4 | 1149.6 | 1379.2 | 876.6 | 1379.2 | 181.6 |
| 断句: LLM首块→首个TTS文本 | 10 | 96.7 | 68.2 | 336.8 | 0.5 | 336.8 | 104.5 |
| 断句: question→首个TTS文本 | 10 | 951.7 | 956.0 | 1332.8 | 551.1 | 1332.8 | 216.5 |
| 断句: 首个TTS文本→首音 | 10 | 648.8 | 609.8 | 858.3 | 497.3 | 858.3 | 130.3 |
| TTS 首句合成: rx→synth_end | 10 | 591.5 | 549.9 | 807.3 | 451.1 | 807.3 | 126.7 |
| TTS 首音: rx→play_start | 10 | 645.1 | 597.8 | 857.9 | 497.1 | 857.9 | 130.5 |
| TTS 首句: synth_end→play_start | 10 | 53.6 | 56.3 | 90.5 | 6.1 | 90.5 | 27.5 |
| 端到端: question→首个play_start | 10 | 1600.4 | 1647.8 | 1852.6 | 1332.8 | 1852.6 | 208.8 |
| 会话总时长: question→session_finished | 10 | 9282.5 | 8558.1 | 14275.9 | 6336.7 | 14275.9 | 2474.9 |
| 音频总时长(各段play相加) | 10 | 7677.6 | 6936.2 | 12849.1 | 4740.9 | 12849.1 | 2533.5 |
| 音频跨距: 首play_start→末play_end | 10 | 7681.0 | 6938.4 | 12854.3 | 4743.1 | 12854.3 | 2535.3 |

> 未采集到数据的指标（通常表示该链路环节未带埋点/未触发）：ASR: kws_detected→start_asr、ASR: start_asr→endpoint、ASR: endpoint→nurse_req、ASR: nurse_req→nurse_resp、ASR: nurse_resp→publish_question、ASR: endpoint→probe收到question、ASR: publish_question→probe收到、语音结束→首音 (endpoint-静音阈值)、问句播放结束→首音 (wav触发)

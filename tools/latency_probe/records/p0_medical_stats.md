## 延迟统计 tag=p0_medical mode=inject-text (n=3, 单位 ms)

| 阶段(stage) | n | mean | median | p95 | min | max | stdev |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Router: question→路由话题到达 | 3 | 3.7 | 0.9 | 9.5 | 0.8 | 9.5 | 5.0 |
| LLM: 路由话题→首块 | 3 | 1113.5 | 1218.3 | 1639.5 | 482.7 | 1639.5 | 585.5 |
| LLM TTFT: question→首块 | 3 | 1117.2 | 1219.3 | 1648.9 | 483.5 | 1648.9 | 589.4 |
| LLM 总时长: 首块→[DONE] | 3 | 965.1 | 939.1 | 1040.6 | 915.5 | 1040.6 | 66.5 |
| LLM: question→[DONE] | 3 | 2082.3 | 2134.8 | 2588.0 | 1524.1 | 2588.0 | 533.9 |
| 断句: LLM首块→首个TTS文本 | 3 | 174.8 | 152.6 | 278.4 | 93.5 | 278.4 | 94.5 |
| 断句: question→首个TTS文本 | 3 | 1292.1 | 1497.7 | 1742.4 | 636.1 | 1742.4 | 581.1 |
| 断句: 首个TTS文本→首音 | 3 | 1360.2 | 1405.8 | 1493.2 | 1181.6 | 1493.2 | 160.8 |
| TTS 首句合成: rx→synth_end | 3 | 1247.8 | 1214.6 | 1408.0 | 1120.9 | 1408.0 | 146.4 |
| TTS 首音: rx→play_start | 3 | 1351.0 | 1378.4 | 1493.2 | 1181.4 | 1493.2 | 157.7 |
| TTS 首句: synth_end→play_start | 3 | 103.2 | 85.2 | 163.8 | 60.5 | 163.8 | 53.9 |
| 端到端: question→首个play_start | 3 | 2652.2 | 2924.0 | 2990.9 | 2041.9 | 2990.9 | 529.7 |
| 会话总时长: question→session_finished | 3 | 17150.3 | 17419.3 | 17580.4 | 16451.4 | 17580.4 | 610.6 |
| 音频总时长(各段play相加) | 3 | 14491.5 | 14488.2 | 14585.5 | 14400.7 | 14585.5 | 92.4 |
| 音频跨距: 首play_start→末play_end | 3 | 14497.1 | 14494.2 | 14588.4 | 14408.6 | 14588.4 | 90.0 |

> 未采集到数据的指标（通常表示该链路环节未带埋点/未触发）：ASR: kws_detected→start_asr、ASR: start_asr→endpoint、ASR: endpoint→nurse_req、ASR: nurse_req→nurse_resp、ASR: nurse_resp→publish_question、ASR: endpoint→probe收到question、ASR: publish_question→probe收到、语音结束→首音 (endpoint-静音阈值)、问句播放结束→首音 (wav触发)

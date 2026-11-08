## 延迟统计 tag=opt2_chatgen mode=inject-text (n=10, 单位 ms)

| 阶段(stage) | n | mean | median | p95 | min | max | stdev |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Router: question→路由话题到达 | 10 | 1.0 | 1.0 | 1.1 | 0.8 | 1.1 | 0.1 |
| LLM: 路由话题→首块 | 10 | 1466.3 | 1040.6 | 2701.9 | 767.2 | 2701.9 | 752.4 |
| LLM TTFT: question→首块 | 10 | 1467.3 | 1041.5 | 2702.9 | 768.1 | 2702.9 | 752.5 |
| LLM 总时长: 首块→[DONE] | 10 | 914.0 | 868.7 | 1404.5 | 483.3 | 1404.5 | 293.4 |
| LLM: question→[DONE] | 10 | 2381.3 | 1954.6 | 4107.4 | 1455.8 | 4107.4 | 918.8 |
| 断句: LLM首块→首个TTS文本 | 10 | 223.9 | 205.2 | 474.0 | 41.1 | 474.0 | 141.6 |
| 断句: question→首个TTS文本 | 10 | 1691.2 | 1260.9 | 2855.4 | 903.5 | 2855.4 | 733.3 |
| 断句: 首个TTS文本→首音 | 10 | 873.0 | 793.8 | 1191.2 | 629.5 | 1191.2 | 189.1 |
| TTS 首句合成: rx→synth_end | 10 | 796.9 | 735.3 | 1113.4 | 598.0 | 1113.4 | 166.3 |
| TTS 首音: rx→play_start | 10 | 866.9 | 793.6 | 1191.0 | 629.2 | 1191.0 | 183.6 |
| TTS 首句: synth_end→play_start | 10 | 70.0 | 80.9 | 99.7 | 13.8 | 99.7 | 29.2 |
| 端到端: question→首个play_start | 10 | 2564.3 | 2333.6 | 3760.5 | 1650.2 | 3760.5 | 765.5 |
| 会话总时长: question→session_finished | 10 | 11551.1 | 11924.2 | 16838.3 | 7779.6 | 16838.3 | 2658.0 |
| 音频总时长(各段play相加) | 10 | 8983.2 | 9405.6 | 13539.2 | 4426.6 | 13539.2 | 2491.5 |
| 音频跨距: 首play_start→末play_end | 10 | 8985.3 | 9407.4 | 13544.5 | 4427.6 | 13544.5 | 2492.4 |

> 未采集到数据的指标（通常表示该链路环节未带埋点/未触发）：ASR: kws_detected→start_asr、ASR: start_asr→endpoint、ASR: endpoint→nurse_req、ASR: nurse_req→nurse_resp、ASR: nurse_resp→publish_question、ASR: endpoint→probe收到question、ASR: publish_question→probe收到、语音结束→首音 (endpoint-静音阈值)、问句播放结束→首音 (wav触发)

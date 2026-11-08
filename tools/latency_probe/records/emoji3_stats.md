## 延迟统计 tag=emoji3 mode=inject-text (n=1, 单位 ms)

| 阶段(stage) | n | mean | median | p95 | min | max | stdev |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |

> 未采集到数据的指标（通常表示该链路环节未带埋点/未触发）：ASR: kws_detected→start_asr、ASR: start_asr→endpoint、ASR: endpoint→nurse_req、ASR: nurse_req→nurse_resp、ASR: nurse_resp→publish_question、ASR: endpoint→probe收到question、ASR: publish_question→probe收到、Router: question→路由话题到达、LLM: 路由话题→首块、LLM TTFT: question→首块、LLM 总时长: 首块→[DONE]、LLM: question→[DONE]、断句: LLM首块→首个TTS文本、断句: question→首个TTS文本、断句: 首个TTS文本→首音、TTS 首句合成: rx→synth_end、TTS 首音: rx→play_start、TTS 首句: synth_end→play_start、端到端: question→首个play_start、语音结束→首音 (endpoint-静音阈值)、问句播放结束→首音 (wav触发)、会话总时长: question→session_finished、音频总时长(各段play相加)、音频跨距: 首play_start→末play_end

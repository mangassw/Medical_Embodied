# 语音全链路回归测试

`real_pipeline_record_v7.py` 在壁钟时间轴上真实跑一遍
问题注入 → 端点 → ASR → 纠错 → RAG(BM25) → LLM 流式 → 断句 → TTS(填充词)，
录音中的静音即真实生成延迟，回答结束后停 3s 再问下一题。

## 运行（不需要 rag_service，检索为进程内 BM25）

```bash
PYTHONNOUSERSITE=1 \
  /home/artrc/miniconda3/envs/qwen_tts_online/bin/python \
  tools/voice_regression/real_pipeline_record_v7.py
```

- 输出：`models/testaudio/qa_fullchain_v7_qwen3tts.wav` + `/tmp/tts_bench/realtime_timings_v7.json`
- 换问题：改脚本内 `QUESTIONS` 列表
- 关键产物：逐轮 ASR/RAG/TTFT/出音耗时（timings json）、分段音频（v7_segments/）
- 验证要点：3s 间隔 RMS=0、数字朗读（ASR 回环）、纠错与规范化日志

历版录音（v2~v6）已清理，本脚本是当前链路的唯一回归工具。

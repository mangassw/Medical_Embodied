#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""用本地 Qwen3-TTS 生成延迟测量用的问题音频（24kHz 单声道 wav）。

生成的 wav 会作为“虚拟麦克风”的输入，模拟用户说话。
用法：
  /home/artrc/miniconda3/envs/qwen_tts_online/bin/python gen_test_wavs.py
"""

import os
import sys
import time

import numpy as np
import scipy.io.wavfile as wavfile
import torch
from qwen_tts import Qwen3TTSModel

sys.path.insert(0, "/home/artrc/medical_ws/Medical_Embodied_dev/src/llm_node_py/llm_node_py")
from tts_fast_generate import fast_generate_custom_voice  # noqa: E402

MODEL_PATH = "/home/artrc/medical_ws/Medical_Embodied_dev/src/llm_node_py/Qwen3-TTS-12Hz-0.6B-CustomVoice"
OUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "audio")

# 与 tts 节点保持一致的采样参数
TTS_GEN_KWARGS = dict(
    do_sample=True,
    top_k=50,
    top_p=1.0,
    temperature=0.9,
    repetition_penalty=1.05,
)

# 文件名 -> 文本
TEXTS = {
    # 联网大模型（关键词“天气”走 /llm_to_openai）
    "q_weather.wav": "天气今天如何？",
    # 医疗知识库（走 /llm_to_dashapp）
    "q_medical_exercise.wav": "肾病患者运动时有哪些注意事项？",
    "q_medical_water.wav": "透析患者饮水量应该如何控制？",
    # 唤醒词候选（与 KWS 模型 keywords.txt 中的拼音条目对应）
    # 急症安全测试（应走同步 nurse 路径并立即呼叫护士）
    "q_emergency_chest.wav": "透析的时候胸口疼，喘不上气，怎么办？",
    # 用户想结束对话
    "q_end_dialog.wav": "好了，先这样吧，不问了。",
    # 句末语气词版本（用于验证“语义端点”提前判停：以“吗”结尾）
    "u05b_needle_water_ma.wav": "针眼沾水要紧吗？",
    "u16b_catheter_shower_ma.wav": "身上的导管能洗澡吗？",
    "wake_xiaoyi.wav": "小艺小艺",
    "wake_xiaoyi_doctor.wav": "小医小医",
}


def synth(model, text):
    try:
        wavs, sr = fast_generate_custom_voice(
            model, text, language="Chinese", speaker="Serena", **TTS_GEN_KWARGS
        )
    except Exception as exc:  # 回退官方路径
        print(f"[warn] fast path failed ({exc}), fallback to official generate")
        wavs, sr = model.generate_custom_voice(
            text=text, language="Chinese", speaker="Serena", **TTS_GEN_KWARGS
        )
    audio = np.asarray(wavs[0], dtype=np.float32)
    return audio, sr


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    print(f"loading model: {MODEL_PATH}")
    t0 = time.time()
    try:
        model = Qwen3TTSModel.from_pretrained(
            MODEL_PATH, device_map="cuda:0", dtype=torch.bfloat16,
            attn_implementation="flash_attention_2",
        )
    except ImportError:
        model = Qwen3TTSModel.from_pretrained(
            MODEL_PATH, device_map="cuda:0", dtype=torch.bfloat16
        )
    print(f"model loaded in {time.time()-t0:.1f}s")
    time.sleep(0.2)

    for filename, text in TEXTS.items():
        t = time.time()
        with torch.inference_mode():
            audio, sr = synth(model, text)
        out = os.path.join(OUT_DIR, filename)
        wavfile.write(out, sr, (np.clip(audio, -1.0, 1.0) * 32767).astype(np.int16))
        print(
            f"{filename}: {text} | {len(audio)/sr:.2f}s audio @ {sr}Hz | "
            f"synth {time.time()-t:.2f}s -> {out}",
            flush=True,
        )

    print("DONE")


if __name__ == "__main__":
    main()

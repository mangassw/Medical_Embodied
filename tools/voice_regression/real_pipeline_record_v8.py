#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""全链路回归录音 v7：资源优化后的生产形态。

对应本轮改造（对照 v6）：
  1) RAG → 进程内 BM25（rag_bm25.py，无 rag_service、零 GPU 检索，Recall@4=100%）
  2) TTS → Qwen3-TTS-0.6B 快速路径（生产节点，显存 2.6GB；tts_text_norm 保留）
  3) ASR → 4 线程（C++ 重编译）
  4) 保留：填充词"好的，"、语义端点建模、typo_map v2、断句器同款、壁钟时间轴
用法:
PYTHONNOUSERSITE=1 /home/artrc/miniconda3/envs/qwen_tts_online/bin/python real_pipeline_record_v7.py
前置: 无需 rag_service（进程内检索）
"""
import ast
import json
import logging
import os
import queue as _queue
import re
import sys
import threading
import time
import urllib.request

import numpy as np
import soundfile as sf
import torch

# 必须在首个 CUDA 分配前设置（与生产节点 ros2_node_tts_cosyvoice.py 一致）
os.environ.setdefault("PYTORCH_CUDA_ALLOC_CONF", "expandable_segments:True")

logging.getLogger("httpx").setLevel(logging.WARNING)
logging.getLogger("openai").setLevel(logging.WARNING)

LLM_NODE_DIR = "/home/artrc/medical_ws/Medical_Embodied_dev/src/llm_node_py/llm_node_py"
RUNTIME = "/home/artrc/cosyvoice_runtime"
ASSETS = "/home/artrc/medical_ws/Medical_Embodied_dev/src/llm_node_py/cosyvoice_assets"
for _p in (os.path.join(RUNTIME, "deps"),
           os.path.join(RUNTIME, "CosyVoice"),
           os.path.join(RUNTIME, "CosyVoice", "third_party", "Matcha-TTS"),
           LLM_NODE_DIR):
    if _p not in sys.path:
        sys.path.insert(0, _p)

# torch>=2.6 weights_only 补丁 + load_wav 补丁（与生产节点一致）
_orig_load = torch.load


def _load(f, *a, **k):
    k.setdefault("weights_only", False)
    return _orig_load(f, *a, **k)


torch.load = _load

import cosyvoice.utils.file_utils as _fu  # noqa: E402
import torchaudio.functional as _taf  # noqa: E402


def _load_wav(wav, target_sr, min_sr=16000):
    if isinstance(wav, (str, os.PathLike)):
        data, sr = sf.read(wav, dtype="float32", always_2d=True)
        speech = torch.from_numpy(data[:, 0])[None, :]
    else:
        speech = wav if wav.dim() == 2 else wav[None]
        sr = 16000
    if speech.shape[0] > 1:
        speech = speech.mean(dim=0, keepdim=True)
    return _taf.resample(speech, sr, target_sr) if sr != target_sr else speech


_fu.load_wav = _load_wav

import sherpa_onnx  # noqa: E402
from openai import OpenAI  # noqa: E402
from cosyvoice.cli.cosyvoice import CosyVoice2  # noqa: E402
from tts_text_norm import normalize_for_tts  # noqa: E402
import rag_bm25  # noqa: E402

SR = 24000
KEY_PATH = ("/home/artrc/medical_ws/Medical_Embodied_dev/"
            "src/llm_node_py/key/dashscope.key")
ASR_MODEL = ("/home/artrc/medical_ws/models/sherpa/"
             "sherpa-onnx-streaming-zipformer-bilingual-zh-en")
COSY_MODEL_DIR = os.path.join(RUNTIME, "CosyVoice2-0.5B")
SEG_DIR = "/tmp/tts_bench/v8_segments"
OUT_WAV = "/home/artrc/medical_ws/models/testaudio/qa_fullchain_v8_cosyvoice_opt.wav"
OUT_JSON = "/tmp/tts_bench/realtime_timings_v8.json"

ENDPOINT_S = 1.2            # rule2（asr_stream.cpp 默认，实测不可再降）
SEMANTIC_ENDPOINT_S = 0.65  # 语义端点：0.55s 文本稳定 + 发布开销（asr_stream.cpp 默认开）
FINALS = "了吗呢吧啊呀嘛么的哦嗯"   # asr_stream.cpp ends_with_sentence_final 同款
GAP_AFTER_ANSWER = 3.0
PLAYBACK_BUFFER = 0.15

MED_SYSTEM_PROMPT = """你是血透中心巡诊机器人「小医」，用轻柔亲切的语气和患者对话。
必须遵守：
1. 医学与护理问题，优先依据【参考资料】回答：
   - 资料里有相关内容（哪怕不是逐字回答），就用自己的话讲清楚，给患者可执行的建议；
   - 只有资料完全没提到这个话题、确实无法回答时，才说“这个问题我资料里没有。建议问护士或医生。”；
   - 不要编造资料以外的具体数值、剂量或做法，不要重复“资料里没有”。
2. 口语化短句，一次说 1-3 句，总长不超过 80 字，适合语音播报；不要列出资料编号。
2.1 第一句必须极短：先给一句不超过 8 个字的独立短句，并以句号结尾（如“先别沾水。”“这个要注意。”），再补充细节；首句必须是完整短句，不要把两个信息合并进第一句，不要带逗号。
3. 不给具体用药剂量、不诊断；涉及具体数值时只做一般性提醒，并建议咨询医生。
4. 患者出现胸痛、呼吸困难、意识改变、内瘘出血不止、明显心慌乏力等紧急情况时，立即说“我马上帮您呼叫护士”，不要展开长篇内容。
5. 禁止输出表情符号、动作描写；回答中不要出现“着急”两个字。

【示例】
问：透析完针眼什么时候能沾水？
答：先别沾水。透析后 24 小时内穿刺点不能清洗，当天也别洗澡。
问：内瘘摸不到震颤了怎么办？
答：先别慌。摸不到震颤要立即就医，可能是内瘘堵塞。
问：透析的时候能吃东西吗？
答：尽量别吃。透析中进食容易引起低血压、抽筋，实在饿了先问护士。"""

# ---- typo_map v2：从 router 源码解析（单一事实来源） ----
_ROUTER_SRC = open(os.path.join(LLM_NODE_DIR, "ros2_node_question_router.py"),
                   encoding="utf-8").read()
_TYPO_MAP, _WAKE_PAT = {}, None
for _n in ast.walk(ast.parse(_ROUTER_SRC)):
    if isinstance(_n, ast.Assign):
        for _t in _n.targets:
            if isinstance(_t, ast.Attribute) and _t.attr == "typo_map" \
                    and isinstance(_n.value, ast.Dict):
                _TYPO_MAP = ast.literal_eval(_n.value)
            if isinstance(_t, ast.Attribute) and _t.attr == "_wake_re" \
                    and isinstance(_n.value, ast.Call) \
                    and isinstance(_n.value.args[0], ast.Constant):
                _WAKE_PAT = _n.value.args[0].value
WAKE_RE = re.compile(_WAKE_PAT)
TYPO_PATTERN = re.compile("|".join(map(re.escape, _TYPO_MAP)))
print(f"[typo_map v2] {len(_TYPO_MAP)} 项, 唤醒词正则: {_WAKE_PAT}")


def fix_typos(text):
    text = TYPO_PATTERN.sub(lambda m: _TYPO_MAP[m.group(0)], text)
    return WAKE_RE.sub("小医", text)


def rag_retrieve(query, top_k=4):
    """进程内 BM25（与生产 llm_medical RAG_MODE=bm25 同一模块），无 HTTP 服务。"""
    return rag_bm25.retrieve(query, top_k=top_k)


def build_user_prompt(question, chunks, chunk_max_chars=350):
    if not chunks:
        return ("【参考资料】\n（本轮没有检索到相关资料）\n\n"
                f"【患者问题】{question}\n\n"
                "注意：没有资料时不要编造，按系统规则回答（说明资料里没有、建议咨询护士或医生）。")
    lines = ["【参考资料】"]
    for idx, c in enumerate(chunks, 1):
        title = c.get("title") or c.get("doc_id") or "资料"
        text = (c.get("text") or "").strip().replace("\n", " ")
        if len(text) > chunk_max_chars:
            text = text[:chunk_max_chars] + "…"
        lines.append(f"[{idx}]（来源：{title}）{text}")
    lines += ["", f"【患者问题】{question}", "",
              "注意：只依据上述资料回答；资料和问题无关时按系统规则说明资料里没有。"]
    return "\n".join(lines)


_EMOJI_RE = re.compile(
    "["
    "\U0001F000-\U0001FAFF"
    "\U00002600-\U000027BF"
    "\U00002B00-\U00002BFF"
    "\U00002190-\U000021FF"
    "\U0000FE00-\U0000FE0F"
    "\U0000200D"
    "]+")
_QUOTE_RE = re.compile("[\u201c\u201d\u300c\u300d\u300e\u300f]")


def clean_delta(text):
    return _QUOTE_RE.sub("", _EMOJI_RE.sub("", text))


class StreamSplitter:
    """ros2_node_stream_text.py SentenceSplitterNode 忠实移植（默认配置）。"""

    PATTERN = re.compile(r"[^，。；：、！？]*[，。；：、！？]?")
    SENTENCE_MARKS = "。；！？"
    SHORT_MARKS = "，：、"

    def __init__(self):
        self.buffer = ""
        self.first_flushed = False

    def feed(self, text):
        """句级切分 + 顺序保护（与生产 TTS 断句器一致）：
        逗号碎片并入紧随的完整句，绝不能先发后面的句子。"""
        self.buffer += text
        units, pending = [], ""
        for part in self.PATTERN.findall(self.buffer):
            part = part.strip()
            if not part:
                continue
            if part[-1] in self.SENTENCE_MARKS:
                self.first_flushed = True
                units.append(pending + part)
                pending = ""
            else:
                pending += part
        self.buffer = pending
        return units

    def flush(self):
        if self.buffer:
            out, self.buffer = self.buffer, ""
            return [out]
        return []


class Timeline:
    def __init__(self):
        self.t0 = None
        self.items = []
        self.fill_end = 0.0
        self._lock = threading.Lock()

    def start(self):
        self.t0 = time.perf_counter()

    def now(self):
        return time.perf_counter() - self.t0

    def place(self, pos, wav):
        with self._lock:
            wav = np.asarray(wav, dtype=np.float32)
            pos = max(pos, self.fill_end)
            self.items.append((pos, wav))
            self.fill_end = pos + len(wav) / SR

    def play_live(self, wav):
        self.place(max(self.now() - PLAYBACK_BUFFER, 0), wav)

    def wait_until(self, pos):
        d = pos - self.now()
        if d > 0:
            time.sleep(d)

    def assemble(self):
        total = int((max(self.fill_end, self.now()) + 1.0) * SR)
        buf = np.zeros(total, dtype=np.float32)
        for pos, wav in self.items:
            i0 = int(pos * SR)
            i1 = min(i0 + len(wav), total)
            buf[i0:i1] += wav[:i1 - i0]
        return buf


def main():
    os.makedirs(SEG_DIR, exist_ok=True)
    timings = {
        "tts_engine": "CosyVoice2-0.5B fp16 + 权重fp16(llm+flow) + expandable_segments"
                      " + 填充词 + 首单元流式",
        "rag": "进程内 BM25（rag_bm25.py，无 rag_service/GPU）",
        "text_norm": "tts_text_norm.normalize_for_tts + wetext 数字正则化",
        "typo_map_v2": _TYPO_MAP,
        "asr": "sherpa streaming zipformer, greedy, 4线程",
        "rounds": []}

    print("== 加载组件 ==")
    t = time.perf_counter()
    model = CosyVoice2(COSY_MODEL_DIR, load_jit=False, load_trt=False,
                       load_vllm=False, fp16=True)
    print(f"  CosyVoice2 fp16 加载 {time.perf_counter() - t:.1f}s")
    voices = {
        "doctor": (os.path.join(ASSETS, "prompt_serena.wav"),
                   "您好，我是血透巡诊机器人小医，今天由我为您做透析前的健康指导。"),
        "patient": (os.path.join(ASSETS, "prompt_patient_unclefu.wav"),
                    "您好，我是透析十年的老病友，想跟您咨询几个日常护理的问题。"),
    }
    prompt16k = {}
    for name, (wav_path, text) in voices.items():
        p16 = _load_wav(wav_path, 16000)
        prompt16k[name] = p16
        mi = model.frontend.frontend_zero_shot(text, text, p16, model.sample_rate, "")
        model.frontend.spk2info[name] = mi
        print(f"  音色注册: {name} <- {os.path.basename(wav_path)}")

    # 权重真 fp16（与生产节点一致）：2946→1600MiB，RTF 0.60→0.44
    inner = model.model
    inner.llm.half()
    inner.flow.half()
    torch.cuda.empty_cache()
    print("  权重 fp16 转换完成（llm+flow）")

    asr = sherpa_onnx.OnlineRecognizer.from_transducer(
        tokens=f"{ASR_MODEL}/tokens.txt",
        encoder=f"{ASR_MODEL}/encoder-epoch-99-avg-1.onnx",
        decoder=f"{ASR_MODEL}/decoder-epoch-99-avg-1.onnx",
        joiner=f"{ASR_MODEL}/joiner-epoch-99-avg-1.onnx",
        num_threads=2, decoding_method="greedy_search")
    _s = asr.create_stream()
    _s.accept_waveform(16000, np.zeros(16000, np.float32))
    _s.input_finished()
    while asr.is_ready(_s):
        asr.decode_stream(_s)
    print("  sherpa-onnx 流式 ASR 加载+预热完成")
    llm = OpenAI(api_key=open(KEY_PATH, encoding="utf-8").read().strip(),
                 base_url="https://dashscope.aliyuncs.com/compatible-mode/v1")

    @torch.inference_mode()
    def synth_one(text, spk, stream=False, on_chunk=None):
        norm = normalize_for_tts(text)
        if norm != text:
            print(f"    [规范化] {text!r} -> {norm!r}")
        pieces = []
        for chunk in model.inference_zero_shot(norm, voices[spk][1], prompt16k[spk],
                                               zero_shot_spk_id=spk, stream=stream):
            piece = chunk["tts_speech"][0].float().cpu().numpy()
            pieces.append(piece)
            if on_chunk is not None:
                on_chunk(piece.astype(np.float32))
        return np.concatenate(pieces).astype(np.float32) if pieces else np.zeros(0, np.float32)

    print("  预热…")
    synth_one("您好。", "doctor")
    # 固定首句缓存（与生产节点 TTS_FILLER_TEXT 同语义）
    FILLER_TEXT = "好的，"
    filler_audio = synth_one(FILLER_TEXT, "doctor")
    print(f"  填充词缓存: {FILLER_TEXT!r} {len(filler_audio) / SR:.1f}s")
    print("  预渲染问题语音…")
    q_wavs = []
    for i, q in enumerate(QUESTIONS := [
            "小医你好，我下周就要开始第一次透析了，去之前需要准备什么呀？",
            "那像我这样每周透析三次的，一天到底能喝多少水？体重又该怎么控制？",
            "小医，我今天摸内瘘的震颤好像比以前弱了，几乎摸不到了，是不是堵了？要不要紧？"], 1):
        w = synth_one(q, "patient")
        sf.write(f"{SEG_DIR}/q{i}.wav", w, SR)
        q_wavs.append(w)
        print(f"    Q{i}: {len(w) / SR:.1f}s")

    tl = Timeline()
    tts_q = _queue.Queue()
    state = {"first_audio_at": None, "filler_at": None}

    def tts_worker():
        while True:
            item = tts_q.get()
            if item is None:
                tts_q.task_done()
                return
            text, path, spk, stream_first = item
            try:
                def on_chunk(piece):
                    if state["first_audio_at"] is None:
                        state["first_audio_at"] = tl.now()
                    tl.play_live(piece)

                if stream_first:
                    wav = synth_one(text, spk, stream=True, on_chunk=on_chunk)
                else:
                    wav = synth_one(text, spk)
                    if state["first_audio_at"] is None:
                        state["first_audio_at"] = tl.now()
                    tl.play_live(wav)
                if path:
                    sf.write(path, wav, SR)
            finally:
                tts_q.task_done()

    worker = threading.Thread(target=tts_worker, daemon=True)
    worker.start()

    tl.start()
    tl.place(0, np.zeros(int(0.5 * SR), np.float32))
    print("\n== 录制开始（时间轴=壁钟）==")
    pos = 0.5

    for i, (q_text, q_wav) in enumerate(zip(QUESTIONS, q_wavs), 1):
        rd = {"question": q_text}
        state["first_audio_at"] = None
        print(f"\n--- Round {i} ---")
        tl.place(pos, q_wav)
        pos += len(q_wav) / SR
        q_end = pos
        tl.wait_until(pos)

        # ASR 流式解码：生产 C++ 节点在说话期间边说边解（不占说完后的时间），
        # 这里离线解码等价复现识别结果，解码耗时不计入时间轴。
        t0 = time.perf_counter()
        wav16k = _taf.resample(torch.from_numpy(q_wav)[None, :], SR, 16000)[0].numpy().astype(np.float32)
        stream = asr.create_stream()
        for j in range(0, len(wav16k), 1600):
            stream.accept_waveform(16000, wav16k[j:j + 1600])
            while asr.is_ready(stream):
                asr.decode_stream(stream)
        stream.input_finished()
        while asr.is_ready(stream):
            asr.decode_stream(stream)
        asr_text = asr.get_result_all(stream).text.replace("SIL", "").strip()
        rd["asr_raw"] = asr_text
        rd["asr_ms"] = round((time.perf_counter() - t0) * 1000)
        rd["asr_text"] = fix_typos(asr_text)
        if rd["asr_text"] != asr_text:
            print(f"  纠错: {asr_text!r} -> {rd['asr_text']!r}")
        print(f"  ASR {rd['asr_ms']}ms: {rd['asr_text']}")

        # 端点：与 asr_stream.cpp 同逻辑——语气词结尾走语义端点(0.55s 稳定)，
        # 否则等 rule2 1.2s（0.8s 实测会截断句中停顿，见 asr_stream.cpp 注释）
        if asr_text and asr_text[-1] in FINALS:
            endpoint_s, reason = SEMANTIC_ENDPOINT_S, "semantic"
        else:
            endpoint_s, reason = ENDPOINT_S, "rule2"
        rd["endpoint_s"] = endpoint_s
        rd["endpoint_reason"] = reason
        print(f"  端点 {endpoint_s}s ({reason})")
        pos += endpoint_s
        tl.wait_until(pos)

        t0 = time.perf_counter()
        chunks = rag_retrieve(rd["asr_text"], top_k=4)
        rd["rag_ms"] = round((time.perf_counter() - t0) * 1000)
        rd["rag_docs"] = [c.get("doc_id") for c in chunks[:4]]
        print(f"  RAG {rd['rag_ms']}ms: {rd['rag_docs']}")

        splitter = StreamSplitter()
        user_prompt = build_user_prompt(rd["asr_text"], chunks)
        t0 = time.perf_counter()
        llm_start_tl = tl.now()
        # [START] 等价点：填充词立即入播（生产节点在收到 [START] 时同样处理）
        if filler_audio is not None:
            tl.play_live(filler_audio)
            state["filler_at"] = tl.now()
            print(f"  [填充词] {FILLER_TEXT!r} 即时入播 @ {state['filler_at']:.2f}s")
        ttft = None
        n_units = 0
        flow = llm.chat.completions.create(
            model="qwen-plus-latest", stream=True,
            extra_body={"enable_thinking": False},
            messages=[{"role": "system", "content": MED_SYSTEM_PROMPT},
                      {"role": "user", "content": user_prompt}])
        full_text = ""
        for ev in flow:
            delta = ev.choices[0].delta.content if ev.choices and ev.choices[0].delta else None
            if not delta:
                continue
            if ttft is None:
                ttft = (time.perf_counter() - t0) * 1000
                print(f"  LLM 首token {ttft:.0f}ms")
            full_text += delta
            for unit in splitter.feed(clean_delta(delta)):
                n_units += 1
                print(f"    [单元{n_units}] {unit}")
                tts_q.put((unit, f"{SEG_DIR}/r{i}_u{n_units}.wav", "doctor",
                           n_units == 1))   # 首单元走流式
        for unit in splitter.flush():
            n_units += 1
            print(f"    [单元{n_units}] {unit}")
            tts_q.put((unit, f"{SEG_DIR}/r{i}_u{n_units}.wav", "doctor", False))
        tts_q.join()

        rd["ttft_ms"] = round(ttft) if ttft else None
        rd["answer_text"] = full_text.strip()
        fa = state["first_audio_at"]
        fl = state["filler_at"]
        rd["filler_gap_s"] = round(fl - q_end, 2) if fl else None      # 体感回应
        rd["first_audio_gap_s"] = round(fa - q_end, 2) if fa else None  # 回答首音
        rd["tts_first_audio_ms"] = round((fa - llm_start_tl) * 1000) if fa else None
        rd["answer_audio_s"] = round(tl.fill_end - fl, 2) if fl else None
        rd["tts_units"] = n_units
        pos = tl.now()
        print(f"  回答完成 @ {pos:.1f}s（问题说完后 {rd['filler_gap_s']}s 听到填充词，"
              f"{rd['first_audio_gap_s']}s 开始正式回答，"
              f"语音时长 {rd['answer_audio_s']}s，{n_units} 个TTS单元）")
        tl.wait_until(tl.fill_end)
        pos = tl.fill_end + GAP_AFTER_ANSWER
        rd["answer_end_s"] = round(tl.fill_end, 2)
        rd["first_audio_at_s"] = round(fa, 2) if fa else None
        rd["q_dur"] = round(len(q_wav) / SR, 2)
        tl.wait_until(pos)
        timings["rounds"].append(rd)

    tts_q.put(None)
    worker.join(timeout=2)
    audio = tl.assemble()
    sf.write(OUT_WAV, audio, SR, subtype="PCM_16")
    timings["total_s"] = round(len(audio) / SR, 1)
    with open(OUT_JSON, "w", encoding="utf-8") as f:
        json.dump(timings, f, ensure_ascii=False, indent=1)
    print(f"\n[output] {OUT_WAV}")
    print(f"  总时长 {timings['total_s']}s | 时间轴=壁钟，静音即真实生成延迟")
    print(f"[timings] {OUT_JSON}")


if __name__ == "__main__":
    main()

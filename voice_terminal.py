#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""语音链路前台状态终端（ASR→LLM→TTS 一体化运行台）。

- 所有节点作为本脚本的子进程启动；本终端退出/关闭（Ctrl+C / 关窗口 SIGHUP）时
  全链路自动终止。
- 画面实时原地刷新（不逐行滚动）：节点存活/CPU/内存 + 流水线各环节状态 +
  合并事件流。
- 启动前会清理残留的同名节点进程。

用法（在前台终端直接运行）：
  python3 voice_terminal.py
可选：--no-bt  不启动行为树两节点（dialog_manager/bt_hci）
"""
import ctypes
import os
import signal
import subprocess
import sys
import threading
import time
from collections import deque
from datetime import datetime

# ---------- 路径 ----------
BASE = os.path.dirname(os.path.abspath(__file__))
WS = os.path.dirname(BASE)                       # medical_ws/
PY = os.path.join(os.path.expanduser("~"),
                  "miniconda3/envs/qwen_tts_online/bin/python")
ASR_BIN = os.path.join(BASE, "install/llm_node_cpp/lib/llm_node_cpp/ros_node_keyword_asr")
SHERPA_ROOT = os.path.join(WS, "models/sherpa")
PUNCT = os.path.join(SHERPA_ROOT, "sherpa-onnx-punct-model/model.onnx")
PY_DIR = os.path.join(BASE, "src/llm_node_py/llm_node_py")
DIALOG = os.path.join(BASE, "src/dialog/dialog/bt_hci_interface.py")

os.environ.setdefault("PYTHONNOUSERSITE", "1")

# ---------- 节点定义 ----------
def ros_bash(inner):
    """包一层 bash：注入 ROS/工作空间环境后 exec（exec 保证 PID 即节点进程）。"""
    return ["bash", "-c",
            f"source /opt/ros/jazzy/setup.bash && source {BASE}/install/setup.bash && exec {inner}"]


NODES = [
    ("唤醒桥",   ros_bash(f"{PY} -u {PY_DIR}/ros2_node_wake_bridge.py"), {}),
    ("网关",     ros_bash(f"{PY} -u {PY_DIR}/ros2_node_llm_medical.py"), {}),
    ("TTS",     ros_bash(f"{PY} -u {PY_DIR}/ros2_node_tts_cosyvoice.py"), {}),
    ("护士告警", ros_bash(f"{PY} -u {PY_DIR}/ros2_node_nurse_alert.py"), {}),
    ("对话管理", ros_bash(f"{PY} -u {PY_DIR}/ros2_node_dialog_manager.py"), {}),
    ("BT接口",  ros_bash(f"{PY} -u {DIALOG}"), {}),
    ("ASR",     ros_bash(f"stdbuf -oL -eL {ASR_BIN}"),
     {"SHERPA_MODEL_ROOT": SHERPA_ROOT, "PUNCT_MODEL_PATH": PUNCT}),
]
if "--no-bt" in sys.argv:
    NODES = [n for n in NODES if n[0] not in ("对话管理", "BT接口")]

_clk = os.sysconf("SC_CLK_TCK")
_proc = {}          # name -> dict(proc, buf, last_cpu, born, restarts)


def prctl_pdeathsig():
    """父进程死亡时内核自动杀掉子进程（兜底，防止 SIGKILL 场景漏杀）。"""
    try:
        ctypes.CDLL("libc.so.6").prctl(1, signal.SIGTERM)  # PR_SET_PDEATHSIG
    except Exception:
        pass


def kill_stale():
    """清理残留的旧节点进程（模式加 [] 防误杀自身）。"""
    for pat in ("ros2_node[_]", "keyword_as[r]", "bt_hci_interfac[e]"):
        subprocess.run(["pkill", "-f", pat], capture_output=True)


def spawn(name, cmd, extra_env):
    env = dict(os.environ)
    env.update(extra_env)
    p = subprocess.Popen(
        cmd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        text=True, bufsize=1, preexec_fn=prctl_pdeathsig)
    st = {"proc": p, "buf": deque(maxlen=300), "last_cpu": _read_cpu(p.pid),
          "born": time.time(), "cpu": 0.0}
    _proc[name] = st
    threading.Thread(target=_reader, args=(name, p), daemon=True).start()
    return p


def _reader(name, p):
    try:
        for line in p.stdout:
            _proc[name]["buf"].append((datetime.now(), line.rstrip()))
    except Exception:
        pass


def _read_cpu(pid):
    try:
        with open(f"/proc/{pid}/stat") as f:
            parts = f.read().split()
        return (int(parts[13]) + int(parts[14])) / _clk
    except Exception:
        return None


def _mem_mb(pid):
    try:
        with open(f"/proc/{pid}/statm") as f:
            return int(f.read().split()[1]) * 4 / 1024
    except Exception:
        return 0.0


def sample_cpu():
    for st in _proc.values():
        p = st["proc"]
        cur = _read_cpu(p.pid)
        if cur is not None and st["last_cpu"] is not None:
            st["cpu"] = max(0.0, (cur - st["last_cpu"])) / _dt * 100
        st["last_cpu"] = cur


# ---------- 状态抽取（流水线面板） ----------
_state = {"wake": "待唤醒", "asr": "-", "question": "-", "intent": "-",
          "rag": "-", "tts": "-", "tts_n": 0, "answer": "-"}

import re  # noqa: E402
_RE_WAKE = re.compile(r"Detected keyword: .*?\"keyword\": \"([^\"]+)\"")
_RE_Q = re.compile(r"Received question: (.*)")
_RE_INTENT = re.compile(r"intent=(\w+)")
_RE_RAG = re.compile(r"RAG: (\d+) chunks, ([\d.]+)ms")
_RE_PLAY = re.compile(r"正在播放：(.*)")
_RE_NORM = re.compile(r"文本规范化: (.*) -> (.*)")


def update_state():
    def last_lines(name, n=60):
        st = _proc.get(name)
        return [t for _, t in list(st["buf"])[-n:]] if st else []

    for t in last_lines("ASR", 25):
        m = _RE_WAKE.search(t)
        if m:
            _state["wake"] = m.group(1)
        if "switch to asr" in t:
            _state["asr"] = "收音中(15s超时)"
        if "Asr Timer triggered" in t:
            _state["asr"] = "超时→回到唤醒监听"
    for t in last_lines("网关", 40):
        m = _RE_Q.search(t)
        if m:
            _state["question"] = m.group(1)[:30]
        m = _RE_INTENT.search(t)
        if m:
            _state["intent"] = m.group(1)
        m = _RE_RAG.search(t)
        if m:
            _state["rag"] = f"{m.group(1)}块/{m.group(2)}ms"
        if "Dialog pair appended" in t:
            _state["answer"] = "已生成"
    for t in last_lines("TTS", 40):
        m = _RE_PLAY.search(t)
        if m:
            _state["tts"] = m.group(1)[:26]
            _state["tts_n"] += 1
        m = _RE_NORM.search(t)
        if m:
            _state["tts"] = f"{m.group(2)[:20]}(规范化)"


# ---------- 渲染 ----------
_dt = 0.6
_running = True
_t0 = time.time()


def render():
    import shutil
    term_w = shutil.get_terminal_size((100, 30)).columns
    out = ["\033[H\033[2J"]  # 清屏定位
    up = time.time() - _t0
    out.append(f"═" * 30 + f" 语音链路状态台  {datetime.now():%H:%M:%S}  运行 {int(up)//60:02d}:{int(up)%60:02d} " + "═" * 20)
    out.append(f"{'节点':<10}{'PID':>8}{'状态':>6}{'CPU%':>8}{'内存':>10}")
    alive = 0
    for name, _, _ in NODES:
        st = _proc.get(name)
        if st and st["proc"].poll() is None:
            ok, alive = "●运行", alive + 1
        else:
            ok = "✗退出"
        pid = st["proc"].pid if st else 0
        cpu = f"{st['cpu']:.1f}" if st else "-"
        mem = f"{_mem_mb(pid):.0f}MB" if st and st["proc"].poll() is None else "-"
        out.append(f"{name:<10}{pid:>8}{ok:>6}{cpu:>8}{mem:>10}")
    out.append("─" * term_w)
    out.append("【流水线】")
    out.append(f" 唤醒(KWS) : {_state['wake']}")
    out.append(f" ASR      : {_state['asr']}")
    out.append(f" 网关     : 意图={_state['intent']}  RAG={_state['rag']}  问题={_state['question']}")
    out.append(f" TTS      : 已播{_state['tts_n']}单元  最近={_state['tts']}")
    out.append("─" * term_w)
    out.append("【最新事件】")
    merged = []
    for name in _proc:
        for ts, t in list(_proc[name]["buf"])[-4:]:
            merged.append((ts, name, t))
    merged.sort(key=lambda x: x[0])
    for ts, name, t in merged[-12:]:
        t = (t[:term_w - 22] + "…") if len(t) > term_w - 22 else t
        out.append(f" {ts:%H:%M:%S} [{name}] {t}")
    out.append("─" * term_w)
    out.append(" Ctrl+C 或关闭本终端 = 退出并关闭全部节点 | 唤醒词:小医小医 | BT流程: tools/voice_regression/bt_flow_sim_client.py")
    sys.stdout.write("\n".join(l[:term_w] for l in out) + "\n")
    sys.stdout.flush()


def _on_signal(signum, frame):
    global _running
    _running = False


def cleanup():
    print("\n[terminal] 正在关闭全部节点...", flush=True)
    for st in _proc.values():
        try:
            st["proc"].terminate()
        except Exception:
            pass
    deadline = time.time() + 3
    for st in _proc.values():
        try:
            st["proc"].wait(timeout=max(0.1, deadline - time.time()))
        except Exception:
            try:
                st["proc"].kill()
            except Exception:
                pass
    print("[terminal] 全部节点已关闭。", flush=True)


def main():
    global _dt
    # 单实例锁：防止两套栈同时运行（双栈 = 每个问题被回答两次，实测事故）
    import fcntl
    _lock_fp = open("/tmp/voice_stack.lock", "w")
    try:
        fcntl.flock(_lock_fp, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        print("[terminal][错误] 已有一套语音链路在运行（/tmp/voice_stack.lock 被占用）。\n"
              "  先关闭已有实例（关闭其终端或 pkill -f voice_terminal），再启动。")
        sys.exit(1)
    sys._lock_fp = _lock_fp  # 生命周期内保持
    signal.signal(signal.SIGINT, _on_signal)
    signal.signal(signal.SIGTERM, _on_signal)
    signal.signal(signal.SIGHUP, _on_signal)   # 关闭终端窗口
    kill_stale()
    time.sleep(0.5)
    print("[terminal] 启动节点...", flush=True)
    for name, cmd, extra in NODES:
        spawn(name, cmd, extra)
        time.sleep(1.2)                        # 依序拉起，TTS/网关先于 ASR 就绪
    try:
        last = 0.0
        while _running:
            now = time.time()
            if now - last >= _dt:
                sample_cpu()
                update_state()
                render()
                last = now
            time.sleep(0.1)
            for st in _proc.values():          # 任意节点退出即播报，不自动重启
                if st["proc"].poll() is not None and st.get("noted") is None:
                    st["noted"] = True
    except KeyboardInterrupt:
        pass
    finally:
        cleanup()


if __name__ == "__main__":
    main()

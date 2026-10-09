#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""KP 分段分析：按 SET:Velocity:KP 命令把采集 CSV 切段，段内重排时间轴后测振荡带幅。

用法:
  python tools/analyze_seg.py tools/logs/capture_exp2.csv
  python tools/analyze_seg.py tools/logs/capture_exp2.csv --band 3.4 4.6 --tail 25

原理/已知坑:
- 固件按 50ms 节拍(TIM3 分频)生成遥测帧，但 PC 侧到达时间戳是爆发式的（帧会迟到，
  中位 dt≈0ms）→ 段内一律按帧序 ×50ms 重排时间轴，不信任 t_mono/t_recv 做频谱。
- 到达间隔 >2s 视为生产停止（掉落重进/退出模式/暂停），硬切段。
- SET 命令（同名 .log，本地时间戳）定位到行：不用到达时间匹配（爆发延迟 ±1.1s、
  乱码期行稀疏会错位），而是按"生产时间"插值——段内第 k 帧的生产时刻 =
  段末帧到达时刻 - (段内剩余帧数)×50ms（生产恒速，到达不恒速）。
- KP 在板子上跨掉落/重进保持，current_kp 全局顺延。
- 每段报告：帧数/时长、带内峰值幅度（整段+稳态尾窗）、主峰频率、误差 std、
  输出饱和率、ang_sp 顶限率、近掉落/疑似模式2/段内断流标记。
"""
import argparse
import csv
import os
import re
import time

import numpy as np

DEFAULT_KP = 12.0  # mode.c 里 pid_velocity 的静态默认值


def load_csv(path):
    t, rows = [], []
    with open(path, newline="", encoding="utf-8") as f:
        r = csv.reader(f)
        next(r)
        for row in r:
            if len(row) < 9:
                continue
            t.append(float(row[0]))
            rows.append([float(x) for x in row[3:9]])
    return np.array(t), np.array(rows)


def parse_set_log(path):
    """同名 .log 里找 SET:Velocity:KP:<v> 事件 -> [(当日秒, 值)]。发送/应答行去重。"""
    if not os.path.exists(path):
        return []
    ev = []
    pat = re.compile(r"^(\d{2}):(\d{2}):(\d{2})\s+(\S+)\s+\|\s+SET:Velocity:KP:([-\d.]+)")
    for line in open(path, encoding="utf-8", errors="replace"):
        m = pat.match(line.strip())
        if not m:
            continue
        h, mi, s, tag, val = m.groups()
        ev.append((int(h) * 3600 + int(mi) * 60 + int(s), float(val)))
    out = []
    for sec, val in sorted(ev):
        if out and abs(sec - out[-1][0]) <= 1 and abs(val - out[-1][1]) < 1e-9:
            continue
        out.append((sec, val))
    return out


def band_amp(x, f1, f2, fs=20.0):
    x = np.asarray(x, float) - np.mean(x)
    if len(x) < 40:
        return 0.0
    w = np.hanning(len(x))
    X = np.abs(np.fft.rfft(x * w))
    f = np.fft.rfftfreq(len(x), 1 / fs)
    m = (f >= f1) & (f <= f2)
    if not m.any():
        return 0.0
    return 2 * X[m].max() / w.sum()


def peak_freq(x, fs=20.0, fmin=0.5, fmax=10.0):
    x = np.asarray(x, float) - np.mean(x)
    if len(x) < 40:
        return 0.0, 0.0
    w = np.hanning(len(x))
    X = np.abs(np.fft.rfft(x * w))
    f = np.fft.rfftfreq(len(x), 1 / fs)
    m = (f >= fmin) & (f <= fmax)
    if not m.any():
        return 0.0, 0.0
    i = np.where(m)[0][np.argmax(X[m])]
    return f[i], 2 * X[i] / w.sum()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("csv")
    ap.add_argument("--band", nargs=2, type=float, default=[3.4, 4.6])
    ap.add_argument("--tail", type=float, default=25.0)
    args = ap.parse_args()

    t, d = load_csv(args.csv)
    if len(t) < 60:
        print("!! 数据太少（%d 帧）" % len(t))
        return
    base = os.path.splitext(args.csv)[0]
    events = parse_set_log(base + ".log")

    # 各行的本地时刻（秒，带小数）与"生产时刻"估计
    lsec = np.array([time.localtime(x).tm_hour * 3600 + time.localtime(x).tm_min * 60 +
                     time.localtime(x).tm_sec + (x - np.floor(x)) for x in t])
    dt = np.diff(t)
    hard = [0] + list(np.where(dt > 2.0)[0] + 1) + [len(t)]
    prod = np.empty(len(t))
    for a, b in zip(hard[:-1], hard[1:]):
        prod[a:b] = lsec[b - 1] - (b - 1 - np.arange(a, b)) * 0.05

    # SET 事件 -> 行号（生产时间轴最近匹配）。
    # 早于数据起点的 SET（桥重启后日志仍在追加）：不定位，取最后一个作为采集开始时的板上 KP
    cuts = []  # (row, val)
    init_kp = None
    for sec, val in events:
        i = int(np.argmin(np.abs(prod - sec)))
        if abs(prod[i] - sec) > 5.0:
            if sec < prod[0]:
                init_kp = val
            else:
                print("!! SET KP:%g 匹配不上数据时段（差 %.1fs），跳过" % (val, abs(prod[i] - sec)))
            continue
        cuts.append((i, val))
    cuts.sort()

    # 切段：硬断点 + SET 边界；KP 跨段顺延
    segs = []  # (a, b, kp)
    current_kp = init_kp if init_kp is not None else DEFAULT_KP
    if init_kp is not None:
        print("（采集开始时板上 KP=%g，来自更早的 SET）" % init_kp)
    ci = 0
    for a, b in zip(hard[:-1], hard[1:]):
        while ci < len(cuts) and cuts[ci][0] < a:      # 边界上的 SET 归后段：先更新
            current_kp = cuts[ci][1]
            ci += 1
        kp = current_kp
        inside = []
        while ci < len(cuts) and cuts[ci][0] < b:
            inside.append(cuts[ci])
            ci += 1
        bounds = [a] + [i for i, _ in inside] + [b]
        for j, (aa, bb) in enumerate(zip(bounds[:-1], bounds[1:])):
            if j > 0:
                kp = inside[j - 1][1]
            if bb - aa >= 40:  # ≥2s 才算段
                segs.append((aa, bb, kp))
        if inside:
            current_kp = inside[-1][1]

    print("== %s ==" % os.path.basename(args.csv))
    print("SET 事件: %s" % ("; ".join("KP:%g@%d行" % (v, i) for i, v in cuts) or "无"))
    print("带窗 %.1f~%.1f Hz, 稳态尾窗 %.0fs, 帧距重排 50ms" % (args.band[0], args.band[1], args.tail))
    print()
    print("段 KP    行范围         帧   时长   带幅(整段) 带幅(尾窗) 主峰   errStd  饱和  顶限  备注")
    for k, (a, b, kp) in enumerate(segs):
        s = d[a:b]
        n = b - a
        tail_n = min(n, max(int(args.tail * 20), 100))
        tw = s[-tail_n:, 3]
        amp_all = band_amp(s[:, 3], *args.band)
        amp_tail = band_amp(tw, *args.band)
        pf, _ = peak_freq(tw)
        out_sat = 100.0 * np.mean(np.abs(s[:, 1]) > 9800)
        sp_clip = 100.0 * np.mean(np.abs(s[:, 4]) > 7.9)
        notes = []
        if np.abs(s[:, 3]).max() > 40:
            notes.append("近掉落!")
        if np.median(np.abs(s[:, 0])) > 10:
            notes.append("疑似模式2数据")
        gap_in = np.diff(t[a:b]).max() if n > 1 else 0
        if gap_in > 1.5:
            notes.append("段内断流%.1fs" % gap_in)
        kp_lab = ("%.4g" % kp) if kp != DEFAULT_KP else "12默认"
        print("段%d %-7s %4d..%-4d %5d %5.1fs  %7.3f°  %7.3f°  %5.2fHz %5.2f° %4.1f%% %4.1f%%  %s" %
              (k, kp_lab, a, b - 1, n, n * 0.05, amp_all, amp_tail, pf, s[:, 3].std(),
               out_sat, sp_clip, ";".join(notes) or "-"))
    print()
    print("(带幅=振荡带内峰值幅度；尾窗取每段最后 %.0fs；重排后帧距按 50ms)" % args.tail)


if __name__ == "__main__":
    main()

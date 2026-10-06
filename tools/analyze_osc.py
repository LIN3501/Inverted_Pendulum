#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""分析 FireWater 采集 CSV，定位高频震荡来源。

用法: python tools/analyze_osc.py tools/logs/capture_com8.csv [--seg start:end]

通道: ch0=参考(0) ch1=PID输出 ch2=直立环积分 ch3=误差(0-angle,°)
      ch4=速度环角度设定(°) ch5=速度环积分
"""
import argparse
import csv
import sys

import numpy as np

CH_NAME = ["ch0 ref", "ch1 out(PWM)", "ch2 int_bal", "ch3 err(deg)",
           "ch4 ang_sp(deg)", "ch5 int_vel"]


def load(path):
    t, rows = [], []
    with open(path, newline="", encoding="utf-8") as f:
        r = csv.reader(f)
        head = next(r)
        ncol = 3 + 6  # t_recv,t_mono,client + ch0..5
        for row in r:
            if len(row) < ncol:
                continue
            t.append(float(row[1]))          # t_mono
            rows.append([float(x) for x in row[3:ncol]])
    return np.array(t), np.array(rows)


def spectrum(x, fs):
    """去均值 + Hann 窗复频谱；幅值已换算回物理峰值幅度 A=2|X|/W"""
    x = np.asarray(x, float)
    w = np.hanning(len(x))
    W = w.sum()
    X = np.fft.rfft((x - x.mean()) * w)
    freq = np.fft.rfftfreq(len(x), 1 / fs)
    return freq, X, 2 * np.abs(X) / W


def top_peaks(freq, amp, n=5, fmin=0.3, fmax=None):
    if fmax is None:
        fmax = freq[-1]
    m = (freq >= fmin) & (freq <= fmax)
    f, a = freq[m], amp[m]
    idx = [i for i in range(1, len(f) - 1) if a[i] > a[i - 1] and a[i] > a[i + 1]]
    idx.sort(key=lambda i: -a[i])
    return [(f[i], a[i]) for i in idx[:n]]


def xcorr_phase(a, b, fs):
    """互相关峰值滞后(采样点) -> 相位差(deg, 相对主导周期)"""
    a = a - a.mean()
    b = b - b.mean()
    c = np.correlate(b, a, "full")
    lag = np.argmax(c) - (len(a) - 1)
    return lag, lag / fs * 360.0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("csv")
    ap.add_argument("--seg", default=None, help="如 200:900 按帧号取段")
    args = ap.parse_args()

    t, d = load(args.csv)
    if args.seg:
        s, e = (int(x) for x in args.seg.split(":"))
        t, d = t[s:e], d[s:e]

    dt = np.diff(t)
    fs = 1.0 / dt.mean()
    print("== 采样 ==")
    print("帧数 %d  时长 %.1fs  平均fs %.2fHz  dt p5/p50/p95 = %.1f/%.1f/%.1f ms"
          % (len(t), t[-1] - t[0], fs,
             np.percentile(dt, 5) * 1e3, np.percentile(dt, 50) * 1e3,
             np.percentile(dt, 95) * 1e3))
    big = dt[dt > np.percentile(dt, 95) * 3]
    if len(big):
        print("!! %d 个 >3x 的采样间隔（数据断流/阻塞）, 最大 %.0f ms"
              % (len(big), big.max() * 1e3))

    print("\n== 时域统计 ==")
    print("%-16s %10s %10s %10s" % ("channel", "min", "max", "std"))
    for i in range(d.shape[1]):
        print("%-16s %10.3f %10.3f %10.3f"
              % (CH_NAME[i], d[:, i].min(), d[:, i].max(), d[:, i].std()))

    # 去掉恒定均值后的波动幅度（波动才和震荡有关）
    print("\n== 去均值后峰峰波动 ==")
    for i in range(1, d.shape[1]):
        x = d[:, i]
        print("%-16s  p2p=%9.3f  (mean=%9.3f)"
              % (CH_NAME[i], x.max() - x.min(), x.mean()))

    print("\n== 频谱主峰（0.5~10Hz，去均值+Hann窗，幅值=物理峰值幅度） ==")
    freq, X, amp = spectrum(d[:, 1], fs)
    peaks_all = {}
    for i in range(1, d.shape[1]):
        fr, Xc, am = spectrum(d[:, i], fs)
        pk = top_peaks(fr, am, n=4)
        peaks_all[i] = (fr, Xc, am, pk)
        print("%-16s %s" % (CH_NAME[i],
              ",  ".join("%.2fHz(%.4g)" % p for p in pk)))

    # 找主导频率（以 ch1 输出为准，输出是所有环的合力）
    fr1, X1, am1, pk1 = peaks_all[1]
    f0 = pk1[0][0] if pk1 else 0
    if f0 > 0:
        print("\n== 主导频率 f0 = %.2f Hz：各通道幅度与相对 ch1 的相位 ==" % f0)
        print("（相位>0 = 该通道超前 ch1；out=Kp*(ang_sp+err)-D项，err 与 out 反相才对）")
        print("%-16s %12s %12s" % ("channel", "amp@f0", "phase(deg)"))
        k1 = np.argmin(np.abs(fr1 - f0))
        ref = X1[k1]
        for i in range(1, d.shape[1]):
            fr, Xc, am, _ = peaks_all[i]
            k = np.argmin(np.abs(fr - f0))
            ph = np.angle(Xc[k] * np.conj(ref), deg=True)
            print("%-16s %12.4g %12.1f" % (CH_NAME[i], am[k], ph))

    # ch4 相对 ch3 的相位：正 = 速度环设定超前摆杆误差（外环在推）
    fr3, X3, am3, _ = peaks_all[3]
    fr4, X4, am4, _ = peaks_all[4]
    k3 = np.argmin(np.abs(fr3 - f0))
    k4 = np.argmin(np.abs(fr4 - f0))
    print("\nch4(ang_sp) 相对 ch3(err) 相位: %.1f°  (幅度 ch3=%.3f°, ch4=%.3f°)"
          % (np.angle(X4[k4] * np.conj(X3[k3]), deg=True), am3[k3], am4[k4]))

    # ch4 的台阶检查：速度环量化会留下 Kp_v x 速度格点 的整数倍台阶
    print("\n== ch4(速度环设定) 变化量直方图（±2° 内 0.1° 分辨率） ==")
    d4 = np.diff(d[:, 4])
    nz = d4[np.abs(d4) > 1e-6]
    inner = nz[np.abs(nz) <= 2.0]
    outer = len(nz) - len(inner)
    if len(inner):
        hist, edges = np.histogram(inner, bins=np.arange(-2.0, 2.01, 0.1))
        for h, e in zip(hist, edges):
            if h:
                print("  d=%+5.1f : %s (%d)" % (e, "#" * min(h, 60), h))
        print("  |dch4| 中位数 %.3f；±2°外另有 %d 次" %
              (np.median(np.abs(nz)), outer))

    # ch1 饱和率
    lim = np.percentile(np.abs(d[:, 1]), 99)
    sat = np.mean(np.abs(d[:, 1]) > 0.98 * lim) if lim > 0 else 0
    print("\nch1 |out| 99分位 %.0f，接近该限的比例 %.1f%%" % (lim, sat * 100))


if __name__ == "__main__":
    main()

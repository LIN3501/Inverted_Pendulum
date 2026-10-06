#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""FireWater 采集桥：三种模式（可同时落盘 CSV 供 tools/analyze_osc.py 分析）。

1. TCP 服务端:  python tools/firewater_listen.py --port 9000
2. 串口只读:    python tools/firewater_listen.py --com COM8
3. 串口<->TCP 双向桥（VOFA+ 用 TCP客户端 连 127.0.0.1:9000）:
                python tools/firewater_listen.py --com COM8 --port 9000
                - COM8 收到的每行 -> 解析落盘 CSV + 原样转发给所有 TCP 客户端
                - TCP 客户端(VOFA+)发来的数据 -> 原样写入 COM8（下发 SET 调参命令）

- 帧格式（mode.c 的 Serial_SendFireWater）: "ch0,ch1,ch2,ch3,ch4,ch5\n"
  ch0=水平参考(恒0) ch1=PID输出 ch2=直立环积分 ch3=误差(0-angle) ch4=速度环角度设定 ch5=速度环积分
- CSV 列: t_recv(epoch秒), t_mono(单调钟秒), client, ch0..ch5
- 解析不了的行（SET:/GET:/ERR: 应答等）进同名 .log
"""
import argparse
import csv
import os
import socket
import threading
import time
from datetime import datetime


def parse_firewater(line):
    parts = [p.strip() for p in line.split(",")]
    if not parts:
        return None
    vals = []
    for p in parts:
        try:
            vals.append(float(p))
        except ValueError:
            return None
    return vals


def local_ips():
    ips = set()
    try:
        for info in socket.getaddrinfo(socket.gethostname(), None):
            ip = info[4][0]
            if "." in ip and not ip.startswith("127."):
                ips.add(ip)
    except OSError:
        pass
    return sorted(ips)


class Capture:
    """CSV 落盘 + 非 FireWater 行的原始日志"""

    def __init__(self, out_path):
        os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
        self.csv_path = out_path
        self.log_path = os.path.splitext(out_path)[0] + ".log"
        self.lock = threading.Lock()
        self.total = 0
        self.window = 0
        self.last_vals = None
        self._fh = open(self.csv_path, "w", newline="", encoding="utf-8")
        self._wr = csv.writer(self._fh)
        self._wr.writerow(["t_recv", "t_mono", "client"] +
                          ["ch%d" % i for i in range(6)])
        self._log = open(self.log_path, "a", encoding="utf-8")

    def write_row(self, tag, vals):
        with self.lock:
            self._wr.writerow(["%.4f" % time.time(), "%.4f" % time.monotonic(),
                               tag] + ["%.6g" % v for v in vals])
            self._fh.flush()
            self.total += 1
            self.window += 1
            self.last_vals = vals

    def write_raw(self, tag, line):
        with self.lock:
            self._log.write("%s %s | %s\n" % (
                datetime.now().strftime("%H:%M:%S"), tag, line))
            self._log.flush()

    def stats_line(self):
        with self.lock:
            v = self.last_vals
            vs = ("[" + ", ".join("%.2f" % x for x in v[:6]) + "]") if v else "-"
            return "frames=%d (%.1f Hz) last=%s" % (
                self.total, self.window / 5.0, vs)

    def reset_window(self):
        with self.lock:
            self.window = 0

    def close(self):
        with self.lock:
            self._fh.close()
            self._log.close()


def feed_line(line, tag, cap):
    line = line.strip()
    if not line:
        return
    vals = parse_firewater(line)
    if vals is None:
        cap.write_raw(tag, line)
        print("[raw] %s" % line, flush=True)
    else:
        cap.write_row(tag, vals)


class Hub:
    """TCP 客户端登记表：上行广播 + 下行汇聚"""

    def __init__(self):
        self.lock = threading.Lock()
        self.clients = {}          # addr -> conn
        self.downlink = None       # 回调: bytes -> 写串口

    def add(self, addr, conn):
        with self.lock:
            self.clients[addr] = conn

    def remove(self, addr):
        with self.lock:
            self.clients.pop(addr, None)

    def broadcast(self, data):
        with self.lock:
            dead = []
            for addr, conn in self.clients.items():
                try:
                    conn.sendall(data)
                except OSError:
                    dead.append(addr)
            for addr in dead:
                self.clients.pop(addr, None)

    def to_serial(self, data):
        if self.downlink:
            self.downlink(data)


def serial_loop(args, cap, hub, stop):
    import serial
    ser = serial.Serial(args.com, args.baud, timeout=0.2)
    ser_lock = threading.Lock()

    def downlink(data):
        with ser_lock:
            try:
                ser.write(data)
                cap.write_raw("vofa>", data.decode("ascii", "replace").strip())
                print("[cmd] %s" % data.decode("ascii", "replace").strip(),
                      flush=True)
            except OSError as e:
                print("[!] serial write failed: %s" % e, flush=True)

    hub.downlink = downlink
    print("serial open %s @%d" % (args.com, args.baud), flush=True)
    buf = b""
    while not stop.is_set():
        buf += ser.read(1024)
        while b"\n" in buf:
            raw, buf = buf.split(b"\n", 1)
            line = raw.decode("ascii", "replace")
            feed_line(line, args.com, cap)
            hub.broadcast(raw + b"\n")
    ser.close()


def tcp_server_loop(args, cap, hub, stop):
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind((args.bind, args.port))
    srv.listen(4)
    print("listening on %s:%d" % (args.bind, args.port), flush=True)
    print("VOFA+/客户端 connect to: " + (", ".join(
        "%s:%d" % (ip, args.port) for ip in local_ips())
        or "-") + "  以及 127.0.0.1:%d" % args.port, flush=True)
    srv.settimeout(0.5)
    while not stop.is_set():
        try:
            conn, addr = srv.accept()
        except socket.timeout:
            continue
        t = threading.Thread(target=tcp_client_loop,
                             args=(conn, addr, hub, cap), daemon=True)
        t.start()
    srv.close()


def tcp_client_loop(conn, addr, hub, cap):
    hub.add(addr, conn)
    print("[+] client %s:%d" % addr[:2], flush=True)
    try:
        while True:
            data = conn.recv(4096)
            if not data:
                break
            hub.to_serial(data)
    except OSError:
        pass
    finally:
        hub.remove(addr)
        print("[-] client %s:%d" % addr[:2], flush=True)
        try:
            conn.close()
        except OSError:
            pass


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=9000)
    ap.add_argument("--bind", default="0.0.0.0")
    ap.add_argument("--com", default=None, help="串口，如 COM8")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--out", default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "logs", "capture.csv"))
    args = ap.parse_args()

    cap = Capture(args.out)
    hub = Hub()
    stop = threading.Event()

    def stats():
        while not stop.wait(5.0):
            print("[stat] " + cap.stats_line(), flush=True)
            cap.reset_window()

    threading.Thread(target=stats, daemon=True).start()
    try:
        if args.com and args.port:
            ts = threading.Thread(target=tcp_server_loop,
                                  args=(args, cap, hub, stop), daemon=True)
            ts.start()
            serial_loop(args, cap, hub, stop)
        elif args.com:
            serial_loop(args, cap, hub, stop)
        else:
            tcp_server_loop(args, cap, hub, stop)
    except KeyboardInterrupt:
        pass
    finally:
        stop.set()
        cap.close()
        print("done. csv=%s" % args.out, flush=True)


if __name__ == "__main__":
    main()

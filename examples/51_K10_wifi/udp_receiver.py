#!/usr/bin/env python3
"""
CodeBuddy P1 验证脚本 — UDP 音频接收器

监听 UDP 端口, 把 ESP32 发来的 16kHz/16bit/mono PCM 累积写入 WAV 文件。
Ctrl+C 停止并保存, 之后用任意播放器打开 capture.wav 验证录音。

用法:
    python udp_receiver.py                 # 默认监听 0.0.0.0:3333, 存 capture.wav
    python udp_receiver.py --port 3333 --out capture.wav

依赖: 仅标准库 (socket, wave, struct)。无需 pip 安装。

验证判断:
    - 清晰可懂       -> P1 成功
    - 变调(太快/太慢) -> 采样率不符, 核对两端都是 16000
    - 静音/纯噪声     -> ESP32 端 MIC_CHANNEL_OFFSET 选错, 翻转 0<->1 重测
    - 断续/爆音       -> 丢包或 DMA 溢出
"""
import argparse
import socket
import wave
import sys
import time

SAMPLE_RATE = 16000
CHANNELS = 1
SAMPWIDTH = 2  # 16-bit


def main():
    ap = argparse.ArgumentParser(description="CodeBuddy P1 UDP 音频接收器")
    ap.add_argument("--host", default="0.0.0.0", help="监听地址 (默认 0.0.0.0)")
    ap.add_argument("--port", type=int, default=3333, help="监听端口 (默认 3333)")
    ap.add_argument("--out", default="capture.wav", help="输出 WAV 文件名")
    args = ap.parse_args()

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    # 加大接收缓冲, 减少突发丢包
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1 << 20)
    sock.bind((args.host, args.port))
    sock.settimeout(1.0)

    print(f"监听 UDP {args.host}:{args.port} ...")
    print(f"输出文件: {args.out}  ({SAMPLE_RATE}Hz/{SAMPWIDTH*8}bit/mono)")
    print("等待 ESP32 发流, 按 Ctrl+C 停止并保存。\n")

    wf = wave.open(args.out, "wb")
    wf.setnchannels(CHANNELS)
    wf.setsampwidth(SAMPWIDTH)
    wf.setframerate(SAMPLE_RATE)

    total_bytes = 0
    packets = 0
    first_time = None
    last_report = time.time()
    first_peer = None

    try:
        while True:
            try:
                data, addr = sock.recvfrom(65535)
            except socket.timeout:
                continue

            if first_time is None:
                first_time = time.time()
                first_peer = addr
                print(f"收到首包, 来自 {addr[0]}:{addr[1]}")

            wf.writeframes(data)
            total_bytes += len(data)
            packets += 1

            # 每秒报告一次吞吐
            now = time.time()
            if now - last_report >= 1.0:
                secs = now - first_time if first_time else 0
                kbps = (total_bytes / 1024.0) / secs if secs > 0 else 0
                print(f"  包数={packets}  累计={total_bytes/1024:.1f}KB  "
                      f"时长={total_bytes/(SAMPLE_RATE*SAMPWIDTH):.1f}s  "
                      f"速率={kbps:.1f}KB/s")
                last_report = now

    except KeyboardInterrupt:
        print("\n停止接收。")
    finally:
        wf.close()
        sock.close()
        dur = total_bytes / (SAMPLE_RATE * SAMPWIDTH)
        print(f"\n已保存 {args.out}")
        print(f"总包数={packets}  总字节={total_bytes}  音频时长={dur:.2f}s")
        if packets == 0:
            print("!! 没收到任何数据。排查: ESP32 是否连上 WiFi、目标 IP/端口是否正确、"
                  "防火墙是否放行 UDP、是否按了按键 A 开始发流。")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""
持续监听 K10 串口输出
按 Ctrl+C 停止
"""
import serial
import time

PORT = 'COM4'
BAUDRATE = 115200

print(f"连接到 {PORT} @ {BAUDRATE} baud...")
print("等待数据... (请按 K10 的复位按钮)")
print("按 Ctrl+C 停止监听\n")
print("="*60)

try:
    ser = serial.Serial(PORT, BAUDRATE, timeout=1)

    while True:
        if ser.in_waiting > 0:
            line = ser.readline().decode('utf-8', errors='ignore').rstrip()
            if line:
                print(line)
        time.sleep(0.01)

except KeyboardInterrupt:
    print("\n停止监听")
except Exception as e:
    print(f"错误: {e}")
finally:
    if 'ser' in locals():
        ser.close()

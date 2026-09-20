#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
K10 按键测试脚本
监听 K10 串口，捕获按键相关日志
"""

import serial
import time
import sys

def monitor_k10(port='COM4', baudrate=115200, timeout=60):
    """监控 K10 串口输出"""
    print(f"=== Connecting to K10 ({port}) ===")
    try:
        ser = serial.Serial(port, baudrate, timeout=1)
        print(f"[OK] Serial port opened")
        print(f"\nPlease press Key A on K10 now...")
        print(f"Monitoring for {timeout} seconds\n")

        start_time = time.time()
        key_pressed = False
        key_released = False

        while (time.time() - start_time) < timeout:
            if ser.in_waiting > 0:
                try:
                    line = ser.readline().decode('utf-8', errors='ignore').strip()
                    if line:
                        # 检查是否包含按键相关信息
                        if 'Key A' in line or 'F2' in line or 'sent to Dongle' in line:
                            print(f"[KEY] {line}")
                            if 'PRESSED' in line:
                                key_pressed = True
                            if 'RELEASED' in line:
                                key_released = True
                        elif 'Streaming' in line or 'STARTED' in line or 'STOPPED' in line:
                            print(f"[STREAM] {line}")
                        elif 'send FAILED' in line or 'FAIL' in line:
                            print(f"[ERROR] {line}")
                        elif 'ESP-NOW' in line or 'init' in line:
                            print(f"[INFO] {line}")
                except UnicodeDecodeError:
                    pass

        ser.close()

        print(f"\n=== Test Results ===")
        if key_pressed:
            print("[OK] Key A PRESSED detected")
        else:
            print("[FAIL] Key A PRESSED not detected")

        if key_released:
            print("[OK] Key A RELEASED detected")
        else:
            print("[FAIL] Key A RELEASED not detected")

        if not key_pressed and not key_released:
            print("\nPossible causes:")
            print("1. K10 not running or crashed (restart K10)")
            print("2. Key A hardware fault")
            print("3. Serial connection issue")
            print("4. Firmware not flashed correctly")

    except serial.SerialException as e:
        print(f"[ERROR] Serial port error: {e}")
        return False
    except KeyboardInterrupt:
        print("\n\nInterrupted by user")
        return False

    return key_pressed or key_released

if __name__ == '__main__':
    port = sys.argv[1] if len(sys.argv) > 1 else 'COM4'
    success = monitor_k10(port)
    sys.exit(0 if success else 1)

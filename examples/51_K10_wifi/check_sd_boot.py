#!/usr/bin/env python3
"""
K10 启动日志捕获脚本
连接串口并触发复位，捕获 SD 卡初始化日志
"""
import serial
import time
import sys

PORT = 'COM4'
BAUDRATE = 115200
TIMEOUT = 15  # 秒

def main():
    print(f"连接到 {PORT} @ {BAUDRATE} baud...")

    try:
        ser = serial.Serial(PORT, BAUDRATE, timeout=1)
        print("串口已打开")

        # 触发硬件复位 (DTR/RTS toggle)
        print("触发复位...")
        ser.setDTR(False)
        ser.setRTS(False)
        time.sleep(0.1)
        ser.setDTR(True)
        ser.setRTS(True)
        time.sleep(0.5)

        print("\n" + "="*60)
        print("捕获启动日志 (15秒)...")
        print("="*60 + "\n")

        start_time = time.time()
        sd_found = False

        while (time.time() - start_time) < TIMEOUT:
            if ser.in_waiting > 0:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    print(line)

                    # 检测 SD 卡相关日志
                    if 'SD Card Init' in line or 'SD card' in line or 'sd_card_ready' in line:
                        sd_found = True

        print("\n" + "="*60)
        if sd_found:
            print("✓ 已找到 SD 卡初始化日志")
        else:
            print("⚠ 未检测到 SD 卡初始化日志")
        print("="*60)

        ser.close()

    except serial.SerialException as e:
        print(f"串口错误: {e}")
        sys.exit(1)
    except KeyboardInterrupt:
        print("\n中断")
        sys.exit(0)

if __name__ == '__main__':
    main()

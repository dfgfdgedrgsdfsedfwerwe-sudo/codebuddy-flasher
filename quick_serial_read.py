#!/usr/bin/env python3
"""Quick serial port reader for debugging"""

import serial
import sys
import time

def read_serial(port, baudrate=115200, duration=10):
    """Read serial port for specified duration"""
    print(f"=== Reading {port} at {baudrate} baud for {duration}s ===\n")

    try:
        ser = serial.Serial(port, baudrate, timeout=0.1)
        print(f"[OK] Port opened\n")

        start_time = time.time()
        line_count = 0

        while time.time() - start_time < duration:
            if ser.in_waiting > 0:
                try:
                    line = ser.readline().decode('utf-8', errors='ignore').strip()
                    if line:
                        print(line)
                        line_count += 1
                except Exception as e:
                    print(f"[DECODE ERROR] {e}")

            time.sleep(0.01)

        ser.close()
        print(f"\n[DONE] Read {line_count} lines in {duration}s")

    except Exception as e:
        print(f"[ERROR] {e}")
        return False

    return True

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python quick_serial_read.py <COM_PORT> [baudrate] [duration]")
        sys.exit(1)

    port = sys.argv[1]
    baudrate = int(sys.argv[2]) if len(sys.argv) > 2 else 115200
    duration = int(sys.argv[3]) if len(sys.argv) > 3 else 10

    read_serial(port, baudrate, duration)

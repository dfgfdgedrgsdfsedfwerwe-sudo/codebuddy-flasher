#!/usr/bin/env python3
"""
测试 Dongle HID 键盘输入
监听来自 CodeBuddy Dongle 的按键事件
"""

import sys
import time

try:
    from pynput import keyboard
except ImportError:
    print("❌ 需要安装 pynput 库")
    print("   请运行: pip install pynput")
    sys.exit(1)

print("=== CodeBuddy Dongle 键盘输入测试 ===")
print()
print("✅ 监听器已启动")
print("📌 请按 K10 上的按键...")
print("📌 按 Ctrl+C 退出")
print()

key_count = 0

def on_press(key):
    global key_count
    key_count += 1
    try:
        print(f"[{key_count}] 按下: {key.char}")
    except AttributeError:
        print(f"[{key_count}] 特殊键: {key}")

def on_release(key):
    try:
        print(f"     释放: {key.char}")
    except AttributeError:
        print(f"     释放: {key}")

    if key == keyboard.Key.esc:
        print("\n⛔ ESC 键按下，退出监听")
        return False

# 启动监听器
with keyboard.Listener(on_press=on_press, on_release=on_release) as listener:
    try:
        listener.join()
    except KeyboardInterrupt:
        print("\n\n✅ 测试结束")
        print(f"📊 总共接收到 {key_count} 次按键")

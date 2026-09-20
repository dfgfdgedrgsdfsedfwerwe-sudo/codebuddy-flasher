#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
猜数字游戏 - ATK BOX 触摸交互版

游戏规则:
1. 电脑随机选一个 1-100 的数字
2. 你在 ATK BOX 上选择范围，逐步逼近
3. 电脑给提示「太大/太小/猜中」
4. 最多 7 次机会

玩法:
- 每回合给你当前范围内的 4 个数字选项
- 在盒子上触摸你认为的答案
- 猜中后显示用了几次

架构:
- 通过 daemon (127.0.0.1:47100) 发送决策请求
- 与 MCP server 共存，不抢占串口
"""

import os
import sys
import io

# Windows 中文支持（必须在其他 import 之前）
if sys.platform == 'win32':
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', line_buffering=True)
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', line_buffering=True)
    try:
        os.system('chcp 65001 >nul 2>&1')
    except:
        pass

import random
import time
import socket
import json
from pathlib import Path

# ========================================
# 游戏配置
# ========================================
MIN_NUM = 1
MAX_NUM = 100
MAX_ATTEMPTS = 7
DAEMON_HOST = "127.0.0.1"
DAEMON_PORT = 47100
TIMEOUT = 40.0


def ask_via_daemon(title, options):
    """通过 daemon IPC 发送决策请求到 ATK BOX

    参数:
        title: 问题标题（英文，最多31字符）
        options: 选项列表（英文，1-4个，每个最多23字符）

    返回:
        chosen_index (0-3) 或 None（超时/取消/错误）
    """
    if not options or len(options) > 4:
        print(f"[错误] 选项数量必须 1-4，当前 {len(options)}")
        return None

    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(TIMEOUT)
        sock.connect((DAEMON_HOST, DAEMON_PORT))

        request = {
            "type": "decision",
            "title": title[:31],
            "options": [opt[:23] for opt in options],
        }
        sock.sendall(json.dumps(request).encode('utf-8') + b'\n')

        # 阻塞等待用户选择
        data = b""
        while b"\n" not in data:
            chunk = sock.recv(1024)
            if not chunk:
                break
            data += chunk
        sock.close()

        if not data:
            print("[错误] daemon 无响应")
            return None

        response = json.loads(data.decode('utf-8').strip())
        if response.get("ok"):
            chosen = response.get("chosen", 255)
            return None if chosen == 255 else chosen
        else:
            print(f"[错误] daemon: {response.get('error', 'Unknown')}")
            return None

    except ConnectionRefusedError:
        print("[错误] 无法连接 daemon (127.0.0.1:47100)")
        print("       请确认 atkbox_daemon.py 或 MCP server 正在运行")
        return None
    except socket.timeout:
        print("[超时] 等待用户选择超时")
        return None
    except Exception as e:
        print(f"[错误] {e}")
        return None


class GuessNumberGame:
    def __init__(self):
        self.secret = 0
        self.attempts = 0
        self.low = MIN_NUM
        self.high = MAX_NUM
        self.history = []

    def start(self):
        """开始新游戏"""
        self.secret = random.randint(MIN_NUM, MAX_NUM)
        self.attempts = 0
        self.low = MIN_NUM
        self.high = MAX_NUM
        self.history = []

        print("\n" + "="*60)
        print("  [猜数字游戏] - ATK BOX 触摸版")
        print("="*60)
        print(f"  规则: 电脑想了一个 {MIN_NUM}-{MAX_NUM} 的数字")
        print(f"  目标: 在 {MAX_ATTEMPTS} 次内猜中")
        print(f"  方式: 在盒子屏幕上触摸你的答案")
        print("="*60 + "\n")

        time.sleep(2)

        # 开始回合
        while self.attempts < MAX_ATTEMPTS:
            if not self.play_round():
                break

        # 游戏结束
        if self.attempts < MAX_ATTEMPTS:
            print("\n[胜利] 恭喜！你赢了！")
        else:
            print(f"\n[失败] 游戏结束！答案是 {self.secret}")

    def play_round(self):
        """玩一回合，返回 True 继续 / False 猜中"""
        self.attempts += 1

        # 生成 4 个选项（智能分布）
        options_num = self.generate_options()
        options_str = [str(n) for n in options_num]

        # 构造标题
        title = f"Round {self.attempts}/{MAX_ATTEMPTS}"
        if self.history:
            last = self.history[-1]
            title += f" ({last['guess']}: {last['hint']})"

        # 显示当前范围
        print(f"\n[回合 {self.attempts}] 当前范围: {self.low} - {self.high}")
        if self.history:
            print(f"  上次提示: {self.history[-1]['hint']}")

        # 推送到 BOX（通过 daemon）
        choice = ask_via_daemon(title, options_str)

        if choice is None:
            print("[超时] 超时或取消，游戏结束")
            return False

        guess = options_num[choice]
        print(f"\n你猜: {guess}")

        # 判断结果
        if guess == self.secret:
            print(f"[命中] 猜中了！用了 {self.attempts} 次")
            self.show_victory_screen(guess)
            return False
        elif guess < self.secret:
            hint = "Too Low"
            self.low = max(self.low, guess + 1)
            print(f"[提示] 太小了！")
        else:
            hint = "Too High"
            self.high = min(self.high, guess - 1)
            print(f"[提示] 太大了！")

        self.history.append({'guess': guess, 'hint': hint})

        # 检查范围收敛
        if self.low > self.high:
            print(f"[异常] 范围矛盾（{self.low} > {self.high}），游戏结束")
            return False

        time.sleep(1)
        return True

    def generate_options(self):
        """生成 4 个选项（智能分布在当前范围内）"""
        range_size = self.high - self.low + 1

        if range_size <= 4:
            # 范围≤4，直接返回所有
            return list(range(self.low, self.high + 1))

        # 范围>4，智能采样（四分位）
        options = []
        step = range_size / 5

        for i in [1, 2, 3, 4]:
            val = int(self.low + step * i)
            val = max(self.low, min(self.high, val))
            if val not in options:
                options.append(val)

        # 补齐到 4 个（如果去重后不足）
        while len(options) < 4 and len(options) < range_size:
            val = random.randint(self.low, self.high)
            if val not in options:
                options.append(val)

        return sorted(options)

    def show_victory_screen(self, guess):
        """显示胜利画面"""
        title = f"YOU WIN! ({self.attempts} tries)"
        options = ["Play Again", "Quit"]

        print("\n" + "*"*40)
        print(f"  答案: {self.secret}")
        print(f"  次数: {self.attempts}")
        print("*"*40 + "\n")

        choice = ask_via_daemon(title, options)

        if choice == 0:
            print("\n开始新游戏...\n")
            self.start()
        else:
            print("\n感谢游玩！\n")


def main():
    print(f"[连接] 使用 daemon IPC (127.0.0.1:47100)")
    print("[提示] 确保 ATK BOX 和 Dongle 已连接\n")

    try:
        game = GuessNumberGame()
        game.start()
    except KeyboardInterrupt:
        print("\n\n游戏中断\n")
    except Exception as e:
        print(f"\n[错误] {e}\n")
        import traceback
        traceback.print_exc()


if __name__ == '__main__':
    main()

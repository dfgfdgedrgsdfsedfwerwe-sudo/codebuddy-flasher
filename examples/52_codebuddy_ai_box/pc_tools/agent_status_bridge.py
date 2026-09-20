#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
agent_status_bridge.py - Claude Code Agent 工作流可视化桥接

功能：
1. 监听 Claude Code session 状态变化（通过 history.jsonl / session 文件）
2. 实时推送 Agent 状态到 AI BOX：THINKING / RUNNING / DONE
3. 检测审批请求（AskUserQuestion / approval hooks），推送审批界面
4. 支持离线检测（与 Dongle 连接断开时显示离线横幅）

协议：
- 0x0C Agent Status Frame (134 bytes): state(1) + task_desc(64) + detail(64) + offline(1) + padding(3) + crc8(1)
- 0x0D Agent Approval Request (134 bytes): task_id(2) + title(64) + description(64) + padding(3) + crc8(1)

用法：
  python agent_status_bridge.py COM6 [--interval 2]

  COM6: Dongle 串口
  --interval: 状态刷新间隔（秒，默认 2s）
"""

import os
import sys
import io

# Windows 中文支持
if sys.platform == 'win32':
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', line_buffering=True)
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', line_buffering=True)
    try:
        os.system('chcp 65001 >nul 2>&1')
    except:
        pass

import serial
import struct
import time
import json
import argparse
from pathlib import Path
from datetime import datetime

# ============================================================
# 协议常量
# ============================================================
MAGIC = bytes([0xA5, 0x5A])
FRAME_TYPE_AGENT_STATUS = 0x0C
FRAME_TYPE_AGENT_APPROVAL_REQ = 0x0D
FRAME_TYPE_AGENT_APPROVAL_REPLY = 0x0E  # 接收审批回执

# Agent 状态枚举（与固件一致）
AGENT_STATE_IDLE = 0
AGENT_STATE_THINKING = 1
AGENT_STATE_RUNNING = 2
AGENT_STATE_DONE = 3

def crc8(data):
    """CRC8 校验（多项式 0x07）"""
    crc = 0x00
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc << 1) ^ 0x07 if crc & 0x80 else crc << 1
            crc &= 0xFF
    return crc

def wrap_frame(payload):
    """封装帧：[魔数 2B][长度 1B][payload][CRC8 1B]"""
    frame = bytearray(MAGIC)
    frame.append(len(payload))
    frame.extend(payload)
    frame.append(crc8(payload))
    return bytes(frame)

def build_agent_status_frame(state, task_desc, detail, offline=False):
    """构造 Agent 状态帧 (0x0C, 134 bytes)

    Args:
        state: AGENT_STATE_IDLE/THINKING/RUNNING/DONE
        task_desc: 任务描述（最多64字节UTF-8）
        detail: 详细信息（最多64字节UTF-8）
        offline: 是否离线（True显示离线横幅）
    """
    frame = bytearray([FRAME_TYPE_AGENT_STATUS])
    frame.append(state)
    frame.extend(task_desc.encode('utf-8')[:64].ljust(64, b'\0'))
    frame.extend(detail.encode('utf-8')[:64].ljust(64, b'\0'))
    frame.append(1 if offline else 0)
    frame.extend([0, 0, 0])  # padding 3字节
    frame.append(crc8(frame))
    return wrap_frame(frame)

def build_approval_request_frame(task_id, title, description):
    """构造审批请求帧 (0x0D, 134 bytes)

    Args:
        task_id: 任务ID（1-65535）
        title: 审批标题（最多64字节UTF-8）
        description: 审批描述（最多64字节UTF-8）
    """
    frame = bytearray([FRAME_TYPE_AGENT_APPROVAL_REQ])
    frame.extend(struct.pack('<H', task_id))  # 小端序 uint16
    frame.extend(title.encode('utf-8')[:64].ljust(64, b'\0'))
    frame.extend(description.encode('utf-8')[:64].ljust(64, b'\0'))
    frame.extend([0, 0, 0])  # padding 3字节
    frame.append(crc8(frame))
    return wrap_frame(frame)

def send_frame(ser, frame):
    """发送帧并等待 Dongle 回显（带重试）"""
    max_retries = 3
    for attempt in range(max_retries):
        try:
            ser.write(frame)
            ser.flush()
            time.sleep(0.05)  # 等待 Dongle 转发
            return True
        except Exception as e:
            if attempt == max_retries - 1:
                print(f"[!] 发送失败（{attempt+1}/{max_retries}）: {e}")
                return False
            time.sleep(0.2)
    return False

# ============================================================
# Claude Code Session 状态解析
# ============================================================
def find_active_sessions():
    """查找活跃的 Claude Code 会话（15分钟内更新）"""
    sessions_dir = Path.home() / '.claude' / 'sessions'
    if not sessions_dir.exists():
        return []

    active = []
    cutoff = time.time() - 15 * 60  # 15分钟前

    for session_dir in sessions_dir.iterdir():
        if not session_dir.is_dir():
            continue
        history = session_dir / 'history.jsonl'
        if not history.exists():
            continue
        if history.stat().st_mtime < cutoff:
            continue
        active.append(session_dir)

    return active

def parse_session_state(session_dir):
    """解析单个 session 的状态

    Returns:
        dict: {
            'state': AGENT_STATE_*,
            'task': str,
            'detail': str,
            'approval_pending': bool,
            'approval_info': dict or None
        }
    """
    history = session_dir / 'history.jsonl'
    if not history.exists():
        return None

    # 读取最后 10 行（包含最近状态）
    lines = []
    try:
        with open(history, 'r', encoding='utf-8') as f:
            lines = f.readlines()[-10:]
    except:
        return None

    state = AGENT_STATE_IDLE
    task = "Idle"
    detail = ""
    approval_pending = False
    approval_info = None

    # 反向扫描（最新状态优先）
    for line in reversed(lines):
        try:
            entry = json.loads(line.strip())
            role = entry.get('role')
            content = entry.get('content', '')

            # 检测 Agent 工作中（assistant 输出且包含 tool_use）
            if role == 'assistant':
                tool_uses = [b for b in entry.get('content', []) if isinstance(b, dict) and b.get('type') == 'tool_use']
                if tool_uses:
                    state = AGENT_STATE_RUNNING
                    # 提取第一个工具调用名称
                    tool_name = tool_uses[0].get('name', 'unknown')
                    task = f"Running: {tool_name}"
                    detail = "Agent is working..."
                    break

            # 检测审批请求（AskUserQuestion 工具）
            if role == 'assistant' and 'AskUserQuestion' in str(content):
                approval_pending = True
                # 尝试提取问题文本
                for block in entry.get('content', []):
                    if isinstance(block, dict) and block.get('type') == 'tool_use' and block.get('name') == 'AskUserQuestion':
                        input_data = block.get('input', {})
                        questions = input_data.get('questions', [])
                        if questions:
                            q = questions[0]
                            approval_info = {
                                'task_id': hash(q.get('question', '')) % 65535 + 1,  # 简单哈希生成ID
                                'title': q.get('question', 'Approve this action?')[:64],
                                'description': q.get('header', 'Review required')[:64]
                            }
                        break
                state = AGENT_STATE_THINKING
                task = "Waiting for approval"
                break

            # 检测用户输入（user role）
            if role == 'user':
                state = AGENT_STATE_THINKING
                task = "Thinking..."
                detail = content[:50] if isinstance(content, str) else "Processing request"
                break

        except json.JSONDecodeError:
            continue

    return {
        'state': state,
        'task': task,
        'detail': detail,
        'approval_pending': approval_pending,
        'approval_info': approval_info
    }

def aggregate_states(sessions_states):
    """聚合多个 session 的状态（按优先级）

    优先级：RUNNING > THINKING > DONE > IDLE
    """
    if not sessions_states:
        return {
            'state': AGENT_STATE_IDLE,
            'task': 'No active session',
            'detail': '',
            'approval_pending': False,
            'approval_info': None
        }

    # 按优先级排序
    priority = {
        AGENT_STATE_RUNNING: 3,
        AGENT_STATE_THINKING: 2,
        AGENT_STATE_DONE: 1,
        AGENT_STATE_IDLE: 0
    }

    sessions_states.sort(key=lambda s: priority.get(s['state'], 0), reverse=True)
    top = sessions_states[0]

    # 如果有审批请求，优先返回
    for s in sessions_states:
        if s['approval_pending']:
            return s

    return top

# ============================================================
# 主循环
# ============================================================
def main():
    parser = argparse.ArgumentParser(description='Agent 状态桥接（Claude Code → AI BOX）')
    parser.add_argument('port', help='Dongle 串口（如 COM6）')
    parser.add_argument('--interval', type=int, default=2, help='状态刷新间隔（秒，默认2）')
    args = parser.parse_args()

    print("=== Agent Status Bridge ===")
    print(f"串口: {args.port}")
    print(f"刷新间隔: {args.interval}s")
    print("监听 Claude Code 会话...\n")

    try:
        ser = serial.Serial(args.port, 115200, timeout=0.5)
    except Exception as e:
        print(f"[!] 无法打开串口 {args.port}: {e}")
        return 1

    last_state_sent = None
    last_approval_sent = None

    try:
        while True:
            # 查找活跃会话
            sessions = find_active_sessions()
            states = [parse_session_state(s) for s in sessions]
            states = [s for s in states if s]  # 过滤 None

            # 聚合状态
            current = aggregate_states(states)

            # 发送审批请求（如果有新的审批）
            if current['approval_pending'] and current['approval_info']:
                info = current['approval_info']
                if info != last_approval_sent:
                    frame = build_approval_request_frame(
                        info['task_id'],
                        info['title'],
                        info['description']
                    )
                    if send_frame(ser, frame):
                        print(f"[→] 审批请求: {info['title']}")
                        last_approval_sent = info

            # 发送 Agent 状态（避免重复发送）
            state_key = (current['state'], current['task'], current['detail'])
            if state_key != last_state_sent:
                frame = build_agent_status_frame(
                    current['state'],
                    current['task'],
                    current['detail'],
                    offline=False
                )
                if send_frame(ser, frame):
                    state_name = ['IDLE', 'THINKING', 'RUNNING', 'DONE'][current['state']]
                    print(f"[→] Agent: {state_name} | {current['task']}")
                    last_state_sent = state_key

            time.sleep(args.interval)

    except KeyboardInterrupt:
        print("\n[*] 用户中断，退出")
    except Exception as e:
        print(f"[!] 运行错误: {e}")
        import traceback
        traceback.print_exc()
    finally:
        ser.close()

    return 0

if __name__ == '__main__':
    sys.exit(main())

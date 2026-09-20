#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ask_user_via_box.py - Claude Code 工具：通过 AI BOX 向用户询问选择

集成到 Claude Code 工作流：我需要决策时调用此工具，
选项推送到 AI BOX，用户触摸选择，结果返回给我。
"""

import os
import sys
import io

# Windows 中文支持（必须在其他 import 之前）
if sys.platform == 'win32':
    # 强制 stdout/stderr 使用 UTF-8（绕过控制台编码）
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', line_buffering=True)
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', line_buffering=True)
    # 尝试设置控制台代码页（可能失败，但不影响上面的 wrapper）
    try:
        os.system('chcp 65001 >nul 2>&1')
    except:
        pass

import serial
import struct
import time
import threading
import json
from pathlib import Path

# ============================================================
# 运行时协调（与 realtime_monitor.py 共享）
# ============================================================
RUNTIME_DIR = Path(__file__).parent / '.runtime'
MONITOR_PID_FILE = RUNTIME_DIR / 'monitor.pid'
PAUSE_FLAG_FILE = RUNTIME_DIR / 'pause.flag'

def ensure_runtime_dir():
    """确保运行时目录存在"""
    RUNTIME_DIR.mkdir(exist_ok=True)

def request_serial_pause():
    """请求监控脚本暂停（写入 pause.flag）"""
    ensure_runtime_dir()
    try:
        PAUSE_FLAG_FILE.write_text(str(os.getpid()))
        time.sleep(2)  # 等待监控脚本检测并释放串口（增加到2秒）
        return True
    except Exception as e:
        print(f"[!] 无法创建暂停标志: {e}")
        return False

def release_serial_pause():
    """释放暂停（删除 pause.flag）"""
    try:
        if PAUSE_FLAG_FILE.exists():
            PAUSE_FLAG_FILE.unlink()
    except Exception as e:
        print(f"[!] 无法删除暂停标志: {e}")

# ============================================================
# 协议常量
# ============================================================
MAGIC = bytes([0xA5, 0x5A])
FRAME_TYPE_DECISION_REQ = 0x0A
FRAME_TYPE_DECISION_REPLY = 0x0B

# 全局决策队列（支持多窗口合并）
_decision_queue = []
_queue_lock = threading.Lock()
_pending_file = Path.home() / '.claude' / 'decision_pending.json'
_serial_port = None
_serial_lock = threading.Lock()

def get_serial(port):
    """获取或创建串口连接（单例模式）"""
    global _serial_port
    with _serial_lock:
        if _serial_port is None or not _serial_port.is_open:
            _serial_port = serial.Serial(port, 115200, timeout=0.5)
        return _serial_port

def close_serial():
    """关闭串口连接"""
    global _serial_port
    with _serial_lock:
        if _serial_port and _serial_port.is_open:
            _serial_port.close()
        _serial_port = None

def crc8(data):
    crc = 0x00
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc << 1) ^ 0x07 if crc & 0x80 else crc << 1
            crc &= 0xFF
    return crc

def wrap_frame(payload):
    frame = bytearray(MAGIC)
    frame.append(len(payload))
    frame.extend(payload)
    frame.append(crc8(payload))
    return bytes(frame)

def build_decision_frame(decision_id, title, options):
    """构造决策请求帧
    decision_id: 1-255
    title: 最多32字节（英文）
    options: 列表，最多4个，每个最多24字节（英文）
    """
    frame = bytearray([FRAME_TYPE_DECISION_REQ])
    frame.extend(struct.pack('<H', decision_id))  # id (2字节)
    frame.append(0)  # kind=0 (单选)

    title_bytes = title.encode('utf-8')[:32].ljust(32, b'\0')
    frame.extend(title_bytes)

    frame.append(min(len(options), 4))  # opt_count

    for i in range(4):
        if i < len(options):
            opt_bytes = options[i].encode('utf-8')[:24].ljust(24, b'\0')
        else:
            opt_bytes = b'\0' * 24
        frame.extend(opt_bytes)

    frame.append(crc8(frame))
    return frame

def wait_for_reply(ser, decision_id, timeout=None):
    """等待 AI BOX 回执
    返回: chosen_index (0-3) 或 None（超时/取消）
    timeout=None 时永久等待
    """
    start = time.time()
    buffer = bytearray()

    while timeout is None or (time.time() - start < timeout):
        if ser.in_waiting > 0:
            buffer.extend(ser.read(ser.in_waiting))

        # 查找完整回执帧: A5 5A 05 0B id(2) chosen(1) crc(1)
        while len(buffer) >= 8:
            if buffer[0] == 0xA5 and buffer[1] == 0x5A and buffer[2] == 5 and buffer[3] == FRAME_TYPE_DECISION_REPLY:
                reply_id = struct.unpack('<H', buffer[4:6])[0]
                chosen = buffer[6]
                frame_crc = buffer[7]

                # 验证
                payload = buffer[3:7]
                if crc8(payload) == frame_crc and reply_id == decision_id:
                    if chosen == 0xFF:  # 用户取消
                        return None
                    return chosen

                buffer = buffer[8:]
            else:
                buffer = buffer[1:]

        time.sleep(0.1)

    return None  # 超时

# 中英映射表（常见决策词汇）
TRANSLATION_MAP = {
    # 是否类
    "是": "Yes", "否": "No", "确认": "Confirm", "取消": "Cancel",
    "同意": "Agree", "拒绝": "Reject", "继续": "Continue", "停止": "Stop",
    "保存": "Save", "放弃": "Discard", "重试": "Retry", "跳过": "Skip",

    # 操作类
    "开始": "Start", "结束": "End", "暂停": "Pause", "恢复": "Resume",
    "上传": "Upload", "下载": "Download", "删除": "Delete", "编辑": "Edit",

    # 模式类
    "自动": "Auto", "手动": "Manual", "快速": "Fast", "慢速": "Slow",
    "正常": "Normal", "调试": "Debug", "测试": "Test", "生产": "Production",

    # 状态类
    "启用": "Enable", "禁用": "Disable", "打开": "Open", "关闭": "Close",

    # 数字
    "选项A": "Option A", "选项B": "Option B", "选项C": "Option C", "选项D": "Option D",
}

def load_pending_decisions():
    """从文件加载待处理决策队列，清理超时的旧决策"""
    global _decision_queue
    try:
        if _pending_file.exists():
            with open(_pending_file, 'r', encoding='utf-8') as f:
                _decision_queue = json.load(f)
            # 清理5分钟前的旧决策
            now = time.time()
            _decision_queue = [d for d in _decision_queue if now - d.get('timestamp', 0) < 300]
    except:
        _decision_queue = []

def save_pending_decisions():
    """保存待处理决策队列到文件"""
    try:
        _pending_file.parent.mkdir(parents=True, exist_ok=True)
        with open(_pending_file, 'w', encoding='utf-8') as f:
            json.dump(_decision_queue, f, ensure_ascii=False, indent=2)
    except:
        pass

def add_to_queue(title_cn, options_cn, decision_id):
    """添加决策到队列"""
    with _queue_lock:
        load_pending_decisions()
        _decision_queue.append({
            'id': decision_id,
            'title': title_cn,
            'options': options_cn,
            'timestamp': time.time()
        })
        save_pending_decisions()

def remove_from_queue(decision_id):
    """从队列移除已完成的决策"""
    with _queue_lock:
        load_pending_decisions()
        _decision_queue[:] = [d for d in _decision_queue if d['id'] != decision_id]
        save_pending_decisions()

def build_merged_decision():
    """合并所有待处理决策，返回合并后的标题和选项"""
    with _queue_lock:
        load_pending_decisions()

        if not _decision_queue:
            return None, None, None

        if len(_decision_queue) == 1:
            # 单个决策，直接返回
            d = _decision_queue[0]
            return d['title'], d['options'], [d['id']]

        # 多个决策，合并显示
        merged_title = f"多窗口决策 ({len(_decision_queue)} 个问题)"
        merged_options = []
        decision_ids = []

        for d in _decision_queue:
            decision_ids.append(d['id'])
            for opt in d['options']:
                label = f"{d['title']}: {opt}"
                if len(label) > 24:
                    label = label[:21] + "..."
                merged_options.append(label)
                if len(merged_options) >= 4:
                    break
            if len(merged_options) >= 4:
                break

        return merged_title, merged_options, decision_ids


def translate_to_english(text):
    """中文转英文（映射表 + 拼音回退）"""
    if not text:
        return text

    # 直接命中映射表
    if text in TRANSLATION_MAP:
        return TRANSLATION_MAP[text]

    # 检查是否包含中文字符
    has_chinese = any('一' <= c <= '鿿' for c in text)

    if not has_chinese:
        return text  # 已经是英文/数字

    # 中文但不在映射表 → 用拼音自动转换
    try:
        from pypinyin import lazy_pinyin
        pinyin = ''.join(lazy_pinyin(text))
        # 首字母大写，去掉数字（声调）
        pinyin = ''.join(c for c in pinyin if c.isalpha())
        return pinyin.capitalize() if pinyin else text
    except ImportError:
        # pypinyin 未安装，返回标记
        return f"[CN:{text}]"

def ask_decision(port, title, options, timeout=None):
    """通过 AI BOX 询问用户选择（支持多窗口队列合并）

    参数:
        port: 串口号 (如 'COM6')
        title: 问题标题（中文或英文，最多32字符）
        options: 选项列表（中文或英文，最多4个，每个最多24字符）
        timeout: 等待超时（秒），None=永久等待

    返回:
        chosen_index: 0-3（用户选择的索引）
        None: 超时或用户取消
    """
    try:
        # 生成决策 ID（时间戳取模）
        decision_id = int(time.time() * 1000) % 65535

        # 添加到队列
        add_to_queue(title, options, decision_id)

        # 短暂延迟，让其他窗口也能加入队列（所有窗口都等待）
        time.sleep(0.3)

        # 检查是否有多个待处理决策
        merged_title, merged_options, decision_ids = build_merged_decision()

        if merged_title is None:
            return None  # 队列为空（不应该发生）

        # 只有第一个决策负责发送和等待
        is_first = (decision_ids[0] == decision_id)

        if not is_first:
            # 非首个决策，等待首个决策完成
            # 轮询检查队列，看自己是否被移除（表示已处理）
            print(f"\n[等待] 已加入队列，由主窗口处理...\n")
            wait_start = time.time()
            while timeout is None or (time.time() - wait_start < timeout):
                with _queue_lock:
                    load_pending_decisions()
                    if not any(d['id'] == decision_id for d in _decision_queue):
                        # 已被处理，但我们不知道结果
                        # 返回 None 表示由其他窗口处理了
                        print(f"[完成] 决策已由主窗口处理\n")
                        return None
                time.sleep(0.5)
            print(f"[超时] 主窗口未完成决策\n")
            return None

        # === 以下代码只有首个决策执行 ===

        # === 请求监控脚本暂停（释放串口）===
        print("[COORD] 请求监控脚本释放串口...")
        request_serial_pause()

        # === 翻译：PC显示中文，BOX发送英文 ===
        title_cn = merged_title  # PC端显示用
        title_en = translate_to_english(merged_title)  # BOX端发送用

        options_cn = merged_options  # PC端显示用
        options_en = [translate_to_english(opt) for opt in merged_options]  # BOX端发送用

        # === 电脑端显示选项（中文） ===
        print("\n" + "="*60)
        print(f"  {title_cn}")
        print("="*60)
        for i, opt in enumerate(options_cn):
            print(f"  [{i}]  {opt}")
        print("="*60)
        if timeout is None:
            print(f"请在 AI BOX 屏幕上触摸选择（永久等待，按B键取消）\n")
        else:
            print(f"请在 AI BOX 屏幕上触摸选择（{timeout}秒超时）\n")

        # 获取串口（共享）
        ser = get_serial(port)

        # 发送决策请求到 AI BOX（英文）
        frame = build_decision_frame(decision_ids[0], title_en, options_en)
        ser.write(wrap_frame(frame))
        ser.flush()

        # 等待 AI BOX 触摸回执
        result = wait_for_reply(ser, decision_ids[0], timeout)

        # === 电脑端显示结果（中文） ===
        print("\n" + "-"*60)
        if result is not None:
            print(f"[OK] 用户选择: [{result}] {options_cn[result]}")
            print("-"*60 + "\n")

            # 清空所有队列（合并决策一次处理完所有）
            with _queue_lock:
                _decision_queue.clear()
                save_pending_decisions()

            # === 释放串口暂停 ===
            release_serial_pause()

            return result
        else:
            print(f"[TIMEOUT] 超时或用户取消")
            print("-"*60 + "\n")

            # 清空所有队列
            with _queue_lock:
                _decision_queue.clear()
                save_pending_decisions()

            # === 释放串口暂停 ===
            release_serial_pause()

            return None

    except Exception as e:
        print(f"\n[错误] {e}\n")
        # 出错也要清理队列和释放暂停
        try:
            remove_from_queue(decision_id)
            release_serial_pause()
        except:
            pass
        return None

if __name__ == '__main__':
    # 命令行测试模式
    if len(sys.argv) < 4:
        print("用法: python ask_user_via_box.py <COM端口> <标题> <选项1> [选项2] [选项3] [选项4]")
        print("示例: python ask_user_via_box.py COM6 'Choose approach' 'Option A' 'Option B'")
        sys.exit(1)

    port = sys.argv[1]
    title = sys.argv[2]
    options = sys.argv[3:7]  # 最多4个

    result = ask_decision(port, title, options)

    if result is not None:
        print(f"\n返回值: {result}")
        sys.exit(0)
    else:
        print("\n无返回值")
        sys.exit(1)

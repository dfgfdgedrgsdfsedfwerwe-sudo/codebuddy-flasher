#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
CodeBuddy 实时监控 - 真实数据版
从 Claude Code 会话提取实时状态推送到 ATK BOX

支持两种模式：
- 单仓库模式: 监控指定的一个 Git 仓库
- 多窗口模式 (--auto): 自动检测所有活跃 Claude 窗口，聚合显示

数据源：
- Token: ~/.claude/history.jsonl 文件大小估算（1KB ≈ 250 tokens，全局共享）
- 项目: 各活跃窗口工作目录的 Git 状态（多窗口模式下每窗口一行，最多6个）
- AI状态: ~/.claude/sessions/{pid}.json 的 status 字段
         多窗口下按优先级聚合: busy(Coding) > thinking(Thinking) > idle(Done)

活跃判定: session 文件 15 分钟内有更新，或对应进程仍在运行
"""

import os
import sys

# Windows 中文支持（必须在其他 import 之前）
if sys.platform == 'win32':
    # 设置环境变量（影响当前进程的所有 I/O，包括重定向到文件）
    os.environ['PYTHONIOENCODING'] = 'utf-8'
    # 设置控制台代码页（直接输出到控制台时生效）
    import ctypes
    kernel32 = ctypes.windll.kernel32
    kernel32.SetConsoleOutputCP(65001)

import os
import struct
import serial
import time
import json
import subprocess
from pathlib import Path
from threading import Thread, Event

# ============================================================
# 运行时协调（串口暂停/恢复）
# ============================================================
RUNTIME_DIR = Path(__file__).parent / '.runtime'
MONITOR_PID_FILE = RUNTIME_DIR / 'monitor.pid'
PAUSE_FLAG_FILE = RUNTIME_DIR / 'pause.flag'

def is_process_alive(pid):
    """检查进程是否存活（Windows tasklist）"""
    try:
        result = subprocess.run(['tasklist', '/FI', f'PID eq {pid}', '/NH'],
                              capture_output=True, text=True, timeout=2)
        return str(pid) in result.stdout
    except:
        return False

def ensure_runtime_dir():
    """确保运行时目录存在"""
    RUNTIME_DIR.mkdir(exist_ok=True)

# ============================================================
# 协议常量
# ============================================================
MAGIC = bytes([0xA5, 0x5A])
FRAME_TYPE_TOKEN = 0x07
FRAME_TYPE_PROJECT = 0x08
FRAME_TYPE_AI = 0x09
FRAME_TYPE_DECISION_REQ = 0x0A
FRAME_TYPE_DECISION_REPLY = 0x0B

# ============================================================
# CRC8
# ============================================================
def crc8(data):
    crc = 0x00
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc << 1) ^ 0x07 if crc & 0x80 else crc << 1
            crc &= 0xFF
    return crc

def wrap_frame(payload):
    """魔术头 + payload + 外层CRC"""
    frame = bytearray(MAGIC)
    frame.append(len(payload))
    frame.extend(payload)
    frame.append(crc8(payload))
    return bytes(frame)

# ============================================================
# 帧构造
# ============================================================
def build_token_frame(services):
    """Token 状态帧 (最多5个服务)"""
    frame = bytearray([FRAME_TYPE_TOKEN, min(len(services), 5), 0])  # type + count + seq_num

    for i in range(5):
        if i < len(services):
            name, used, total, pct10 = services[i]
            name_bytes = name.encode('utf-8')[:20].ljust(20, b'\0')
            frame.extend(name_bytes)
            frame.extend(struct.pack('<I', used))   # used tokens
            frame.extend(struct.pack('<I', total))  # total tokens
            frame.extend(struct.pack('<H', pct10))  # percent × 10
        else:
            frame.extend(b'\0' * 30)

    frame.append(crc8(frame))
    return frame

def build_project_frame(projects):
    """项目状态帧 (最多6个项目)"""
    frame = bytearray([FRAME_TYPE_PROJECT, min(len(projects), 6), 0])  # type + count + seq_num

    for i in range(6):
        if i < len(projects):
            name, status_code = projects[i]
            name_bytes = name.encode('utf-8')[:24].ljust(24, b'\0')
            frame.extend(name_bytes)
            frame.append(status_code)
        else:
            frame.extend(b'\0' * 25)

    frame.append(crc8(frame))
    return frame

def build_ai_frame(emotion, text):
    """AI 情绪帧
    emotion: 0=Thinking, 1=Coding, 2=Done
    text: 自定义文字 (最多20字节)
    """
    frame = bytearray([FRAME_TYPE_AI, emotion, 0])  # type + emotion + seq_num
    text_bytes = text.encode('utf-8')[:20].ljust(20, b'\0')
    frame.extend(text_bytes)
    frame.append(crc8(frame))
    return frame

def build_decision_frame(decision_id, title, options):
    """决策请求帧
    decision_id: 1-65535
    title: 最多32字节
    options: 列表，最多4个，每个最多24字节
    返回: 134字节完整帧（含内层CRC）
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

# ============================================================
# 数据提取
# ============================================================
def get_token_usage():
    """从 history.jsonl 文件大小估算 Token 用量
    返回: [(name, used, total, pct10), ...]
    """
    history_file = Path.home() / '.claude' / 'history.jsonl'

    if not history_file.exists():
        return [("Claude Opus", 0, 200000, 0)]

    # 文件大小（字节）转 tokens（粗略: 1KB ~= 250 tokens）
    size_kb = history_file.stat().st_size / 1024
    estimated_tokens = int(size_kb * 250)

    # 假设预算 200K
    total = 200000
    used = min(estimated_tokens, total)
    pct10 = int((used / total) * 1000) if total > 0 else 0  # 千分比整数 (BOX 协议字段)

    return [
        ("Claude Opus", used, total, pct10)
    ]

def get_git_projects_from_sessions():
    """从所有活跃会话的工作目录提取 Git 状态
    返回: [(name, status_code), ...]
    status_code: 0=Planning 1=Coding 2=Review 3=Done 4=Error 5=Idle
    """
    sessions = get_active_sessions()
    projects = []

    for session in sessions[:6]:  # 最多6个项目（AI BOX 界面限制）
        cwd = session['cwd']
        if not cwd:
            continue

        # 提取项目名（目录名最后一段）
        project_name = Path(cwd).name[:20]  # 限制20字符

        try:
            # 获取状态
            status_result = subprocess.run(
                ['git', 'status', '--porcelain'],
                cwd=cwd, capture_output=True, text=True, timeout=2
            )

            if status_result.returncode != 0:
                projects.append((project_name, 5))  # Idle（非Git仓库）
                continue

            lines = status_result.stdout.strip().split('\n')
            modified = sum(1 for line in lines if line and line[0] in 'MADR')
            untracked = sum(1 for line in lines if line.startswith('??'))

            # 根据文件状态判断阶段
            if modified > 10 or untracked > 5:
                status = 1  # Coding (大量改动)
            elif modified > 0:
                status = 2  # Review (少量改动)
            elif untracked > 0:
                status = 0  # Planning (只有新文件)
            else:
                status = 3  # Done (干净)

            projects.append((project_name, status))

        except Exception as e:
            projects.append((project_name, 4))  # Error

    return projects if projects else [("No active session", 5)]

def get_git_projects(repo_path):
    """从指定 Git 仓库提取项目状态（兼容旧版，仅用于单仓库模式）
    返回: [(name, status_code), ...]
    status_code: 0=Planning 1=Coding 2=Review 3=Done 4=Error 5=Idle
    """
    try:
        # 获取当前分支
        branch_result = subprocess.run(
            ['git', 'rev-parse', '--abbrev-ref', 'HEAD'],
            cwd=repo_path, capture_output=True, text=True, timeout=2
        )
        branch = branch_result.stdout.strip() if branch_result.returncode == 0 else "unknown"

        # 获取状态
        status_result = subprocess.run(
            ['git', 'status', '--porcelain'],
            cwd=repo_path, capture_output=True, text=True, timeout=2
        )

        if status_result.returncode != 0:
            return [(f"Branch: {branch}", 5)]  # Idle

        lines = status_result.stdout.strip().split('\n')
        modified = sum(1 for line in lines if line and line[0] in 'MADR')
        untracked = sum(1 for line in lines if line.startswith('??'))

        # 根据文件状态判断阶段
        if modified > 10 or untracked > 5:
            status = 1  # Coding (大量改动)
        elif modified > 0:
            status = 2  # Review (少量改动)
        elif untracked > 0:
            status = 0  # Planning (只有新文件)
        else:
            status = 3  # Done (干净)

        return [
            (f"Branch: {branch[:20]}", status),
            (f"Modified: {modified}", 1 if modified > 0 else 5),
            (f"Untracked: {untracked}", 0 if untracked > 0 else 5),
        ]

    except Exception as e:
        return [("Git error", 4)]  # Error

def get_active_sessions():
    """获取所有活跃的 Claude 会话
    返回: [{'pid': int, 'cwd': str, 'status': str, 'name': str, 'updatedAt': int}, ...]
    """
    sessions_dir = Path.home() / '.claude' / 'sessions'
    active_sessions = []

    try:
        current_time = time.time() * 1000  # 毫秒时间戳

        for session_file in sessions_dir.glob('*.json'):
            try:
                with open(session_file, 'r', encoding='utf-8') as f:
                    data = json.load(f)

                # 检查是否在最近 15 分钟内活跃 OR 进程仍在运行
                updated_at = data.get('updatedAt', 0)
                age_ms = current_time - updated_at
                is_recent = age_ms < 15 * 60 * 1000  # 15分钟

                # Windows 进程检查（作为备份判断）
                pid = data.get('pid')
                is_running = False
                if pid:
                    try:
                        import subprocess
                        result = subprocess.run(['tasklist', '/FI', f'PID eq {pid}', '/NH'],
                                              capture_output=True, text=True, timeout=2)
                        is_running = str(pid) in result.stdout
                    except:
                        pass

                # 只要满足一个条件就认为活跃
                if not (is_recent or is_running):
                    continue

                active_sessions.append({
                    'pid': pid,
                    'cwd': data.get('cwd', ''),
                    'status': data.get('status', 'idle'),
                    'name': data.get('name', 'Unknown'),
                    'updatedAt': updated_at
                })
            except:
                continue

        # 按更新时间排序（最新的在前）
        active_sessions.sort(key=lambda s: s['updatedAt'], reverse=True)

    except Exception as e:
        pass

    return active_sessions

def get_ai_emotion():
    """从所有活跃会话读取 AI 情绪（优先级：busy > thinking > idle）
    返回: (emotion_code, text)
    emotion: 0=Thinking, 1=Coding, 2=Done
    """
    sessions = get_active_sessions()

    if not sessions:
        return (0, "No session")

    # 检查是否有任何窗口在 busy 状态
    for s in sessions:
        if s['status'] == 'busy':
            return (1, "Coding...")

    # 检查是否有 thinking 状态
    for s in sessions:
        if s['status'] == 'thinking':
            return (0, "Thinking...")

    # 都是 idle
    return (2, "Done!")

# ============================================================
# 监控线程
# ============================================================
class RealtimeMonitor:
    def __init__(self, port, repo_path=None, interval=5, auto_detect=False):
        self.port = port
        self.repo_path = Path(repo_path) if repo_path else None
        self.ser = None
        self.stop_event = Event()
        self.interval = interval
        self.reconnect_attempts = 0
        self.auto_detect = auto_detect  # 自动检测多窗口模式

        # 决策队列文件（Claude 写请求，监控读取并发送）
        self.decision_queue_file = Path.home() / '.claude' / 'decision_queue.json'
        self.decision_result_file = Path.home() / '.claude' / 'decision_result.json'

        # 运行时协调
        ensure_runtime_dir()
        self.my_pid = os.getpid()
        self.paused = False  # 当前是否处于暂停状态

    def check_pause_signal(self):
        """检查暂停信号，决定是否需要释放串口
        返回: True=需要暂停, False=继续运行
        """
        if not PAUSE_FLAG_FILE.exists():
            return False

        try:
            # 读取请求暂停的进程 PID
            requester_pid = int(PAUSE_FLAG_FILE.read_text().strip())

            # 检查请求者是否还活着
            if not is_process_alive(requester_pid):
                # 请求者已死，清理残留 flag
                print(f"[!] 暂停请求者进程 {requester_pid} 已退出，清理残留标志")
                PAUSE_FLAG_FILE.unlink()
                return False

            # 检查超时（120秒强制恢复）
            mtime = PAUSE_FLAG_FILE.stat().st_mtime
            if time.time() - mtime > 120:
                print(f"[!] 暂停超时（>120s），强制恢复")
                PAUSE_FLAG_FILE.unlink()
                return False

            return True
        except Exception as e:
            print(f"[!] 检查暂停信号异常: {e}")
            return False

    def wait_while_paused(self):
        """等待暂停结束（释放串口，轮询直到 pause.flag 消失）"""
        if self.ser and self.ser.is_open:
            print("[PAUSE] 检测到决策工具请求串口，释放 COM 口...")
            self.ser.close()
            self.ser = None
            self.paused = True

        # 轮询等待 flag 消失
        while PAUSE_FLAG_FILE.exists() and not self.stop_event.is_set():
            if not self.check_pause_signal():
                break
            time.sleep(0.5)

        if self.paused:
            print("[RESUME] 决策工具已释放串口，恢复监控...")
            self.paused = False

    def connect(self):
        """连接串口（自动重试）"""
        try:
            if self.ser and self.ser.is_open:
                self.ser.close()
            self.ser = serial.Serial(self.port, 115200, timeout=0.5)
            print(f"[OK] 已连接到 {self.port}")
            self.reconnect_attempts = 0
            return True
        except Exception as e:
            self.reconnect_attempts += 1
            print(f"[!] 无法打开 {self.port} (尝试 {self.reconnect_attempts}): {e}")
            return False

    def send_frame(self, payload, name):
        """发送帧（失败时标记需要重连）"""
        if not self.ser or not self.ser.is_open:
            return False
        try:
            self.ser.write(wrap_frame(payload))
            self.ser.flush()
            print(f"[TX] {name} 帧已发送")
            return True
        except Exception as e:
            print(f"[!] 发送失败 ({name}): {e}")
            self.ser = None  # 触发重连
            return False

    def wait_for_decision_reply(self, decision_id, timeout=30):
        """等待 AI BOX 决策回执（0x0B）
        返回: chosen_index (0-3) 或 None（超时/取消）
        """
        start = time.time()
        buffer = bytearray()

        while time.time() - start < timeout:
            if not self.ser or not self.ser.is_open:
                return None

            if self.ser.in_waiting > 0:
                buffer.extend(self.ser.read(self.ser.in_waiting))

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

    def check_decision_queue(self):
        """检查决策队列，如有新请求则处理并发送
        队列文件格式: {"id": 123, "title": "...", "options": ["A", "B", "C"]}
        结果文件格式: {"id": 123, "result": 0}  或  {"id": 123, "result": null}
        """
        if not self.decision_queue_file.exists():
            return

        try:
            # 读取请求
            with open(self.decision_queue_file, 'r', encoding='utf-8') as f:
                req = json.load(f)

            decision_id = req.get('id', 0)
            title = req.get('title', 'Question')
            options = req.get('options', [])

            if not options or len(options) > 4:
                print(f"[!] 决策请求格式错误: 选项数={len(options)}")
                return

            # 删除队列文件（避免重复处理）
            self.decision_queue_file.unlink()

            print(f"\n[决策请求] ID={decision_id} 标题='{title}'")
            for i, opt in enumerate(options):
                print(f"  [{i}] {opt}")
            print("等待 AI BOX 触摸选择...")

            # 清空接收缓冲区（避免状态帧残留干扰回执解析）
            if self.ser and self.ser.is_open:
                self.ser.reset_input_buffer()

            # 发送决策帧
            frame = build_decision_frame(decision_id, title, options)
            wrapped = wrap_frame(frame)
            print(f"[调试] 决策帧串口字节数={len(wrapped)} payload={len(frame)} 前8字节={wrapped[:8].hex()}")
            if not self.send_frame(frame, "Decision"):
                # 发送失败，写回 null 结果
                with open(self.decision_result_file, 'w', encoding='utf-8') as f:
                    json.dump({"id": decision_id, "result": None, "error": "发送失败"}, f)
                return

            # 等待回执
            result = self.wait_for_decision_reply(decision_id, timeout=30)

            # 写结果文件
            if result is not None:
                print(f"[OK] 用户选择: [{result}] {options[result]}\n")
                with open(self.decision_result_file, 'w', encoding='utf-8') as f:
                    json.dump({"id": decision_id, "result": result}, f)
            else:
                print(f"[TIMEOUT] 超时或用户取消\n")
                with open(self.decision_result_file, 'w', encoding='utf-8') as f:
                    json.dump({"id": decision_id, "result": None, "error": "超时或取消"}, f)

        except Exception as e:
            print(f"[!] 决策处理异常: {e}")


    def update_cycle(self):
        """单次更新循环（带错误处理）"""
        # 检查连接状态
        if not self.ser or not self.ser.is_open:
            print("[!] 串口未连接，尝试重连...")
            if not self.connect():
                time.sleep(2)
                return

        # 1. Token 用量
        try:
            token_data = get_token_usage()
            self.send_frame(build_token_frame(token_data), "Token")
            # PC端显示: 2位小数百分比
            used, total = token_data[0][1], token_data[0][2]
            pct = (used / total * 100) if total > 0 else 0
            print(f"    Token: {used}/{total} ({pct:.2f}%)")
        except Exception as e:
            print(f"[!] Token 数据提取失败: {e}")

        time.sleep(0.5)

        # 2. 项目状态
        try:
            if self.auto_detect:
                # 多窗口模式: 自动检测所有活跃会话的仓库
                project_data = get_git_projects_from_sessions()
            else:
                # 单仓库模式: 使用指定路径
                project_data = get_git_projects(self.repo_path)
            self.send_frame(build_project_frame(project_data), "Project")
            for name, code in project_data:
                print(f"    {name} -> status={code}")
        except Exception as e:
            print(f"[!] Git 数据提取失败: {e}")

        time.sleep(0.5)

        # 3. AI 情绪
        try:
            emotion, text = get_ai_emotion()
            self.send_frame(build_ai_frame(emotion, text), "AI")
            print(f"    AI: emotion={emotion} text='{text}'")
        except Exception as e:
            print(f"[!] AI 状态提取失败: {e}")

    def run(self):
        """监控主循环（决策队列每秒检查，状态按 interval 更新）"""
        if not self.connect():
            return

        # 写入自己的 PID 文件
        try:
            MONITOR_PID_FILE.write_text(str(self.my_pid))
        except Exception as e:
            print(f"[!] 无法写入 PID 文件: {e}")

        mode = "多窗口自动检测" if self.auto_detect else f"单仓库 ({self.repo_path})"
        print(f"\n[监控启动] 模式: {mode}, 状态每 {self.interval} 秒更新，决策队列每秒检查")
        print(f"[监控启动] PID={self.my_pid}, 运行时目录: {RUNTIME_DIR}")
        print("按 Ctrl+C 停止\n")

        last_status_update = 0  # 上次状态更新时间

        try:
            while not self.stop_event.is_set():
                # === 1. 检查暂停信号（最高优先级）===
                if self.check_pause_signal():
                    self.wait_while_paused()
                    # 恢复后重连串口
                    if not self.ser or not self.ser.is_open:
                        self.connect()
                    continue

                current_time = time.time()

                # === 2. 每秒检查决策队列 ===
                self.check_decision_queue()

                # === 3. 按 interval 间隔发送状态帧 ===
                if current_time - last_status_update >= self.interval:
                    print(f"[{time.strftime('%H:%M:%S')}] 更新状态...")
                    self.update_cycle()
                    print()
                    last_status_update = current_time

                # 等待 1 秒（下次检查决策）
                self.stop_event.wait(1)

        except KeyboardInterrupt:
            print("\n[停止] 用户中断")
        finally:
            if self.ser:
                self.ser.close()
            # 清理 PID 文件
            try:
                if MONITOR_PID_FILE.exists():
                    MONITOR_PID_FILE.unlink()
            except:
                pass
            print("[OK] 串口已关闭，运行时文件已清理")

    def stop(self):
        self.stop_event.set()

# ============================================================
# 主程序
# ============================================================
if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("用法: python realtime_monitor.py <COM端口> [仓库路径|--auto] [更新间隔秒数]")
        print("示例:")
        print("  单仓库模式: python realtime_monitor.py COM6 . 5")
        print("  多窗口模式: python realtime_monitor.py COM6 --auto 5")
        sys.exit(1)

    port = sys.argv[1]

    # 检查是否启用自动检测模式
    if len(sys.argv) > 2 and sys.argv[2] == '--auto':
        auto_detect = True
        repo_path = None
        interval = int(sys.argv[3]) if len(sys.argv) > 3 else 5
    else:
        auto_detect = False
        repo_path = sys.argv[2] if len(sys.argv) > 2 else os.getcwd()
        interval = int(sys.argv[3]) if len(sys.argv) > 3 else 5

    if auto_detect:
        print(f"[配置] 串口={port}, 模式=多窗口自动检测, 间隔={interval}秒")
    else:
        print(f"[配置] 串口={port}, 仓库={repo_path}, 间隔={interval}秒")

    monitor = RealtimeMonitor(port, repo_path, interval, auto_detect)
    monitor.run()

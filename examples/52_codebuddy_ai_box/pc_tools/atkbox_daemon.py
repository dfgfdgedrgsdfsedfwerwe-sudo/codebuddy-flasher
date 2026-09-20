#!/usr/bin/env python3
"""ATK BOX Daemon - Claude Code Hook 集成守护进程

职责：
- 独占 Dongle 串口（COM6）
- 提供 TCP IPC 服务器（127.0.0.1:47100）
- 处理状态推送（0x0C 帧，带节流）
- 处理审批请求（0x0D/0x0E 帧，阻塞等待）
- 断线重连（5 秒冷却）
"""
import sys
import json
import socket
import logging
import threading
import time
from pathlib import Path
from typing import Optional, Dict, List
from datetime import datetime

from serial_manager import SerialManager
from protocol_frames import (
    build_status_frame, build_approval_frame, parse_approval_reply,
    build_token_frame, build_project_frame,
    build_decision_frame, parse_decision_reply
)

# 配置日志
logging.basicConfig(
    level=logging.INFO,
    format='[%(asctime)s] [%(levelname)s] %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S'
)
logger = logging.getLogger(__name__)

# 状态字符串到状态码的映射
STATE_MAP = {
    "READY": 0x00,
    "THINKING": 0x01,
    "RUNNING": 0x02,
    "DONE": 0x03,
    "ERROR": 0x04,
}

# 状态节流时间（秒）
THROTTLE_WINDOW = 1.0

# Token/Project 推送间隔（秒）
PUSH_INTERVAL = 30

# Token 固定配额映射（tokens/天）
TOKEN_QUOTAS = {
    "opus": 5_000_000,
    "sonnet": 10_000_000,
    "haiku": 20_000_000,
    "default": 50_000_000,
}

# Project 状态码（对齐固件 project_status_code_t）
PROJ_STATUS_PLANNING = 0x00
PROJ_STATUS_CODING = 0x01
PROJ_STATUS_REVIEW = 0x02
PROJ_STATUS_COMPLETED = 0x03
PROJ_STATUS_ERROR = 0x04
PROJ_STATUS_IDLE = 0x05


class DaemonServer:
    """Daemon 服务器 - 独占串口 + IPC 服务"""

    def __init__(self, ipc_port: int = 47100, serial_port: str = "COM6", push_interval: int = 30):
        """初始化 Daemon

        Args:
            ipc_port: TCP IPC 监听端口
            serial_port: Dongle 串口设备名
            push_interval: Token/Project 推送间隔（秒）
        """
        self.ipc_port = ipc_port
        self.serial_port = serial_port
        self.push_interval = push_interval

        # 串口管理器
        self.serial_mgr = SerialManager(port=serial_port, auto_reconnect=True)

        # IPC 服务器
        self.server_socket: Optional[socket.socket] = None
        self.running = False

        # 状态节流缓存
        self._status_cache: Dict[str, float] = {}  # key: "state:tool", value: timestamp

        # 序列号计数器（0-255 循环）
        self._seq_num = 0

        # 任务 ID 计数器（0-65535 循环）
        self._task_id = 0

        # 决策 ID 计数器（0-65535 循环）
        self._decision_id = 1

        # 决策交互状态（供 GUI 显示 + 输入回填）
        # GUI 轮询 get_pending_decision() 显示面板，用户点击后调 set_decision_choice()
        self._pending_decision_id: Optional[int] = None
        self._pending_decision_title: str = ""
        self._pending_decision_options: List[str] = []
        self._gui_choice: Optional[int] = None  # None=未选择，0-3=GUI已选择
        self._decision_lock = threading.Lock()

        # 后台推送线程
        self._pusher_thread: Optional[threading.Thread] = None

        # 统计计数器（线程安全）
        self._stats_lock = threading.Lock()
        self.stats = {
            "frames_sent": 0,
            "approvals_requested": 0,
            "approvals_allowed": 0,
            "approvals_rejected": 0,
            "current_agent_state": "READY",
            "last_task_message": "",
        }

        logger.info(f"DaemonServer initialized (IPC: {ipc_port}, Serial: {serial_port}, PushInterval: {push_interval}s)")

    def start(self):
        """启动 Daemon 服务器"""
        if self.running:
            logger.warning("Daemon already running")
            return

        self.running = True

        # 启动后台推送线程
        self._pusher_thread = threading.Thread(target=self._background_pusher, daemon=True)
        self._pusher_thread.start()
        logger.info("Background pusher thread started")

        # 启动 TCP 服务器
        try:
            self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.server_socket.bind(("127.0.0.1", self.ipc_port))
            self.server_socket.listen(5)
            self.server_socket.settimeout(1.0)  # 1 秒超时，避免阻塞 stop()

            logger.info(f"IPC server listening on 127.0.0.1:{self.ipc_port}")

            # 主循环：接受连接
            while self.running:
                try:
                    client_sock, client_addr = self.server_socket.accept()
                    # 每个连接在独立线程处理
                    threading.Thread(
                        target=self._handle_client,
                        args=(client_sock,),
                        daemon=True
                    ).start()
                except socket.timeout:
                    continue
                except Exception as e:
                    if self.running:
                        logger.error(f"Accept connection error: {e}")

        except Exception as e:
            logger.error(f"Failed to start IPC server: {e}")
            self.running = False

    def stop(self):
        """停止 Daemon 服务器"""
        logger.info("Stopping daemon...")
        self.running = False

        if self.server_socket:
            try:
                self.server_socket.close()
            except:
                pass
            self.server_socket = None

        # 关闭串口
        self.serial_mgr.close()

        logger.info("Daemon stopped")

    def is_running(self) -> bool:
        """检查 Daemon 是否运行"""
        return self.running

    def _handle_client(self, client_sock: socket.socket):
        """处理单个 IPC 客户端连接

        Args:
            client_sock: 客户端 socket
        """
        try:
            # 读取 JSON 请求（单行）
            data = b""
            while b"\n" not in data:
                chunk = client_sock.recv(1024)
                if not chunk:
                    break
                data += chunk

            if not data:
                return

            request = json.loads(data.decode('utf-8').strip())
            request_type = request.get("type")

            # 路由到处理函数
            if request_type == "status":
                response = self._handle_status(request)
            elif request_type == "approval":
                response = self._handle_approval(request)
            elif request_type == "decision":
                response = self._handle_decision(request)
            else:
                response = {"ok": False, "error": f"Unknown request type: {request_type}"}

            # 发送响应
            client_sock.sendall((json.dumps(response) + "\n").encode('utf-8'))

        except Exception as e:
            logger.error(f"Handle client error: {e}")
            try:
                error_resp = {"ok": False, "error": str(e)}
                client_sock.sendall((json.dumps(error_resp) + "\n").encode('utf-8'))
            except:
                pass
        finally:
            try:
                client_sock.close()
            except:
                pass

    def _handle_status(self, req: dict) -> dict:
        """处理状态推送请求（非阻塞，带节流）

        Args:
            req: {"type": "status", "state": "THINKING", "tool": "Read", "detail": "config.py"}

        Returns:
            {"ok": True} 或 {"ok": False, "error": "..."}
        """
        state_str = req.get("state", "READY")
        tool = req.get("tool", "")
        detail = req.get("detail", "")

        # 映射状态字符串到状态码
        state_code = STATE_MAP.get(state_str.upper(), 0x01)

        # 构造节流键
        throttle_key = f"{state_code}:{tool}"

        # 检查节流
        now = time.time()
        if throttle_key in self._status_cache:
            last_time = self._status_cache[throttle_key]
            if now - last_time < THROTTLE_WINDOW:
                logger.debug(f"Status throttled: {state_str} + {tool}")
                return {"ok": True}  # 节流，但返回成功

        # 更新节流缓存
        self._status_cache[throttle_key] = now

        # 构造任务消息
        if tool:
            task_message = f"{tool}: {detail}"[:48]
        else:
            task_message = detail[:48]

        # 构造并发送 0x0C 帧
        payload = build_status_frame(state_code, self._seq_num, task_message)
        self._seq_num = (self._seq_num + 1) % 256

        if not self.serial_mgr.send_frame(payload):
            return {"ok": False, "error": "Serial send failed"}

        # 更新统计
        with self._stats_lock:
            self.stats["frames_sent"] += 1
            self.stats["current_agent_state"] = state_str.upper()
            self.stats["last_task_message"] = task_message

        logger.info(f"Status sent: {state_str}, tool={tool}, seq={self._seq_num-1}")
        return {"ok": True}

    def _handle_approval(self, req: dict) -> dict:
        """处理审批请求（阻塞，等待 0x0E 回执）

        Args:
            req: {"type": "approval", "tool": "Write", "file": "main.py", "preview": "..."}

        Returns:
            {"ok": True, "decision": "allow"|"deny"} 或 {"ok": False, "error": "..."}
        """
        tool = req.get("tool", "Write")
        file = req.get("file", "unknown")
        preview = req.get("preview", "")[:200]

        # 推断风险等级
        if "/test/" in file or ".test." in file:
            risk = 0x00  # LOW
        elif "/config/" in file or "settings" in file.lower():
            risk = 0x01  # MEDIUM
        else:
            risk = 0x02  # HIGH

        # 分配 task_id
        task_id = self._task_id
        self._task_id = (self._task_id + 1) % 65536

        # 构造 0x0D 帧
        title = f"{tool} to {file.split('/')[-1]}"[:32]
        target = file[-32:] if len(file) > 32 else file
        diff = preview[:40]
        payload = build_approval_frame(task_id, risk, title, target, diff)

        if not self.serial_mgr.send_frame(payload):
            return {"ok": False, "error": "Serial send failed"}

        logger.info(f"Approval sent: task_id={task_id}, file={file}")
        with self._stats_lock:
            self.stats["approvals_requested"] += 1
            self.stats["frames_sent"] += 1

        # 阻塞轮询等待 0x0E
        max_polls = 10000  # 理论无限，实际设个大值防挂死
        for _ in range(max_polls):
            reply_payload = self.serial_mgr.read_reply(timeout_ms=250)
            if reply_payload:
                parsed = parse_approval_reply(reply_payload)
                if parsed and parsed["task_id"] == task_id:
                    action = parsed["action"]
                    if action == 0x00:
                        logger.info(f"Approval allowed: task_id={task_id}")
                        with self._stats_lock:
                            self.stats["approvals_allowed"] += 1
                        return {"ok": True, "decision": "allow"}
                    elif action == 0x01:
                        logger.info(f"Approval rejected: task_id={task_id}")
                        with self._stats_lock:
                            self.stats["approvals_rejected"] += 1
                        return {"ok": True, "decision": "deny"}
                    elif action == 0x02:
                        logger.info("User requested diff, continue waiting")
                        continue  # 继续等待下一个 0x0E
                    else:
                        # 0xFF 或其他：视为拒绝
                        logger.warning(f"Approval timeout/unknown action: {action:#x}")
                        return {"ok": True, "decision": "deny"}

            # 检查串口断线
            if not self.serial_mgr.is_connected():
                logger.error("Serial disconnected during approval")
                return {"ok": False, "error": "Serial disconnected"}

        logger.error(f"Approval polling limit exceeded: task_id={task_id}")
        return {"ok": False, "error": "Polling limit exceeded"}

    def _handle_decision(self, req: dict) -> dict:
        """处理决策请求（阻塞，等待 0x0B 回执或键盘输入）

        Args:
            req: {"type": "decision", "title": "Question?", "options": ["A", "B", "C"]}

        Returns:
            {"ok": True, "chosen": 0-3, "source": "box"|"keyboard"} 或 {"ok": False, "error": "..."}
            chosen=255 表示用户取消或超时
        """
        title = req.get("title", "Choose:")
        options = req.get("options", [])

        if not options or len(options) > 4:
            return {"ok": False, "error": "Options must be 1-4 items"}

        # 生成决策 ID
        decision_id = self._decision_id
        self._decision_id = (self._decision_id + 1) % 65536

        # 登记待处理决策（供 GUI 显示面板 + 回填选择）
        with self._decision_lock:
            self._pending_decision_id = decision_id
            self._pending_decision_title = title
            self._pending_decision_options = list(options)
            self._gui_choice = None

        # 构造并发送 0x0A 决策帧
        payload = build_decision_frame(decision_id, title, options, kind=1)
        if not self.serial_mgr.send_frame(payload):
            self._clear_pending_decision()
            return {"ok": False, "error": "Serial send failed"}

        logger.info(f"Decision sent: id={decision_id}, title={title}, opts={len(options)}")
        logger.info(f"Waiting for choice: BOX touch OR PC input (1-{len(options)})")
        with self._stats_lock:
            self.stats["frames_sent"] += 1

        # 阻塞轮询等待 0x0B 回执（BOX 触摸）或 GUI 输入（PC 端）
        max_polls = 160  # 160 * 250ms = 40s
        chosen = None
        source = None

        for _ in range(max_polls):
            # 检查 GUI 输入（用户在面板点击或输入数字）
            with self._decision_lock:
                if self._gui_choice is not None:
                    chosen = self._gui_choice
                    source = "pc"
                    logger.info(f"Decision from PC input: chosen={chosen}")
                    break

            # 检查串口回执（BOX 触摸）
            reply_payload = self.serial_mgr.read_reply(timeout_ms=250)
            if reply_payload:
                parsed = parse_decision_reply(reply_payload)
                if parsed and parsed["decision_id"] == decision_id:
                    chosen = parsed["chosen_index"]
                    source = "box"
                    logger.info(f"Decision from BOX: chosen={chosen}")
                    break

            # 检查串口断线
            if not self.serial_mgr.is_connected():
                logger.error("Serial disconnected during decision")
                self._clear_pending_decision()
                return {"ok": False, "error": "Serial disconnected"}

        # 清除待处理决策
        self._clear_pending_decision()

        if chosen is not None:
            return {"ok": True, "chosen": chosen, "source": source}
        else:
            logger.error(f"Decision polling limit exceeded: id={decision_id}")
            return {"ok": False, "error": "Polling limit exceeded"}

    # ---------- GUI 决策交互接口 ----------
    def get_pending_decision(self) -> Optional[dict]:
        """获取当前待处理决策（供 GUI 轮询显示面板）

        Returns:
            {"id": int, "title": str, "options": [str]} 或 None（无待处理）
        """
        with self._decision_lock:
            if self._pending_decision_id is None:
                return None
            return {
                "id": self._pending_decision_id,
                "title": self._pending_decision_title,
                "options": list(self._pending_decision_options),
            }

    def set_decision_choice(self, index: int) -> bool:
        """GUI 回填用户选择（点击选项或输入数字后调用）

        Args:
            index: 选项索引 (0-3)

        Returns:
            True=成功登记，False=无待处理决策或索引越界
        """
        with self._decision_lock:
            if self._pending_decision_id is None:
                return False
            if not (0 <= index < len(self._pending_decision_options)):
                return False
            self._gui_choice = index
            logger.info(f"GUI choice set: [{index+1}] {self._pending_decision_options[index]}")
            return True

    def match_decision_text(self, text: str) -> Optional[int]:
        """将文本输入匹配为选项索引（数字 1-4 或选项文本模糊匹配）

        Args:
            text: 用户输入文本

        Returns:
            匹配的索引 (0-3) 或 None（无匹配/歧义）
        """
        text = text.strip().lower()
        if not text:
            return None

        with self._decision_lock:
            options = list(self._pending_decision_options)

        if not options:
            return None

        # 数字 1-4
        if text in ['1', '2', '3', '4']:
            idx = int(text) - 1
            return idx if 0 <= idx < len(options) else None

        # 文本匹配（全文或前缀，需唯一）
        matches = [i for i, opt in enumerate(options)
                   if text == opt.lower() or opt.lower().startswith(text)]
        return matches[0] if len(matches) == 1 else None

    def _clear_pending_decision(self):
        """清除待处理决策状态"""
        with self._decision_lock:
            self._pending_decision_id = None
            self._pending_decision_title = ""
            self._pending_decision_options = []
            self._gui_choice = None

    def _read_token_stats(self) -> List[dict]:
        """读取 Claude Code token 统计数据

        从 ~/.claude/stats-cache.json 读取 modelUsage，
        取 outputTokens 最高的前 5 个模型。

        Returns:
            Token 项列表，每项 {"name": str, "used": int, "total": int, "percent_x10": int}
        """
        try:
            stats_file = Path.home() / ".claude" / "stats-cache.json"
            if not stats_file.exists():
                logger.warning(f"Stats file not found: {stats_file}")
                return []

            with open(stats_file, 'r', encoding='utf-8') as f:
                data = json.load(f)

            model_usage = data.get("modelUsage", {})
            if not model_usage:
                return []

            # 转换为列表并按 output tokens 排序
            items = []
            for model_name, usage in model_usage.items():
                used = usage.get("inputTokens", 0) + usage.get("outputTokens", 0)

                # 配额 = 实际用量的 1.5 倍（让进度条有意义）
                # 这样显示的是"已用/预期最大值"的比例
                total = max(int(used * 1.5), 1000000)  # 至少 100 万

                percent_x10 = min(int((used / total) * 1000), 1000) if total > 0 else 0

                items.append({
                    "name": model_name[:19],  # 截断到 TOKEN_NAME_LEN-1
                    "used": used,
                    "total": total,
                    "percent_x10": percent_x10,
                    "output": usage.get("outputTokens", 0)  # 用于排序
                })

            # 按 output tokens 降序，取前 5
            items.sort(key=lambda x: x["output"], reverse=True)
            return items[:5]

        except Exception as e:
            logger.error(f"Failed to read token stats: {e}")
            return []

    def _scan_projects(self) -> List[dict]:
        """扫描 Claude Code 项目目录

        从 ~/.claude/projects/ 扫描项目，按最近活动排序，取前 6 个。
        状态根据最后修改时间推断。

        Returns:
            Project 项列表，每项 {"name": str, "status_code": int}
        """
        try:
            projects_dir = Path.home() / ".claude" / "projects"
            if not projects_dir.exists():
                logger.warning(f"Projects dir not found: {projects_dir}")
                return []

            # 扫描所有子目录
            project_dirs = [d for d in projects_dir.iterdir() if d.is_dir()]
            if not project_dirs:
                return []

            # 按修改时间排序
            project_dirs.sort(key=lambda d: d.stat().st_mtime, reverse=True)

            items = []
            now = time.time()
            for proj_dir in project_dirs[:6]:  # 取前 6 个
                # 清理项目名（去掉长前缀 hash）
                name = proj_dir.name
                if name.startswith("C--"):
                    # C--Users-4090-Desktop-xxx → xxx
                    parts = name.split("-")
                    if len(parts) > 3:
                        name = "-".join(parts[3:])

                # 推断状态
                mtime = proj_dir.stat().st_mtime
                age_hours = (now - mtime) / 3600

                if age_hours < 1:
                    status_code = PROJ_STATUS_CODING  # 最近 1 小时 → Coding
                elif age_hours < 24:
                    status_code = PROJ_STATUS_REVIEW  # 最近 24 小时 → Review
                else:
                    status_code = PROJ_STATUS_IDLE    # 更早 → Idle

                items.append({
                    "name": name[:23],  # 截断到 PROJECT_NAME_LEN-1
                    "status_code": status_code
                })

            return items

        except Exception as e:
            logger.error(f"Failed to scan projects: {e}")
            return []

    def _push_token_status(self):
        """推送 Token 状态到 ATK BOX"""
        try:
            items = self._read_token_stats()
            if not items:
                logger.debug("No token data to push")
                return

            frame = build_token_frame(items, self._seq_num)
            self._seq_num = (self._seq_num + 1) % 256

            if self.serial_mgr.send_frame(frame):
                logger.info(f"Token status sent: {len(items)} items, seq={self._seq_num-1}")
                with self._stats_lock:
                    self.stats["frames_sent"] += 1
            else:
                logger.warning("Failed to send token status")

        except Exception as e:
            logger.error(f"Push token status error: {e}")

    def _push_project_status(self):
        """推送 Project 状态到 ATK BOX"""
        try:
            items = self._scan_projects()
            if not items:
                logger.debug("No project data to push")
                return

            frame = build_project_frame(items, self._seq_num)
            self._seq_num = (self._seq_num + 1) % 256

            if self.serial_mgr.send_frame(frame):
                logger.info(f"Project status sent: {len(items)} items, seq={self._seq_num-1}")
                with self._stats_lock:
                    self.stats["frames_sent"] += 1
            else:
                logger.warning("Failed to send project status")

        except Exception as e:
            logger.error(f"Push project status error: {e}")

    def _background_pusher(self):
        """后台推送线程：每 push_interval 秒推送 Token/Project 数据"""
        logger.info(f"Background pusher started (interval: {self.push_interval}s)")

        # 启动时立即推送一次
        time.sleep(2)  # 等待串口稳定
        self._push_token_status()
        self._push_project_status()

        # 定期推送（拆成 1 秒粒度检查 running，加快停止响应）
        elapsed = 0
        while self.running:
            time.sleep(1)
            elapsed += 1
            if not self.running:
                break
            if elapsed >= self.push_interval:
                elapsed = 0
                self._push_token_status()
                self._push_project_status()

        logger.info("Background pusher stopped")

    def get_stats(self) -> dict:
        """获取统计信息（线程安全）"""
        with self._stats_lock:
            return dict(self.stats)


if __name__ == "__main__":
    import argparse
    import signal
    from pathlib import Path

    parser = argparse.ArgumentParser(description="ATK BOX Daemon for Claude Code Hooks")
    parser.add_argument("--port", default="COM6", help="Serial port (default: COM6)")
    parser.add_argument("--ipc-port", type=int, default=47100, help="IPC TCP port")
    parser.add_argument("--log-level", default="INFO", choices=["DEBUG", "INFO", "WARNING", "ERROR"])
    args = parser.parse_args()

    # 配置日志到文件
    log_path = Path.home() / ".claude" / "atkbox_daemon.log"
    log_path.parent.mkdir(parents=True, exist_ok=True)
    file_handler = logging.FileHandler(log_path, encoding='utf-8')
    file_handler.setFormatter(logging.Formatter('[%(asctime)s] [%(levelname)s] %(message)s'))
    logger.addHandler(file_handler)
    logger.setLevel(getattr(logging, args.log_level))

    daemon = DaemonServer(ipc_port=args.ipc_port, serial_port=args.port)

    # 信号处理（优雅关闭）
    def signal_handler(sig, frame):
        logger.info("Received shutdown signal")
        daemon.stop()
        sys.exit(0)

    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    daemon.start()
    logger.info(f"Daemon running (serial: {args.port}, IPC: {args.ipc_port})")
    logger.info(f"Log file: {log_path}")

    # 保持运行
    try:
        while daemon.is_running():
            time.sleep(1)
            daemon.serial_mgr.try_reconnect()  # 定期检查重连
    except KeyboardInterrupt:
        pass

    daemon.stop()

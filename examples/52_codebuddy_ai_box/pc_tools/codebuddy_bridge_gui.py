#!/usr/bin/env python3
"""CodeBuddy Bridge - ATK BOX 守护进程 GUI 应用

提供友好的图形界面管理 CodeBuddy Bridge 守护进程：
- 配置串口、IPC端口、推送间隔
- 实时显示连接状态、Agent状态、统计信息
- 查看实时日志
- 安装/卸载 Claude Code Hooks
- 开机自启动
"""
import sys
import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox
import threading
import logging
import queue
import time
from pathlib import Path

# 导入本地模块
from config_manager import ConfigManager
from atkbox_daemon import DaemonServer
import serial.tools.list_ports


# 配置日志
logging.basicConfig(
    level=logging.INFO,
    format='[%(asctime)s] [%(levelname)s] %(message)s',
    datefmt='%H:%M:%S'
)
logger = logging.getLogger(__name__)


class LogQueueHandler(logging.Handler):
    """自定义日志处理器，将日志推送到队列供 GUI 消费"""
    def __init__(self, log_queue):
        super().__init__()
        self.log_queue = log_queue

    def emit(self, record):
        msg = self.format(record)
        self.log_queue.put(msg)


class CodeBuddyBridgeGUI:
    """CodeBuddy Bridge GUI 主窗口"""

    def __init__(self):
        self.root = tk.Tk()
        self.root.title("CodeBuddy Bridge v1.0.0")
        self.root.geometry("900x700")
        self.root.resizable(True, True)

        # 配置管理器
        self.config = ConfigManager()

        # Daemon 后台线程
        self.daemon = None
        self.daemon_thread = None
        self.daemon_running = False

        # 日志队列
        self.log_queue = queue.Queue()
        self.log_handler = LogQueueHandler(self.log_queue)
        self.log_handler.setFormatter(logging.Formatter('[%(asctime)s] %(message)s', datefmt='%H:%M:%S'))
        logger.addHandler(self.log_handler)

        # 构建 UI
        self._build_ui()

        # 启动定时任务
        self._start_update_loop()

        # 窗口关闭事件
        self.root.protocol("WM_DELETE_WINDOW", self._on_closing)

    def _build_ui(self):
        """构建主界面"""
        # === 顶部：设置面板 ===
        settings_frame = ttk.LabelFrame(self.root, text="设置", padding=10)
        settings_frame.pack(fill=tk.X, padx=10, pady=5)

        # 串口选择
        ttk.Label(settings_frame, text="串口:").grid(row=0, column=0, sticky=tk.W, padx=5, pady=5)
        self.serial_port_var = tk.StringVar(value=self.config.get("serial_port"))
        self.serial_port_combo = ttk.Combobox(settings_frame, textvariable=self.serial_port_var, width=15, state="readonly")
        self.serial_port_combo.grid(row=0, column=1, padx=5, pady=5)
        ttk.Button(settings_frame, text="扫描", command=self._scan_serial_ports).grid(row=0, column=2, padx=5, pady=5)

        # IPC 端口
        ttk.Label(settings_frame, text="IPC端口:").grid(row=0, column=3, sticky=tk.W, padx=(20,5), pady=5)
        self.ipc_port_var = tk.StringVar(value=str(self.config.get("ipc_port")))
        ttk.Entry(settings_frame, textvariable=self.ipc_port_var, width=10).grid(row=0, column=4, padx=5, pady=5)

        # 推送间隔
        ttk.Label(settings_frame, text="推送间隔:").grid(row=1, column=0, sticky=tk.W, padx=5, pady=5)
        self.push_interval_var = tk.StringVar(value=str(self.config.get("push_interval")))
        ttk.Entry(settings_frame, textvariable=self.push_interval_var, width=10).grid(row=1, column=1, padx=5, pady=5)
        ttk.Label(settings_frame, text="秒").grid(row=1, column=2, sticky=tk.W, pady=5)

        # 开机自启（占位）
        self.auto_start_var = tk.BooleanVar(value=self.config.get("auto_start"))
        ttk.Checkbutton(settings_frame, text="开机自启动", variable=self.auto_start_var).grid(row=1, column=3, columnspan=2, sticky=tk.W, padx=(20,5), pady=5)

        # 按钮行
        btn_frame = ttk.Frame(settings_frame)
        btn_frame.grid(row=2, column=0, columnspan=5, pady=(10,0))
        ttk.Button(btn_frame, text="应用设置", command=self._apply_settings).pack(side=tk.LEFT, padx=5)

        # 第一行按钮
        ttk.Button(btn_frame, text="安装 Hooks", command=self._install_hooks).pack(side=tk.LEFT, padx=5)
        ttk.Button(btn_frame, text="卸载 Hooks", command=self._uninstall_hooks).pack(side=tk.LEFT, padx=5)

        # 第二行按钮
        btn_frame2 = ttk.Frame(settings_frame)
        btn_frame2.grid(row=3, column=0, columnspan=5, pady=(5,0))
        ttk.Button(btn_frame2, text="安装 MCP", command=self._install_mcp).pack(side=tk.LEFT, padx=5)
        ttk.Button(btn_frame2, text="卸载 MCP", command=self._uninstall_mcp).pack(side=tk.LEFT, padx=5)
        ttk.Button(btn_frame2, text="安装全局指令", command=self._install_claude_md).pack(side=tk.LEFT, padx=5)
        ttk.Button(btn_frame2, text="卸载全局指令", command=self._uninstall_claude_md).pack(side=tk.LEFT, padx=5)

        # 初始化串口列表
        self._scan_serial_ports()

        # === 中部：状态面板 ===
        status_frame = ttk.LabelFrame(self.root, text="状态", padding=10)
        status_frame.pack(fill=tk.X, padx=10, pady=5)

        # 左列：连接状态
        left_frame = ttk.Frame(status_frame)
        left_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        self.serial_status_label = ttk.Label(left_frame, text="串口: ● 未连接", foreground="red")
        self.serial_status_label.pack(anchor=tk.W)

        self.ipc_status_label = ttk.Label(left_frame, text="IPC: ● 未监听", foreground="red")
        self.ipc_status_label.pack(anchor=tk.W)

        self.agent_status_label = ttk.Label(left_frame, text="Agent: READY")
        self.agent_status_label.pack(anchor=tk.W)

        # 右列：统计信息
        right_frame = ttk.Frame(status_frame)
        right_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        self.frames_sent_label = ttk.Label(right_frame, text="已推送: 0 帧")
        self.frames_sent_label.pack(anchor=tk.W)

        self.approvals_label = ttk.Label(right_frame, text="审批: 0 次 (批准: 0 | 拒绝: 0)")
        self.approvals_label.pack(anchor=tk.W)

        # === 决策交互面板 ===
        self.decision_frame = ttk.LabelFrame(self.root, text="决策交互", padding=10)
        self.decision_frame.pack(fill=tk.X, padx=10, pady=5)

        # 问题标题
        self.decision_title_label = ttk.Label(
            self.decision_frame,
            text="（无待处理决策）",
            font=("Microsoft YaHei", 11, "bold"),
            foreground="gray"
        )
        self.decision_title_label.pack(anchor=tk.W, pady=(0, 8))

        # 选项按钮容器
        self.decision_options_frame = ttk.Frame(self.decision_frame)
        self.decision_options_frame.pack(fill=tk.X)
        self.decision_option_buttons = []  # 动态创建的选项按钮

        # 文本输入行
        input_row = ttk.Frame(self.decision_frame)
        input_row.pack(fill=tk.X, pady=(8, 0))
        ttk.Label(input_row, text="或输入数字/文本:").pack(side=tk.LEFT, padx=(0, 5))
        self.decision_input_var = tk.StringVar()
        self.decision_input_entry = ttk.Entry(input_row, textvariable=self.decision_input_var, width=25)
        self.decision_input_entry.pack(side=tk.LEFT, padx=5)
        self.decision_input_entry.bind("<Return>", lambda e: self._submit_decision_text())
        self.decision_submit_btn = ttk.Button(input_row, text="提交", command=self._submit_decision_text)
        self.decision_submit_btn.pack(side=tk.LEFT, padx=5)

        # 当前显示的决策 ID（用于检测新决策）
        self._current_decision_id = None

        # 初始隐藏输入控件
        self._set_decision_input_enabled(False)

        # === 日志面板 ===
        log_frame = ttk.LabelFrame(self.root, text="实时日志", padding=5)
        log_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)

        self.log_text = scrolledtext.ScrolledText(log_frame, height=20, state=tk.DISABLED, wrap=tk.WORD)
        self.log_text.pack(fill=tk.BOTH, expand=True)

        # === 底部：控制按钮 ===
        control_frame = ttk.Frame(self.root)
        control_frame.pack(fill=tk.X, padx=10, pady=5)

        self.start_button = ttk.Button(control_frame, text="启动服务", command=self._start_daemon, state=tk.NORMAL)
        self.start_button.pack(side=tk.LEFT, padx=5)

        self.stop_button = ttk.Button(control_frame, text="停止服务", command=self._stop_daemon, state=tk.DISABLED)
        self.stop_button.pack(side=tk.LEFT, padx=5)

        ttk.Button(control_frame, text="释放串口", command=self._release_serial_ports).pack(side=tk.LEFT, padx=5)

        ttk.Button(control_frame, text="清空日志", command=self._clear_log).pack(side=tk.LEFT, padx=5)

        ttk.Label(control_frame, text="v1.0.0").pack(side=tk.RIGHT, padx=5)

    def _scan_serial_ports(self):
        """扫描可用串口"""
        ports = [p.device for p in serial.tools.list_ports.comports()]
        self.serial_port_combo['values'] = ports if ports else ["无可用端口"]
        if ports and self.serial_port_var.get() not in ports:
            self.serial_port_var.set(ports[0])

    def _apply_settings(self):
        """应用设置"""
        try:
            # 验证 IPC 端口
            ipc_port = int(self.ipc_port_var.get())
            if not (1024 <= ipc_port <= 65535):
                raise ValueError("IPC 端口范围应在 1024-65535")

            # 验证推送间隔
            push_interval = int(self.push_interval_var.get())
            if not (5 <= push_interval <= 300):
                raise ValueError("推送间隔范围应在 5-300 秒")

            # 保存配置
            self.config.set("serial_port", self.serial_port_var.get())
            self.config.set("ipc_port", ipc_port)
            self.config.set("push_interval", push_interval)
            self.config.set("auto_start", self.auto_start_var.get())

            messagebox.showinfo("成功", "设置已保存。如服务正在运行，请重启服务使设置生效。")
            logger.info("Settings applied successfully")

        except ValueError as e:
            messagebox.showerror("错误", f"设置无效: {e}")

    def _install_hooks(self):
        """安装 Claude Code Hooks"""
        try:
            # 动态导入避免启动时依赖
            from install_hooks import install as do_install
            do_install()
            self.config.set("hooks_installed", True)
            messagebox.showinfo("成功", "Hooks 已安装到 Claude Code。请重启 Claude Code 使其生效。")
            logger.info("Hooks installed successfully")
        except Exception as e:
            messagebox.showerror("错误", f"安装 Hooks 失败:\n{e}")
            logger.error(f"Failed to install hooks: {e}")

    def _uninstall_hooks(self):
        """卸载 Claude Code Hooks"""
        try:
            from install_hooks import uninstall as do_uninstall
            do_uninstall()
            self.config.set("hooks_installed", False)
            messagebox.showinfo("成功", "Hooks 已从 Claude Code 卸载。")
            logger.info("Hooks uninstalled successfully")
        except Exception as e:
            messagebox.showerror("错误", f"卸载 Hooks 失败:\n{e}")
            logger.error(f"Failed to uninstall hooks: {e}")

    def _install_mcp(self):
        """安装 MCP Server 配置"""
        try:
            from install_mcp_atkbox import install as do_install_mcp
            do_install_mcp()
            self.config.set("mcp_installed", True)
            messagebox.showinfo("成功",
                "MCP Server (atkbox) 已安装到 ~/.claude.json。\n\n"
                "下一步:\n"
                "1. 完全退出 Claude Code CLI（exit 或关闭所有窗口）\n"
                "2. 重新进入项目目录并启动: claude\n"
                "3. 输入 /mcp 验证工具已加载\n"
                "4. 对话里说: 用 ask_on_atkbox 问我...")
            logger.info("MCP Server installed successfully")
        except Exception as e:
            messagebox.showerror("错误", f"安装 MCP 失败:\n{e}")
            logger.error(f"Failed to install MCP: {e}")

    def _uninstall_mcp(self):
        """卸载 MCP Server 配置"""
        try:
            from install_mcp_atkbox import uninstall as do_uninstall_mcp
            do_uninstall_mcp()
            self.config.set("mcp_installed", False)
            messagebox.showinfo("成功", "MCP Server (atkbox) 已从 ~/.claude.json 卸载。")
            logger.info("MCP Server uninstalled successfully")
        except Exception as e:
            messagebox.showerror("错误", f"卸载 MCP 失败:\n{e}")
            logger.error(f"Failed to uninstall MCP: {e}")

    def _install_claude_md(self):
        """安装全局 CLAUDE.md 指令"""
        try:
            from install_claude_md import install as do_install_claude_md
            do_install_claude_md()
            self.config.set("claude_md_installed", True)
            messagebox.showinfo("成功",
                "全局 CLAUDE.md 指令已安装到 ~/.claude/CLAUDE.md。\n\n"
                "功能: 让 Claude 在遇到 2-4 选项的多选决策时，\n"
                "自动同步显示到 ATK BOX 硬件。\n\n"
                "下一步:\n"
                "1. 完全退出 Claude Code CLI（exit 或关闭所有窗口）\n"
                "2. 重新启动: claude\n"
                "3. 在对话中问一个 2-4 选项的问题\n"
                "4. 观察 Claude 是否同时调用 AskUserQuestion 和 ask_on_atkbox")
            logger.info("Global CLAUDE.md instructions installed successfully")
        except Exception as e:
            messagebox.showerror("错误", f"安装全局指令失败:\n{e}")
            logger.error(f"Failed to install CLAUDE.md: {e}")

    def _uninstall_claude_md(self):
        """卸载全局 CLAUDE.md 指令"""
        try:
            from install_claude_md import uninstall as do_uninstall_claude_md
            do_uninstall_claude_md()
            self.config.set("claude_md_installed", False)
            messagebox.showinfo("成功", "全局 CLAUDE.md 指令已卸载。\n\n多选决策将不再自动同步到硬件。")
            logger.info("Global CLAUDE.md instructions uninstalled successfully")
        except Exception as e:
            messagebox.showerror("错误", f"卸载全局指令失败:\n{e}")
            logger.error(f"Failed to uninstall CLAUDE.md: {e}")

    def _start_daemon(self):
        """启动 daemon 后台服务"""
        if self.daemon_running:
            messagebox.showwarning("警告", "服务已在运行中")
            return

        try:
            # 创建 daemon 实例
            self.daemon = DaemonServer(
                ipc_port=int(self.ipc_port_var.get()),
                serial_port=self.serial_port_var.get(),
                push_interval=int(self.push_interval_var.get())
            )

            # 在独立线程启动
            self.daemon_running = True
            self.daemon_thread = threading.Thread(target=self._daemon_run, daemon=True)
            self.daemon_thread.start()

            # 更新按钮状态
            self.start_button.config(state=tk.DISABLED)
            self.stop_button.config(state=tk.NORMAL)

            logger.info("Daemon service started")

        except Exception as e:
            messagebox.showerror("错误", f"启动服务失败:\n{e}")
            logger.error(f"Failed to start daemon: {e}")
            self.daemon_running = False

    def _daemon_run(self):
        """Daemon 运行线程（阻塞）"""
        try:
            self.daemon.start()
        except Exception as e:
            logger.error(f"Daemon crashed: {e}")
            self.daemon_running = False

    def _stop_daemon(self):
        """停止 daemon 服务"""
        if not self.daemon_running:
            return

        try:
            if self.daemon:
                self.daemon.stop()
            self.daemon_running = False

            # 更新按钮状态
            self.start_button.config(state=tk.NORMAL)
            self.stop_button.config(state=tk.DISABLED)

            logger.info("Daemon service stopped")

        except Exception as e:
            messagebox.showerror("错误", f"停止服务失败:\n{e}")
            logger.error(f"Failed to stop daemon: {e}")

    def _clear_log(self):
        """清空日志窗口"""
        self.log_text.config(state=tk.NORMAL)
        self.log_text.delete(1.0, tk.END)
        self.log_text.config(state=tk.DISABLED)

    def _release_serial_ports(self):
        """释放所有占用串口的进程"""
        if messagebox.askyesno("确认", "即将强制关闭所有可能占用串口的进程（python.exe、CodeBuddyBridge.exe 等），继续吗？"):
            try:
                import subprocess
                killed_count = 0

                # 目标进程名列表（串口相关）
                target_processes = [
                    "python.exe",           # daemon、monitor 等
                    "CodeBuddyBridge.exe",  # 其他 Bridge 实例
                    "platformio.exe",       # PlatformIO 串口监视器
                    "pio.exe",
                    "esptool.exe",          # 烧录工具
                    "putty.exe",            # 串口终端
                    "TeraTerm.exe"          # 另一个常见串口终端
                ]

                for proc_name in target_processes:
                    try:
                        # 使用 taskkill /F 强制结束
                        result = subprocess.run(
                            ["taskkill", "/F", "/IM", proc_name],
                            capture_output=True,
                            text=True,
                            timeout=5
                        )

                        # 检查是否成功杀掉了进程
                        if result.returncode == 0 and "SUCCESS" in result.stdout:
                            killed_count += 1
                            logger.info(f"Killed process: {proc_name}")
                    except subprocess.TimeoutExpired:
                        logger.warning(f"Timeout killing {proc_name}")
                    except Exception as e:
                        # 进程不存在或无法杀死（正常情况）
                        logger.debug(f"Could not kill {proc_name}: {e}")

                if killed_count > 0:
                    messagebox.showinfo("完成", f"已强制关闭 {killed_count} 个进程。串口应该已释放，请稍后重试连接。")
                    logger.info(f"Released serial ports by killing {killed_count} processes")
                else:
                    messagebox.showinfo("完成", "未找到占用串口的进程（或所有串口已空闲）。")
                    logger.info("No serial-holding processes found")

            except Exception as e:
                messagebox.showerror("错误", f"释放串口失败:\n{e}")
                logger.error(f"Failed to release serial ports: {e}")

    def _start_update_loop(self):
        """启动定时更新循环"""
        self._update_log()
        self._update_status()
        self._update_decision_panel()
        # 每 500ms 更新一次
        self.root.after(500, self._start_update_loop)

    def _set_decision_input_enabled(self, enabled: bool):
        """启用/禁用决策输入控件"""
        state = tk.NORMAL if enabled else tk.DISABLED
        self.decision_input_entry.config(state=state)
        self.decision_submit_btn.config(state=state)

    def _update_decision_panel(self):
        """轮询守护进程的待处理决策，更新决策面板"""
        if not (self.daemon and self.daemon_running):
            # 服务未运行，清空面板
            if self._current_decision_id is not None:
                self._clear_decision_panel()
            return

        pending = self.daemon.get_pending_decision()

        if pending is None:
            # 无待处理决策
            if self._current_decision_id is not None:
                self._clear_decision_panel()
            return

        # 有待处理决策：检测是否为新决策
        if pending["id"] != self._current_decision_id:
            self._show_decision(pending)

    def _show_decision(self, pending: dict):
        """显示新的决策请求"""
        self._current_decision_id = pending["id"]
        title = pending["title"]
        options = pending["options"]

        # 更新标题
        self.decision_title_label.config(text=f"❓ {title}", foreground="#0066CC")

        # 清除旧选项按钮
        for btn in self.decision_option_buttons:
            btn.destroy()
        self.decision_option_buttons = []

        # 创建新选项按钮（数字+文本格式，与 BOX 一致）
        for i, opt in enumerate(options):
            btn = ttk.Button(
                self.decision_options_frame,
                text=f"{i+1}. {opt}",
                command=lambda idx=i: self._choose_decision(idx)
            )
            btn.pack(side=tk.LEFT, padx=5, pady=2)
            self.decision_option_buttons.append(btn)

        # 启用输入
        self._set_decision_input_enabled(True)
        self.decision_input_var.set("")
        self.decision_input_entry.focus_set()

        logger.info(f"Decision panel: {title} ({len(options)} options)")

    def _choose_decision(self, index: int):
        """用户点击选项按钮"""
        if self.daemon and self.daemon.set_decision_choice(index):
            logger.info(f"PC choice: option {index+1}")
            self._clear_decision_panel()

    def _submit_decision_text(self):
        """用户提交文本输入（数字或选项文本）"""
        text = self.decision_input_var.get().strip()
        if not text:
            return

        if not (self.daemon and self.daemon_running):
            return

        # 匹配文本到选项索引
        index = self.daemon.match_decision_text(text)
        if index is None:
            messagebox.showwarning("无匹配", f"'{text}' 无法匹配任何选项，或存在歧义。\n请输入数字 1-4 或明确的选项文本。")
            self.decision_input_var.set("")
            return

        if self.daemon.set_decision_choice(index):
            logger.info(f"PC text input '{text}' -> option {index+1}")
            self._clear_decision_panel()

    def _clear_decision_panel(self):
        """清空决策面板"""
        self._current_decision_id = None
        self.decision_title_label.config(text="（无待处理决策）", foreground="gray")

        # 清除选项按钮
        for btn in self.decision_option_buttons:
            btn.destroy()
        self.decision_option_buttons = []

        # 禁用输入
        self._set_decision_input_enabled(False)
        self.decision_input_var.set("")

    def _update_log(self):
        """从队列读取日志并显示"""
        while not self.log_queue.empty():
            try:
                msg = self.log_queue.get_nowait()
                self.log_text.config(state=tk.NORMAL)
                self.log_text.insert(tk.END, msg + "\n")
                self.log_text.see(tk.END)  # 自动滚动到底部

                # 限制日志行数（最多 1000 行）
                lines = int(self.log_text.index('end-1c').split('.')[0])
                if lines > 1000:
                    self.log_text.delete(1.0, f"{lines - 1000}.0")

                self.log_text.config(state=tk.DISABLED)
            except queue.Empty:
                break

    def _update_status(self):
        """更新状态面板"""
        if self.daemon and self.daemon_running:
            # 串口状态
            if self.daemon.serial_mgr.is_connected():
                self.serial_status_label.config(text="串口: ● 已连接", foreground="green")
            else:
                self.serial_status_label.config(text="串口: ● 未连接", foreground="red")

            # IPC 状态
            if self.daemon.is_running():
                self.ipc_status_label.config(text="IPC: ● 监听中", foreground="green")
            else:
                self.ipc_status_label.config(text="IPC: ● 未监听", foreground="red")

            # 获取统计信息
            stats = self.daemon.get_stats()

            # Agent 状态
            agent_state = stats.get("current_agent_state", "READY")
            self.agent_status_label.config(text=f"Agent: {agent_state}")

            # 统计信息
            frames_sent = stats.get("frames_sent", 0)
            self.frames_sent_label.config(text=f"已推送: {frames_sent} 帧")

            approved = stats.get("approvals_allowed", 0)
            rejected = stats.get("approvals_rejected", 0)
            total_approvals = stats.get("approvals_requested", 0)
            self.approvals_label.config(text=f"审批: {total_approvals} 次 (批准: {approved} | 拒绝: {rejected})")
        else:
            # 服务未运行
            self.serial_status_label.config(text="串口: ● 未连接", foreground="gray")
            self.ipc_status_label.config(text="IPC: ● 未监听", foreground="gray")
            self.agent_status_label.config(text="Agent: -")
            self.frames_sent_label.config(text="已推送: - 帧")
            self.approvals_label.config(text="审批: - 次")

    def _on_closing(self):
        """窗口关闭事件"""
        if self.daemon_running:
            if messagebox.askokcancel("退出", "服务正在运行，确定要退出吗？"):
                self._stop_daemon()
                self.root.destroy()
        else:
            self.root.destroy()

    def run(self):
        """运行 GUI 主循环"""
        logger.info("CodeBuddy Bridge GUI started")
        self.root.mainloop()


def main():
    app = CodeBuddyBridgeGUI()
    app.run()


if __name__ == "__main__":
    main()

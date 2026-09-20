#!/usr/bin/env python3
"""Daemon IPC 服务器单元测试"""
import sys
import json
import socket
import time
from pathlib import Path
from unittest.mock import Mock, patch, MagicMock

# 添加父目录到路径
sys.path.insert(0, str(Path(__file__).parent.parent))


def test_daemon_init():
    """测试 DaemonServer 初始化"""
    with patch('atkbox_daemon.SerialManager'):
        from atkbox_daemon import DaemonServer

        daemon = DaemonServer(ipc_port=47100, serial_port="COM6")

        assert daemon.ipc_port == 47100
        assert daemon.serial_port == "COM6"


def test_status_throttling():
    """测试状态推送节流（1秒去重）"""
    with patch('atkbox_daemon.SerialManager') as mock_sm_class:
        mock_sm = MagicMock()
        mock_sm.is_connected.return_value = True
        mock_sm.send_frame.return_value = True
        mock_sm_class.return_value = mock_sm

        from atkbox_daemon import DaemonServer

        daemon = DaemonServer(ipc_port=47101, serial_port="COM6")

        # 第一次请求：应发送
        req1 = {"type": "status", "state": "THINKING", "detail": "test"}
        resp1 = daemon._handle_status(req1)
        assert resp1["ok"] is True
        assert mock_sm.send_frame.call_count == 1

        # 0.5 秒后第二次相同请求：应被节流
        time.sleep(0.5)
        req2 = {"type": "status", "state": "THINKING", "detail": "test"}
        resp2 = daemon._handle_status(req2)
        assert resp2["ok"] is True
        assert mock_sm.send_frame.call_count == 1  # 未增加

        # 1.5 秒后第三次请求：应发送
        time.sleep(1.0)
        req3 = {"type": "status", "state": "THINKING", "detail": "test2"}
        resp3 = daemon._handle_status(req3)
        assert resp3["ok"] is True
        assert mock_sm.send_frame.call_count == 2  # 增加了


def test_ipc_socket_request():
    """测试 IPC socket 请求/响应"""
    with patch('atkbox_daemon.SerialManager') as mock_sm_class:
        mock_sm = MagicMock()
        mock_sm.is_connected.return_value = True
        mock_sm.send_frame.return_value = True
        mock_sm_class.return_value = mock_sm

        from atkbox_daemon import DaemonServer
        import threading

        daemon = DaemonServer(ipc_port=47102, serial_port="COM6")

        # 在后台线程启动服务器
        server_thread = threading.Thread(target=daemon.start, daemon=True)
        server_thread.start()

        time.sleep(0.2)  # 等待服务器启动

        try:
            # 发送 IPC 请求
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(2.0)
            sock.connect(("127.0.0.1", 47102))

            request = {"type": "status", "state": "RUNNING", "tool": "Read", "detail": "config.py"}
            sock.sendall((json.dumps(request) + "\n").encode('utf-8'))

            # 接收响应
            response_data = sock.recv(4096).decode('utf-8').strip()
            sock.close()

            response = json.loads(response_data)
            assert response["ok"] is True

        finally:
            daemon.stop()


def test_state_mapping():
    """测试状态字符串到状态码的映射"""
    with patch('atkbox_daemon.SerialManager') as mock_sm_class:
        mock_sm = MagicMock()
        mock_sm.is_connected.return_value = True
        mock_sm.send_frame.return_value = True
        mock_sm_class.return_value = mock_sm

        from atkbox_daemon import DaemonServer

        daemon = DaemonServer(ipc_port=47103, serial_port="COM6")

        # THINKING → 0x01
        daemon._handle_status({"type": "status", "state": "THINKING"})
        call_args = mock_sm.send_frame.call_args[0][0]
        assert call_args[1] == 0x01  # state 字段

        # RUNNING → 0x02
        daemon._handle_status({"type": "status", "state": "RUNNING"})
        call_args = mock_sm.send_frame.call_args[0][0]
        assert call_args[1] == 0x02

        # DONE → 0x03
        daemon._handle_status({"type": "status", "state": "DONE"})
        call_args = mock_sm.send_frame.call_args[0][0]
        assert call_args[1] == 0x03

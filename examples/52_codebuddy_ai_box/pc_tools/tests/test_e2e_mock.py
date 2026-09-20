#!/usr/bin/env python3
"""端到端集成测试 - 用 mock 串口验证完整链路

模拟 hook_client.py → daemon → 串口帧的完整数据流，
不需要真实硬件。
"""
import sys
import json
import socket
import struct
import time
import threading
from pathlib import Path
from unittest.mock import Mock, patch, MagicMock

# 添加父目录到路径
sys.path.insert(0, str(Path(__file__).parent.parent))


def _make_reply(task_id: int, action: int) -> bytes:
    """构造 0x0E 回执 payload（含 inner CRC8）"""
    from protocol_frames import crc8
    payload = struct.pack('<BHB', 0x0E, task_id, action)
    return payload + bytes([crc8(payload)])


def _send_ipc(port: int, request: dict, timeout: float = 3.0) -> dict:
    """发送 IPC 请求，返回响应"""
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(timeout)
    sock.connect(("127.0.0.1", port))
    sock.sendall((json.dumps(request) + "\n").encode('utf-8'))
    response = sock.recv(4096).decode('utf-8').strip()
    sock.close()
    return json.loads(response)


def test_e2e_status_frame_sent():
    """E2E: 状态请求 → 0x0C 帧发送"""
    with patch('atkbox_daemon.SerialManager') as mock_sm_class:
        mock_sm = MagicMock()
        mock_sm.is_connected.return_value = True
        mock_sm.send_frame.return_value = True
        mock_sm_class.return_value = mock_sm

        from atkbox_daemon import DaemonServer

        daemon = DaemonServer(ipc_port=47120, serial_port="COM6")
        daemon.serial_mgr = mock_sm

        server_thread = threading.Thread(target=daemon.start, daemon=True)
        server_thread.start()
        time.sleep(0.2)

        try:
            resp = _send_ipc(47120, {"type": "status", "state": "THINKING", "detail": "test prompt"})
            assert resp["ok"] is True

            # 验证发送了 0x0C 帧
            assert mock_sm.send_frame.called
            sent_frame = mock_sm.send_frame.call_args[0][0]
            assert sent_frame[0] == 0x0C  # frame_type
            assert sent_frame[1] == 0x01  # THINKING
            assert len(sent_frame) == 52
        finally:
            daemon.stop()


def test_e2e_approval_allow():
    """E2E: 审批请求 → 阻塞等待 → allow"""
    with patch('atkbox_daemon.SerialManager') as mock_sm_class:
        mock_sm = MagicMock()
        mock_sm.is_connected.return_value = True
        mock_sm.send_frame.return_value = True
        # 模拟 500ms 后返回 0x0E approve
        call_count = [0]

        def read_side_effect(timeout_ms):
            call_count[0] += 1
            if call_count[0] >= 3:  # 前2次返回None，第3次返回回执
                return _make_reply(0, 0x00)
            return None

        mock_sm.read_reply.side_effect = read_side_effect
        mock_sm_class.return_value = mock_sm

        from atkbox_daemon import DaemonServer

        daemon = DaemonServer(ipc_port=47121, serial_port="COM6")
        daemon.serial_mgr = mock_sm

        server_thread = threading.Thread(target=daemon.start, daemon=True)
        server_thread.start()
        time.sleep(0.2)

        try:
            resp = _send_ipc(47121, {
                "type": "approval", "tool": "Write",
                "file": "main.py", "preview": "code"
            }, timeout=5.0)
            assert resp["ok"] is True
            assert resp["decision"] == "allow"
        finally:
            daemon.stop()


def test_e2e_approval_deny():
    """E2E: 审批请求 → deny"""
    with patch('atkbox_daemon.SerialManager') as mock_sm_class:
        mock_sm = MagicMock()
        mock_sm.is_connected.return_value = True
        mock_sm.send_frame.return_value = True
        mock_sm.read_reply.return_value = _make_reply(0, 0x01)  # reject
        mock_sm_class.return_value = mock_sm

        from atkbox_daemon import DaemonServer

        daemon = DaemonServer(ipc_port=47122, serial_port="COM6")
        daemon.serial_mgr = mock_sm

        server_thread = threading.Thread(target=daemon.start, daemon=True)
        server_thread.start()
        time.sleep(0.2)

        try:
            resp = _send_ipc(47122, {
                "type": "approval", "tool": "Edit",
                "file": "config.py", "preview": "code"
            }, timeout=5.0)
            assert resp["ok"] is True
            assert resp["decision"] == "deny"
        finally:
            daemon.stop()


def test_e2e_hook_client_integration():
    """E2E: hook_client.py 逻辑集成（直接调用 send_request）"""
    with patch('atkbox_daemon.SerialManager') as mock_sm_class:
        mock_sm = MagicMock()
        mock_sm.is_connected.return_value = True
        mock_sm.send_frame.return_value = True
        mock_sm_class.return_value = mock_sm

        from atkbox_daemon import DaemonServer
        import hook_client

        daemon = DaemonServer(ipc_port=47123, serial_port="COM6")
        daemon.serial_mgr = mock_sm

        server_thread = threading.Thread(target=daemon.start, daemon=True)
        server_thread.start()
        time.sleep(0.2)

        try:
            # 使用 hook_client 的 send_request（临时改端口）
            original_port = hook_client.DAEMON_PORT
            hook_client.DAEMON_PORT = 47123

            resp = hook_client.send_request({
                "type": "status", "state": "RUNNING", "tool": "Read", "detail": "file.py"
            })
            assert resp["ok"] is True

            hook_client.DAEMON_PORT = original_port
        finally:
            daemon.stop()

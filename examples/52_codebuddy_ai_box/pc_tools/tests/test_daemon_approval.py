#!/usr/bin/env python3
"""Daemon 审批阻塞等待单元测试"""
import sys
import struct
from pathlib import Path
from unittest.mock import Mock, patch, MagicMock

# 添加父目录到路径
sys.path.insert(0, str(Path(__file__).parent.parent))


def _make_reply(task_id: int, action: int) -> bytes:
    """构造 0x0E 回执 payload（含 inner CRC8）"""
    from protocol_frames import crc8
    payload = struct.pack('<BHB', 0x0E, task_id, action)
    return payload + bytes([crc8(payload)])


def test_approval_allow():
    """测试审批通过（action=0x00）"""
    with patch('atkbox_daemon.SerialManager') as mock_sm_class:
        mock_sm = MagicMock()
        mock_sm.is_connected.return_value = True
        mock_sm.send_frame.return_value = True

        from atkbox_daemon import DaemonServer

        daemon = DaemonServer(ipc_port=47110, serial_port="COM6")
        daemon.serial_mgr = mock_sm

        # task_id 从 0 开始，第一个审批用 task_id=0
        # 模拟读到 action=0x00 (approve) 的回执
        mock_sm.read_reply.return_value = _make_reply(0, 0x00)

        req = {"type": "approval", "tool": "Write", "file": "main.py", "preview": "test"}
        resp = daemon._handle_approval(req)

        assert resp["ok"] is True
        assert resp["decision"] == "allow"


def test_approval_deny():
    """测试审批拒绝（action=0x01）"""
    with patch('atkbox_daemon.SerialManager') as mock_sm_class:
        mock_sm = MagicMock()
        mock_sm.is_connected.return_value = True
        mock_sm.send_frame.return_value = True

        from atkbox_daemon import DaemonServer

        daemon = DaemonServer(ipc_port=47111, serial_port="COM6")
        daemon.serial_mgr = mock_sm

        # 模拟读到 action=0x01 (reject) 的回执
        mock_sm.read_reply.return_value = _make_reply(0, 0x01)

        req = {"type": "approval", "tool": "Write", "file": "main.py", "preview": "test"}
        resp = daemon._handle_approval(req)

        assert resp["ok"] is True
        assert resp["decision"] == "deny"


def test_approval_wrong_task_id_ignored():
    """测试错误的 task_id 被忽略，继续等待"""
    with patch('atkbox_daemon.SerialManager') as mock_sm_class:
        mock_sm = MagicMock()
        mock_sm.is_connected.return_value = True
        mock_sm.send_frame.return_value = True

        from atkbox_daemon import DaemonServer

        daemon = DaemonServer(ipc_port=47112, serial_port="COM6")
        daemon.serial_mgr = mock_sm

        # 第一次读到错误 task_id=999，第二次读到正确 task_id=0
        replies = [
            _make_reply(999, 0x00),  # 错误 task_id，应忽略
            _make_reply(0, 0x00),    # 正确 task_id
        ]
        mock_sm.read_reply.side_effect = replies

        req = {"type": "approval", "tool": "Write", "file": "main.py", "preview": "test"}
        resp = daemon._handle_approval(req)

        assert resp["ok"] is True
        assert resp["decision"] == "allow"
        # 验证读取了 2 次
        assert mock_sm.read_reply.call_count == 2


def test_approval_serial_disconnect():
    """测试审批期间串口断线"""
    with patch('atkbox_daemon.SerialManager') as mock_sm_class:
        mock_sm = MagicMock()
        mock_sm.send_frame.return_value = True
        # 读取返回 None，且 is_connected 返回 False（断线）
        mock_sm.read_reply.return_value = None
        mock_sm.is_connected.return_value = False

        from atkbox_daemon import DaemonServer

        daemon = DaemonServer(ipc_port=47113, serial_port="COM6")
        daemon.serial_mgr = mock_sm

        req = {"type": "approval", "tool": "Write", "file": "main.py", "preview": "test"}
        resp = daemon._handle_approval(req)

        assert resp["ok"] is False
        assert "disconnect" in resp["error"].lower()


def test_approval_risk_inference():
    """测试风险等级推断"""
    with patch('atkbox_daemon.SerialManager') as mock_sm_class:
        mock_sm = MagicMock()
        mock_sm.is_connected.return_value = True
        mock_sm.send_frame.return_value = True
        mock_sm.read_reply.return_value = _make_reply(0, 0x00)

        from atkbox_daemon import DaemonServer

        daemon = DaemonServer(ipc_port=47114, serial_port="COM6")
        daemon.serial_mgr = mock_sm

        # 测试文件路径 → HIGH 风险 (0x02)
        daemon._handle_approval({"type": "approval", "tool": "Write", "file": "src/main.py", "preview": ""})
        sent_frame = mock_sm.send_frame.call_args[0][0]
        # 0x0D 帧的 risk_level 在偏移 3
        assert sent_frame[3] == 0x02  # HIGH

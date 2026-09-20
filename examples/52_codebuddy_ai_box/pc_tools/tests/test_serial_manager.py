#!/usr/bin/env python3
"""串口管理模块单元测试"""
import sys
import time
from pathlib import Path
from unittest.mock import Mock, patch, MagicMock

# 添加父目录到路径
sys.path.insert(0, str(Path(__file__).parent.parent))


def test_serial_manager_init():
    """测试 SerialManager 初始化"""
    with patch('serial.Serial') as mock_serial:
        from serial_manager import SerialManager

        sm = SerialManager(port="COM6", baudrate=115200, auto_reconnect=False)

        assert sm.port == "COM6"
        assert sm.baudrate == 115200
        assert not sm.auto_reconnect


def test_send_frame_when_disconnected():
    """测试断线时发送返回 False"""
    with patch('serial.Serial') as mock_serial:
        mock_serial.side_effect = Exception("Port not found")

        from serial_manager import SerialManager
        sm = SerialManager(port="COM999", auto_reconnect=False)

        assert not sm.is_connected()
        assert not sm.send_frame(b'\x0C\x01\x00')


def test_send_frame_success():
    """测试成功发送帧"""
    with patch('serial.Serial') as mock_serial_class:
        mock_ser = MagicMock()
        mock_serial_class.return_value = mock_ser

        from serial_manager import SerialManager
        sm = SerialManager(port="COM6", auto_reconnect=False)

        # 发送 0x0C 帧
        payload = b'\x0C\x01\x00' + b'\x00' * 48 + b'\xEF'
        result = sm.send_frame(payload)

        assert result is True
        assert mock_ser.write.called


def test_read_reply_timeout():
    """测试读取超时返回 None"""
    with patch('serial.Serial') as mock_serial_class:
        mock_ser = MagicMock()
        mock_ser.in_waiting = 0
        mock_serial_class.return_value = mock_ser

        from serial_manager import SerialManager
        sm = SerialManager(port="COM6", auto_reconnect=False)

        # 100ms 超时，应返回 None
        result = sm.read_reply(timeout_ms=100)
        assert result is None


def test_read_reply_valid_frame():
    """测试读取有效帧"""
    with patch('serial.Serial') as mock_serial_class:
        mock_ser = MagicMock()

        # 模拟串口返回 0x0E 帧: A5 5A 05 [0E 2E 16 00 CRC]
        from protocol_frames import crc8
        payload = b'\x0E\x2E\x16\x00'
        inner_crc = crc8(payload)
        outer_crc = crc8(payload + bytes([inner_crc]))
        frame = b'\xA5\x5A\x05' + payload + bytes([inner_crc, outer_crc])

        mock_ser.in_waiting = len(frame)
        mock_ser.read.return_value = frame
        mock_serial_class.return_value = mock_ser

        from serial_manager import SerialManager
        sm = SerialManager(port="COM6", auto_reconnect=False)

        result = sm.read_reply(timeout_ms=500)

        # 应返回去掉 A5 5A 和外层 CRC 的 payload
        assert result == payload + bytes([inner_crc])


def test_reconnect_cooldown():
    """测试重连冷却时间（5秒）"""
    with patch('serial.Serial') as mock_serial_class:
        mock_serial_class.side_effect = Exception("Port error")

        from serial_manager import SerialManager
        sm = SerialManager(port="COM6", auto_reconnect=True)

        # 第一次重连尝试
        result1 = sm.try_reconnect()
        assert result1 is False

        # 立即第二次尝试，应被冷却拒绝
        result2 = sm.try_reconnect()
        assert result2 is False


def test_reconnect_success():
    """测试重连成功"""
    with patch('serial.Serial') as mock_serial_class:
        call_count = [0]

        def serial_side_effect(*args, **kwargs):
            call_count[0] += 1
            if call_count[0] == 1:
                raise Exception("First attempt fails")
            else:
                return MagicMock()

        mock_serial_class.side_effect = serial_side_effect

        from serial_manager import SerialManager
        sm = SerialManager(port="COM6", auto_reconnect=True)

        assert not sm.is_connected()

        # 修改最后重连时间，绕过冷却
        sm._last_reconnect_attempt = time.time() - 10

        result = sm.try_reconnect()
        assert result is True
        assert sm.is_connected()

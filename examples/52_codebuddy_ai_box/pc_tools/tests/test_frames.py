#!/usr/bin/env python3
"""协议帧构造模块单元测试"""
import sys
import struct
from pathlib import Path

# 添加父目录到路径
sys.path.insert(0, str(Path(__file__).parent.parent))


def test_crc8_empty():
    """测试空数据的 CRC8"""
    from protocol_frames import crc8
    assert crc8(b'') == 0x00


def test_crc8_single_byte():
    """测试单字节 CRC8"""
    from protocol_frames import crc8
    assert crc8(b'\x01') == 0x07


def test_crc8_known_values():
    """测试已知数据的 CRC8（与固件一致性验证）"""
    from protocol_frames import crc8
    # 0x0C 帧的前3字节
    assert crc8(b'\x0C\x01\x00') == 0xEF


def test_wrap_frame():
    """测试帧封装"""
    from protocol_frames import wrap_frame, crc8
    payload = b'\x0C\x01\x00'
    frame = wrap_frame(payload)

    # 验证结构: A5 5A <len> <payload> <crc8>
    assert frame[0:2] == b'\xA5\x5A'
    assert frame[2] == 3
    assert frame[3:6] == payload
    assert frame[6] == crc8(payload)
    assert len(frame) == 7


def test_build_status_frame_minimal():
    """测试状态帧构造（最小数据）"""
    from protocol_frames import build_status_frame, crc8

    frame = build_status_frame(state=0x01, seq_num=42, task_message="Analyzing code")

    # 验证总长度
    assert len(frame) == 52

    # 解包并验证字段
    frame_type, state, seq = struct.unpack_from('<BBB', frame, 0)
    assert frame_type == 0x0C
    assert state == 0x01
    assert seq == 42

    # 验证消息字段
    msg_bytes = frame[3:51]
    assert len(msg_bytes) == 48
    assert msg_bytes.startswith(b'Analyzing code')

    # 验证 CRC8
    assert frame[51] == crc8(frame[:51])


def test_build_status_frame_utf8():
    """测试状态帧 UTF-8 编码和截断"""
    from protocol_frames import build_status_frame

    # 中文字符
    frame = build_status_frame(0x02, 10, "正在读取配置文件")
    msg_bytes = frame[3:51]
    assert msg_bytes[:24] == "正在读取配置文件".encode('utf-8')

    # 超长消息截断
    long_msg = "A" * 100
    frame = build_status_frame(0x02, 0, long_msg)
    assert len(frame) == 52
    msg_bytes = frame[3:51]
    assert msg_bytes[:48] == (b'A' * 48)


def test_build_approval_frame():
    """测试审批帧构造"""
    from protocol_frames import build_approval_frame, crc8

    frame = build_approval_frame(
        task_id=1234,
        risk=0x02,
        title="Write to main.py",
        target="src/main.py",
        diff="+10 -3"
    )

    # 验证总长度
    assert len(frame) == 109

    # 解包并验证字段
    ft, tid, risk = struct.unpack_from('<BHB', frame, 0)
    assert ft == 0x0D
    assert tid == 1234
    assert risk == 0x02

    # 验证字符串字段
    assert frame[4:36].startswith(b'Write to main.py')
    assert frame[36:68].startswith(b'src/main.py')
    assert frame[68:108].startswith(b'+10 -3')

    # 验证 CRC8
    assert frame[108] == crc8(frame[:108])


def test_parse_approval_reply():
    """测试审批回执解析（正常情况）"""
    from protocol_frames import parse_approval_reply, crc8

    # 构造 0x0E 帧: frame_type + task_id + action + crc8
    payload = struct.pack('<BHB', 0x0E, 5678, 0x00)
    frame = payload + bytes([crc8(payload)])

    result = parse_approval_reply(frame)
    assert result is not None
    assert result == {"task_id": 5678, "action": 0x00}


def test_parse_approval_reply_crc_fail():
    """测试审批回执解析（CRC 错误）"""
    from protocol_frames import parse_approval_reply

    # 错误的 CRC
    frame = b'\x0E\x2E\x16\x00\xFF'
    assert parse_approval_reply(frame) is None


def test_parse_approval_reply_wrong_type():
    """测试审批回执解析（错误的帧类型）"""
    from protocol_frames import parse_approval_reply, crc8

    # frame_type = 0x0C (不是 0x0E)
    payload = struct.pack('<BHB', 0x0C, 1234, 0x00)
    frame = payload + bytes([crc8(payload)])

    assert parse_approval_reply(frame) is None

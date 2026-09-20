import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from serial_link import crc8, encode_frame, pack_decision_request, parse_reply


def test_crc8_matches_known():
    # 与 Dongle espnow_crc8 同算法 (多项式 0x07 初值 0x00)
    assert crc8(b"\x01\x02\x03") == 0x48


def test_encode_frame_layout():
    f = encode_frame(b"\xAA\xBB")
    assert f[0] == 0xA5 and f[1] == 0x5A and f[2] == 2
    assert f[3:5] == b"\xAA\xBB"
    assert f[5] == crc8(b"\xAA\xBB")


def test_pack_decision_request_size():
    body = pack_decision_request(7, 0, "Allow Bash", ["Yes", "No", "Always"])
    assert len(body) == 134
    assert body[0] == 0x0A
    assert struct.unpack_from("<H", body, 1)[0] == 7  # decision_id
    assert body[3] == 0  # kind
    assert body[3 + 1 + 32] == 3  # opt_count 位置: frame_type(1)+id(2)+kind(1)+title(32)


def test_parse_reply_roundtrip():
    payload = bytes([0x0B]) + struct.pack("<H", 7) + bytes([1])
    payload += bytes([crc8(payload)])
    assert parse_reply(payload) == (7, 1)


def test_parse_reply_bad_crc():
    payload = bytes([0x0B]) + struct.pack("<H", 7) + bytes([1, 0x00])
    assert parse_reply(payload) is None

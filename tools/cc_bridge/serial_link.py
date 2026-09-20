"""PC↔Dongle 串口链路: 帧编解码 + 后台读线程 + decision_id 等待/唤醒。

串口帧格式 (与 Dongle pc_link.c 对称):
    0xA5 0x5A | len(1) | payload(len) | crc8(1)
payload 首字节即 frame_type (0x0A 决策请求 / 0x0B 决策回执)。
CRC8: 多项式 0x07, 初值 0x00, 与 Dongle espnow_crc8 一致。
"""
import struct
import threading

import serial

TITLE_LEN, OPT_LEN, MAX_OPTS = 32, 24, 4
FRAME_DECISION_REQ, FRAME_DECISION_REPLY = 0x0A, 0x0B
MAGIC0, MAGIC1 = 0xA5, 0x5A


def crc8(data: bytes) -> int:
    crc = 0
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = ((crc << 1) ^ 0x07) & 0xFF if (crc & 0x80) else (crc << 1) & 0xFF
    return crc


def encode_frame(payload: bytes) -> bytes:
    return bytes([MAGIC0, MAGIC1, len(payload)]) + payload + bytes([crc8(payload)])


def _fixed(s: str, n: int) -> bytes:
    b = s.encode("utf-8")[: n - 1]
    return b + b"\x00" * (n - len(b))


def pack_decision_request(decision_id, kind, title, opts):
    body = bytes([FRAME_DECISION_REQ]) + struct.pack("<H", decision_id) + bytes([kind])
    body += _fixed(title, TITLE_LEN)
    body += bytes([len(opts)])
    for i in range(MAX_OPTS):
        body += _fixed(opts[i] if i < len(opts) else "", OPT_LEN)
    body += bytes([crc8(body)])
    return body


def parse_reply(payload: bytes):
    if len(payload) != 5 or payload[0] != FRAME_DECISION_REPLY:
        return None
    if crc8(payload[:4]) != payload[4]:
        return None
    decision_id = struct.unpack_from("<H", payload, 1)[0]
    return (decision_id, payload[3])


class SerialLink:
    def __init__(self, port, baud=115200):
        self._port, self._baud = port, baud
        self._ser = None
        self._next_id = 1
        self._events = {}   # decision_id -> threading.Event
        self._results = {}  # decision_id -> chosen_index
        self._lock = threading.Lock()
        self._stop = False

    def open(self):
        self._ser = serial.Serial(self._port, self._baud, timeout=0.1)
        threading.Thread(target=self._reader, daemon=True).start()

    def close(self):
        self._stop = True
        if self._ser is not None:
            self._ser.close()

    def send_decision(self, kind, title, opts) -> int:
        with self._lock:
            did = self._next_id
            self._next_id = (self._next_id + 1) & 0xFFFF
            self._events[did] = threading.Event()
        body = pack_decision_request(did, kind, title, opts)
        # 外层帧 payload = 完整 134 字节 0x0A 帧 (含其自身帧内 crc8);
        # encode_frame 再加一层串口帧 crc8 (供 Dongle 校验). 两层校验各自独立.
        self._ser.write(encode_frame(body))
        return did

    def wait_reply(self, decision_id, timeout=120.0) -> int:
        ev = self._events.get(decision_id)
        if ev is None:
            raise KeyError(decision_id)
        if not ev.wait(timeout):
            raise TimeoutError(f"decision {decision_id} timed out")
        return self._results.pop(decision_id, 0xFF)

    def _reader(self):
        st = 0
        need = 0
        buf = bytearray()
        while not self._stop:
            b = self._ser.read(1)
            if not b:
                continue
            c = b[0]
            if st == 0:
                st = 1 if c == MAGIC0 else 0
            elif st == 1:
                st = 2 if c == MAGIC1 else 0
            elif st == 2:
                need = c
                buf = bytearray()
                st = 3 if 0 < need <= 160 else 0
            elif st == 3:
                buf.append(c)
                if len(buf) >= need:
                    st = 4
            elif st == 4:
                if crc8(bytes(buf)) == c:
                    r = parse_reply(bytes(buf))
                    if r:
                        did, idx = r
                        with self._lock:
                            self._results[did] = idx
                            if did in self._events:
                                self._events[did].set()
                st = 0

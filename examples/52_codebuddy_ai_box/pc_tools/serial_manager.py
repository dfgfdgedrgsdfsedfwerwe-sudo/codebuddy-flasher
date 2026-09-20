#!/usr/bin/env python3
"""串口管理模块 - 负责串口打开/关闭、断线检测、自动重连

SerialManager 独占串口设备，处理：
- 串口打开与配置（115200 8N1）
- 帧发送（自动封装 A5 5A）
- 帧接收（解析 A5 5A 包装，验证 CRC8）
- 断线检测与标记
- 5 秒冷却重连
"""
import time
import logging
from typing import Optional

import serial

from protocol_frames import wrap_frame, crc8, MAGIC

logger = logging.getLogger(__name__)

# 重连冷却时间（秒）
RECONNECT_COOLDOWN = 5.0


class SerialManager:
    """串口管理器 - 独占 Dongle 串口设备"""

    def __init__(self, port: str, baudrate: int = 115200, timeout: float = 0.1, auto_reconnect: bool = True):
        """初始化串口管理器

        Args:
            port: 串口设备名（Windows: "COM6", Linux: "/dev/ttyUSB0"）
            baudrate: 波特率，默认 115200
            timeout: 读超时（秒），默认 0.1
            auto_reconnect: 是否自动重连
        """
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.auto_reconnect = auto_reconnect

        self.ser: Optional[serial.Serial] = None
        self._last_reconnect_attempt = 0.0

        # 尝试打开串口
        self._open()

    def _open(self) -> bool:
        """打开串口

        Returns:
            True=成功, False=失败
        """
        try:
            self.ser = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=self.timeout
            )
            logger.info(f"Serial opened: {self.port} @ {self.baudrate}")
            return True
        except Exception as e:
            logger.error(f"Failed to open {self.port}: {e}")
            self.ser = None
            return False

    def is_connected(self) -> bool:
        """检查串口是否连接

        Returns:
            True=已连接, False=未连接
        """
        return self.ser is not None and self.ser.is_open

    def send_frame(self, payload: bytes) -> bool:
        """发送帧（自动封装 A5 5A）

        Args:
            payload: 内层帧数据（已包含 inner CRC8）

        Returns:
            True=成功, False=失败
        """
        if not self.is_connected():
            logger.warning("Cannot send frame: serial not connected")
            return False

        try:
            frame = wrap_frame(payload)
            self.ser.write(frame)
            logger.debug(f"Sent frame: {frame.hex()}")
            return True
        except Exception as e:
            logger.error(f"Send frame failed: {e}")
            self._mark_disconnected()
            return False

    def read_reply(self, timeout_ms: int) -> Optional[bytes]:
        """阻塞读取一个完整回执帧（去掉 A5 5A 外层包装）

        轮询读取串口，查找 A5 5A 包装帧，验证 CRC8 后返回 payload。

        Args:
            timeout_ms: 超时时间（毫秒）

        Returns:
            payload (去掉 A5 5A 和 outer CRC 的内层帧) 或 None（超时/断线）
        """
        if not self.is_connected():
            return None

        start = time.time()
        buffer = bytearray()

        while (time.time() - start) * 1000 < timeout_ms:
            try:
                if self.ser.in_waiting > 0:
                    chunk = self.ser.read(self.ser.in_waiting)
                    buffer.extend(chunk)

                # 查找完整帧: A5 5A <len> <payload> <crc8>
                while len(buffer) >= 4:
                    # 找魔术字
                    if buffer[0] != 0xA5 or buffer[1] != 0x5A:
                        buffer.pop(0)
                        continue

                    payload_len = buffer[2]

                    # 帧长度 = 3(header) + payload_len + 1(outer_crc8)
                    frame_len = 3 + payload_len + 1

                    if len(buffer) < frame_len:
                        # 帧不完整，等待更多数据
                        break

                    # 提取完整帧
                    frame = buffer[:frame_len]
                    payload = frame[3:3 + payload_len]
                    outer_crc = frame[-1]

                    # 验证外层 CRC8
                    if crc8(payload) != outer_crc:
                        logger.warning(f"Outer CRC mismatch: frame={frame.hex()}")
                        buffer = buffer[frame_len:]
                        continue

                    # 移除已处理的帧
                    buffer = buffer[frame_len:]

                    logger.debug(f"Received frame: payload={payload.hex()}")
                    return bytes(payload)

                # 短暂休眠，避免 CPU 占用
                time.sleep(0.01)

            except Exception as e:
                logger.error(f"Read reply error: {e}")
                self._mark_disconnected()
                return None

        # 超时
        return None

    def try_reconnect(self) -> bool:
        """尝试重连串口（5 秒冷却）

        Returns:
            True=重连成功, False=失败或冷却中
        """
        if self.is_connected():
            return True

        if not self.auto_reconnect:
            return False

        # 检查冷却时间
        now = time.time()
        if now - self._last_reconnect_attempt < RECONNECT_COOLDOWN:
            return False

        self._last_reconnect_attempt = now
        logger.info(f"Attempting to reconnect to {self.port}...")

        # 关闭旧连接（如果存在）
        if self.ser is not None:
            try:
                self.ser.close()
            except:
                pass
            self.ser = None

        # 尝试重新打开
        return self._open()

    def _mark_disconnected(self):
        """标记串口断线（关闭连接）"""
        if self.ser is not None:
            try:
                self.ser.close()
            except:
                pass
            self.ser = None
            logger.warning(f"Serial marked as disconnected: {self.port}")

    def close(self):
        """关闭串口"""
        if self.ser is not None:
            try:
                self.ser.close()
                logger.info(f"Serial closed: {self.port}")
            except Exception as e:
                logger.error(f"Error closing serial: {e}")
            finally:
                self.ser = None

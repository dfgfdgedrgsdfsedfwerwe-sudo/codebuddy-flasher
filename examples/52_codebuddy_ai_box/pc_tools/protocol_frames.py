#!/usr/bin/env python3
"""协议帧构造与解析模块 - 对齐固件 espnow_protocol.h 的 struct 定义

严格按固件 packed struct 构造帧，确保字节对齐。
所有帧内部已包含 inner CRC8，外层串口传输时再套 A5 5A 包装。
"""
import struct

# 串口包装魔术字
MAGIC = bytes([0xA5, 0x5A])

# 帧类型定义（与固件 espnow_protocol.h 一致）
FRAME_TYPE_TOKEN_STATUS = 0x07
FRAME_TYPE_PROJECT_STATUS = 0x08
FRAME_TYPE_DECISION_REQ = 0x0A
FRAME_TYPE_DECISION_REPLY = 0x0B
FRAME_TYPE_AGENT_STATUS = 0x0C
FRAME_TYPE_AGENT_APPROVAL_REQ = 0x0D
FRAME_TYPE_AGENT_APPROVAL_REPLY = 0x0E

# 字段长度常量（与固件 struct 定义严格对齐）
TOKEN_NAME_LEN = 20
TOKEN_MAX_ITEMS = 5
PROJECT_NAME_LEN = 24
PROJECT_MAX_ITEMS = 6
AGENT_TASK_MSG_LEN = 48
APPROVAL_TITLE_LEN = 32
APPROVAL_TARGET_LEN = 32
APPROVAL_DIFF_LEN = 40
DECISION_TITLE_LEN = 32
DECISION_OPT_LEN = 24
DECISION_MAX_OPTS = 4


def crc8(data: bytes) -> int:
    """CRC8 校验 (多项式 0x07, 初值 0x00) - 与固件 espnow_crc8 一致

    Args:
        data: 待校验的字节数据

    Returns:
        CRC8 校验值 (0x00-0xFF)
    """
    crc = 0x00
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x80:
                crc = ((crc << 1) ^ 0x07) & 0xFF
            else:
                crc = (crc << 1) & 0xFF
    return crc


def wrap_frame(payload: bytes) -> bytes:
    """封帧：A5 5A <length> <payload> <outer_crc8>

    外层串口包装协议，与 Dongle pc_link.c 一致。

    Args:
        payload: 内层帧数据（已包含 inner CRC8）

    Returns:
        完整串口帧 (4 + len(payload) 字节)

    Raises:
        ValueError: payload 长度超过 255 字节
    """
    if len(payload) > 255:
        raise ValueError(f"Payload too large: {len(payload)} > 255")

    frame = MAGIC + bytes([len(payload)]) + payload
    frame += bytes([crc8(payload)])  # 外层 CRC8 覆盖 payload
    return frame


def build_status_frame(state: int, seq_num: int, task_message: str) -> bytes:
    """构造 0x0C Agent Status 帧（52 字节，未封装 A5 5A）

    对齐固件 agent_status_frame_t:
        typedef struct __attribute__((packed)) {
            uint8_t  frame_type;               // = 0x0C
            uint8_t  state;                    // agent_state_t (0x00-0x04)
            uint8_t  seq_num;                  // 序列号
            char     task_message[48];         // UTF-8 任务回显
            uint8_t  crc8;                     // CRC8 校验
        } agent_status_frame_t;  // 52 字节

    Args:
        state: agent_state_t 状态码 (0x00=READY, 0x01=THINKING, 0x02=RUNNING, 0x03=DONE, 0x04=ERROR)
        seq_num: 序列号 (0-255，循环递增)
        task_message: 任务回显文字，UTF-8 编码，最多 48 字节

    Returns:
        52 字节的 0x0C 帧（包含 inner CRC8）
    """
    # 编码并截断/补齐到 48 字节
    msg_bytes = task_message.encode('utf-8')[:AGENT_TASK_MSG_LEN]
    msg_bytes = msg_bytes.ljust(AGENT_TASK_MSG_LEN, b'\x00')

    # 构造帧体（前 51 字节）
    frame = struct.pack(
        '<BBB48s',  # 小端序: uint8, uint8, uint8, char[48]
        FRAME_TYPE_AGENT_STATUS,
        state & 0xFF,
        seq_num & 0xFF,
        msg_bytes
    )

    # 追加 inner CRC8
    frame += bytes([crc8(frame)])

    assert len(frame) == 52, f"Status frame length mismatch: {len(frame)} != 52"
    return frame


def build_approval_frame(task_id: int, risk: int, title: str, target: str, diff: str) -> bytes:
    """构造 0x0D Agent Approval Request 帧（109 字节）

    对齐固件 agent_approval_request_frame_t:
        typedef struct __attribute__((packed)) {
            uint8_t  frame_type;               // = 0x0D
            uint16_t task_id;                  // 任务 ID (小端序)
            uint8_t  risk_level;               // 0=LOW/1=MEDIUM/2=HIGH
            char     title[32];                // "Write to src/main.py"
            char     target[32];               // "src/main.py"
            char     diff_summary[40];         // "+5 -2 lines"
            uint8_t  crc8;                     // CRC8 校验
        } agent_approval_request_frame_t;  // 109 字节

    Args:
        task_id: 任务 ID (0-65535)
        risk: 风险等级 (0=LOW, 1=MEDIUM, 2=HIGH)
        title: 审批标题，UTF-8 编码，最多 32 字节
        target: 目标文件路径，UTF-8 编码，最多 32 字节
        diff: 差异摘要，UTF-8 编码，最多 40 字节

    Returns:
        109 字节的 0x0D 帧（包含 inner CRC8）
    """
    # 编码并截断/补齐各字符串字段
    title_bytes = title.encode('utf-8')[:APPROVAL_TITLE_LEN].ljust(APPROVAL_TITLE_LEN, b'\x00')
    target_bytes = target.encode('utf-8')[:APPROVAL_TARGET_LEN].ljust(APPROVAL_TARGET_LEN, b'\x00')
    diff_bytes = diff.encode('utf-8')[:APPROVAL_DIFF_LEN].ljust(APPROVAL_DIFF_LEN, b'\x00')

    # 构造帧体（前 108 字节）
    frame = struct.pack(
        '<BHB32s32s40s',  # 小端序: uint8, uint16, uint8, char[32], char[32], char[40]
        FRAME_TYPE_AGENT_APPROVAL_REQ,
        task_id & 0xFFFF,
        risk & 0xFF,
        title_bytes,
        target_bytes,
        diff_bytes
    )

    # 追加 inner CRC8
    frame += bytes([crc8(frame)])

    assert len(frame) == 109, f"Approval frame length mismatch: {len(frame)} != 109"
    return frame


def parse_approval_reply(data: bytes) -> dict | None:
    """解析 0x0E Agent Approval Reply 帧（5 字节）

    对齐固件 agent_approval_reply_frame_t:
        typedef struct __attribute__((packed)) {
            uint8_t  frame_type;    // = 0x0E
            uint16_t task_id;       // 对应请求的任务 ID
            uint8_t  action;        // 0x00=approve, 0x01=reject, 0x02=view_diff, 0xFF=timeout
            uint8_t  crc8;          // CRC8 校验
        } agent_approval_reply_frame_t;  // 5 字节

    Args:
        data: 接收到的 0x0E 帧数据（5 字节）

    Returns:
        {"task_id": int, "action": int} 或 None（长度/CRC/类型错误）
    """
    if len(data) != 5:
        return None

    # 解包
    frame_type, task_id, action, frame_crc = struct.unpack('<BHBB', data)

    # 验证帧类型
    if frame_type != FRAME_TYPE_AGENT_APPROVAL_REPLY:
        return None

    # 验证 CRC8（覆盖前 4 字节）
    if crc8(data[:4]) != frame_crc:
        return None

    return {"task_id": task_id, "action": action}


def build_token_frame(items: list, seq_num: int = 0) -> bytes:
    """构造 Token 状态帧 (0x07, 154 字节，未封装 A5 5A)

    对齐固件 token_status_frame_t:
        uint8_t frame_type (=0x07) + uint8_t count + uint8_t seq_num
        + token_item_t items[5] (每项 30 字节) + uint8_t crc8

    token_item_t: char name[20] + uint32 used + uint32 total + uint16 percent_x10

    Args:
        items: Token 列表, 每项 {"name": str, "used": int, "total": int, "percent_x10": int}
               最多 TOKEN_MAX_ITEMS (5) 项
        seq_num: 序列号 (0-255)

    Returns:
        154 字节的 0x07 帧（包含 inner CRC8，未封装）

    Raises:
        ValueError: items 超过 5 项
    """
    if len(items) > TOKEN_MAX_ITEMS:
        raise ValueError(f"Too many token items: {len(items)} > {TOKEN_MAX_ITEMS}")

    count = len(items)
    payload = struct.pack('<BBB', FRAME_TYPE_TOKEN_STATUS, count, seq_num & 0xFF)

    # 打包有效条目
    for item in items:
        name = item.get("name", "Unknown").encode('utf-8')[:TOKEN_NAME_LEN-1]
        name = name.ljust(TOKEN_NAME_LEN, b'\x00')
        used = item.get("used", 0) & 0xFFFFFFFF
        total = item.get("total", 1) & 0xFFFFFFFF
        percent_x10 = item.get("percent_x10", 0) & 0xFFFF
        payload += struct.pack('<20sIIH', name, used, total, percent_x10)

    # 填充剩余空位（固件是固定 5 项数组）
    for _ in range(TOKEN_MAX_ITEMS - count):
        payload += struct.pack('<20sIIH', b'\x00' * TOKEN_NAME_LEN, 0, 0, 0)

    payload += bytes([crc8(payload)])
    assert len(payload) == 154, f"Token frame length mismatch: {len(payload)} != 154"
    return payload


def build_project_frame(items: list, seq_num: int = 0) -> bytes:
    """构造 Project 状态帧 (0x08, 154 字节，未封装 A5 5A)

    对齐固件 project_status_frame_t:
        uint8_t frame_type (=0x08) + uint8_t count + uint8_t seq_num
        + project_item_t items[6] (每项 25 字节) + uint8_t crc8

    project_item_t: char name[24] + uint8 status_code

    status_code: 0=Planning 1=Coding 2=Review 3=Completed 4=Error 5=Idle

    Args:
        items: Project 列表, 每项 {"name": str, "status_code": int}
               最多 PROJECT_MAX_ITEMS (6) 项
        seq_num: 序列号 (0-255)

    Returns:
        154 字节的 0x08 帧（包含 inner CRC8，未封装）

    Raises:
        ValueError: items 超过 6 项
    """
    if len(items) > PROJECT_MAX_ITEMS:
        raise ValueError(f"Too many project items: {len(items)} > {PROJECT_MAX_ITEMS}")

    count = len(items)
    payload = struct.pack('<BBB', FRAME_TYPE_PROJECT_STATUS, count, seq_num & 0xFF)

    # 打包有效条目
    for item in items:
        name = item.get("name", "Unknown").encode('utf-8')[:PROJECT_NAME_LEN-1]
        name = name.ljust(PROJECT_NAME_LEN, b'\x00')
        status_code = item.get("status_code", 5) & 0xFF  # 默认 IDLE
        payload += struct.pack('<24sB', name, status_code)

    # 填充剩余空位（固件是固定 6 项数组）
    for _ in range(PROJECT_MAX_ITEMS - count):
        payload += struct.pack('<24sB', b'\x00' * PROJECT_NAME_LEN, 5)

    payload += bytes([crc8(payload)])
    assert len(payload) == 154, f"Project frame length mismatch: {len(payload)} != 154"
    return payload


def build_decision_frame(decision_id: int, title: str, options: list, kind: int = 1) -> bytes:
    """构造决策请求帧 (0x0A, 134 字节，未封装 A5 5A)

    对齐固件 decision_request_frame_t:
        uint8_t  frame_type (=0x0A)
        uint16_t decision_id (递增, 回执对齐)
        uint8_t  kind (0=工具权限 1=多选题)
        char     title[32]
        uint8_t  opt_count (1-4)
        char     opts[4][24]
        uint8_t  crc8

    Args:
        decision_id: 决策 ID (0-65535)，用于匹配 0x0B 回执
        title: 标题，UTF-8，最多 31 字节
        options: 选项列表，1-4 个，每个最多 23 字节
        kind: 0=工具权限，1=多选题（默认）

    Returns:
        134 字节的 0x0A 帧（含 inner CRC8，未封装）

    Raises:
        ValueError: 选项数量非法
    """
    opt_count = len(options)
    if opt_count < 1 or opt_count > DECISION_MAX_OPTS:
        raise ValueError(f"Options count must be 1-{DECISION_MAX_OPTS}, got {opt_count}")

    title_bytes = title.encode('utf-8')[:DECISION_TITLE_LEN-1].ljust(DECISION_TITLE_LEN, b'\x00')

    # frame_type + decision_id + kind + title
    payload = struct.pack('<BHB32s', FRAME_TYPE_DECISION_REQ, decision_id & 0xFFFF, kind & 0xFF, title_bytes)

    # opt_count
    payload += struct.pack('<B', opt_count)

    # 4 个选项（每个 24 字节，不足补 \0）
    for i in range(DECISION_MAX_OPTS):
        if i < opt_count:
            opt_bytes = options[i].encode('utf-8')[:DECISION_OPT_LEN-1].ljust(DECISION_OPT_LEN, b'\x00')
        else:
            opt_bytes = b'\x00' * DECISION_OPT_LEN
        payload += opt_bytes

    payload += bytes([crc8(payload)])
    assert len(payload) == 134, f"Decision frame length mismatch: {len(payload)} != 134"
    return payload


def parse_decision_reply(data: bytes) -> dict | None:
    """解析决策回执帧 (0x0B, 5 字节)

    对齐固件 decision_reply_frame_t:
        uint8_t  frame_type (=0x0B)
        uint16_t decision_id
        uint8_t  chosen_index (0xFF=超时/取消)
        uint8_t  crc8

    Args:
        data: 接收到的 0x0B 帧数据（5 字节）

    Returns:
        {"decision_id": int, "chosen_index": int} 或 None（校验失败）
    """
    if len(data) != 5:
        return None

    frame_type, decision_id, chosen_index, frame_crc = struct.unpack('<BHBB', data)

    if frame_type != FRAME_TYPE_DECISION_REPLY:
        return None

    if crc8(data[:4]) != frame_crc:
        return None

    return {"decision_id": decision_id, "chosen_index": chosen_index}

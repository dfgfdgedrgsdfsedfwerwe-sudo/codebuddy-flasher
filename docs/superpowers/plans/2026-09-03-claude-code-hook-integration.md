# Claude Code Hook 集成实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 ATK BOX 的 Agent 工作流可视化和物理审批功能接入 Claude Code 的 hook 事件系统，实现状态实时推送和阻塞式审批交互。

**Architecture:** PC 守护进程（atkbox_daemon.py）独占串口，hook 脚本（hook_client.py）通过 TCP socket 连接守护进程发送 IPC 请求。状态推送（0x0C）走非阻塞，审批（0x0D/0x0E）走阻塞轮询。

**Tech Stack:** Python 3.9+, pyserial 3.5+, socket (stdlib), struct (stdlib), threading, Claude Code hooks

## Global Constraints

- Python 版本 ≥ 3.9（需要 dict merge operator `|`）
- pyserial 版本 ≥ 3.5
- 串口参数：COM6, 115200, 8N1, timeout=0.1s
- IPC 端口：127.0.0.1:47100（不与现有 ANTHROPIC_BASE_URL 代理 10048 冲突）
- 帧格式：`A5 5A <length> <payload> <crc8>`，严格按固件 struct 对齐
- 0x0C 帧长度 = 52 字节，0x0D 帧长度 = 109 字节，0x0E 帧长度 = 5 字节
- CRC8 多项式 = 0x07，初值 = 0x00（与固件 `espnow_crc8` 一致）
- UTF-8 编码，字符串字段右补 `\x00` 到固定长度
- 日志路径：`~/.claude/atkbox_daemon.log`（Windows 展开为 `C:\Users\<user>\.claude\`）
- Hook 配置路径：`~/.claude/settings.json`

---

## 文件结构

**新增文件**（PC 端，固件不动）：

1. **pc_tools/atkbox_daemon.py** (550 行)  
   守护进程主体：串口管理、IPC 服务器、状态节流、审批阻塞等待、日志

2. **pc_tools/hook_client.py** (120 行)  
   Hook 瘦客户端：根据 argv[1] 决定行为（user_prompt/pre_tool/post_tool/stop），连 daemon 发 JSON

3. **pc_tools/tests/test_daemon_frames.py** (200 行)  
   单元测试：帧构造、CRC8、task_id 匹配、审批回复解析

4. **pc_tools/tests/test_ipc_protocol.py** (150 行)  
   集成测试：IPC 请求/响应、节流逻辑、断线处理

5. **pc_tools/install_hooks.py** (80 行)  
   辅助脚本：合并 hook 配置到 `~/.claude/settings.json`（保留已有字段）

6. **pc_tools/README_HOOKS.md** (文档)  
   部署指南：daemon 启动、hook 注册、测试步骤、故障排查

**删除文件**：

- **pc_tools/agent_status_bridge.py** — 被 daemon 完全替代（现有实现有协议 bug）

**修改文件**：

- **~/.claude/settings.json** — 新增 4 个 hook（UserPromptSubmit/PreToolUse/PostToolUse/Stop）

---

### Task 1: 帧构造与 CRC8 模块

**Files:**
- Create: `examples/52_codebuddy_ai_box/pc_tools/protocol_frames.py`
- Create: `examples/52_codebuddy_ai_box/pc_tools/tests/test_frames.py`

**Interfaces:**
- Consumes: `espnow_protocol.h` 的 struct 定义（只读参考，不修改固件）
- Produces:
  - `def crc8(data: bytes) -> int` — CRC8 计算（多项式 0x07，初值 0x00）
  - `def wrap_frame(payload: bytes) -> bytes` — 封帧 `A5 5A <len> <payload> <crc8>`
  - `def build_status_frame(state: int, seq_num: int, task_message: str) -> bytes` — 构造 0x0C 帧（52 字节）
  - `def build_approval_frame(task_id: int, risk: int, title: str, target: str, diff: str) -> bytes` — 构造 0x0D 帧（109 字节）
  - `def parse_approval_reply(data: bytes) -> dict | None` — 解析 0x0E 帧（返回 `{"task_id": int, "action": int}` 或 None）

- [ ] **Step 1: Write failing test for CRC8**

```python
# examples/52_codebuddy_ai_box/pc_tools/tests/test_frames.py
import sys, struct
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent.parent))

def test_crc8_empty():
    from protocol_frames import crc8
    assert crc8(b'') == 0x00

def test_crc8_single_byte():
    from protocol_frames import crc8
    assert crc8(b'\x01') == 0x07
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd examples/52_codebuddy_ai_box/pc_tools && python -m pytest tests/test_frames.py::test_crc8_empty -v`
Expected: `ModuleNotFoundError: No module named 'protocol_frames'`

- [ ] **Step 3: Implement CRC8 function**

```python
# examples/52_codebuddy_ai_box/pc_tools/protocol_frames.py
"""协议帧构造与解析模块 - 对齐固件 espnow_protocol.h 的 struct 定义"""
import struct

MAGIC = bytes([0xA5, 0x5A])
FRAME_TYPE_AGENT_STATUS = 0x0C
FRAME_TYPE_AGENT_APPROVAL_REQ = 0x0D
FRAME_TYPE_AGENT_APPROVAL_REPLY = 0x0E
AGENT_TASK_MSG_LEN = 48
APPROVAL_TITLE_LEN = 32
APPROVAL_TARGET_LEN = 32
APPROVAL_DIFF_LEN = 40


def crc8(data: bytes) -> int:
    """CRC8 校验 (多项式 0x07, 初值 0x00) - 与固件 espnow_crc8 一致"""
    crc = 0x00
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x80:
                crc = ((crc << 1) ^ 0x07) & 0xFF
            else:
                crc = (crc << 1) & 0xFF
    return crc
```

- [ ] **Step 4: Run test to verify CRC8 passes**

Run: `python -m pytest tests/test_frames.py::test_crc8_empty tests/test_frames.py::test_crc8_single_byte -v`
Expected: 2 PASSED

- [ ] **Step 5: Write failing test for wrap_frame**

```python
# tests/test_frames.py (append)
def test_wrap_frame():
    from protocol_frames import wrap_frame, crc8
    payload = b'\x0C\x01\x00'
    frame = wrap_frame(payload)
    assert frame[0:2] == b'\xA5\x5A'
    assert frame[2] == 3
    assert frame[3:6] == payload
    assert frame[6] == crc8(payload)
    assert len(frame) == 7
```

- [ ] **Step 6: Run test to verify it fails**

Run: `python -m pytest tests/test_frames.py::test_wrap_frame -v`
Expected: `AttributeError: ... has no attribute 'wrap_frame'`

- [ ] **Step 7: Implement wrap_frame**

```python
# protocol_frames.py (append)
def wrap_frame(payload: bytes) -> bytes:
    """封帧：A5 5A <length> <payload> <crc8>"""
    if len(payload) > 255:
        raise ValueError(f"Payload too large: {len(payload)} > 255")
    return MAGIC + bytes([len(payload)]) + payload + bytes([crc8(payload)])
```

- [ ] **Step 8: Run test to verify wrap_frame passes**

Run: `python -m pytest tests/test_frames.py::test_wrap_frame -v`
Expected: PASSED

- [ ] **Step 9: Write failing test for build_status_frame**

```python
# tests/test_frames.py (append)
def test_build_status_frame_minimal():
    from protocol_frames import build_status_frame, crc8
    frame = build_status_frame(state=0x01, seq_num=42, task_message="Analyzing code")
    frame_type, state, seq = struct.unpack_from('<BBB', frame, 0)
    assert frame_type == 0x0C
    assert state == 0x01
    assert seq == 42
    msg_bytes = frame[3:51]
    assert msg_bytes.startswith(b'Analyzing code')
    assert len(msg_bytes) == 48
    assert frame[51] == crc8(frame[:51])
    assert len(frame) == 52

def test_build_status_frame_utf8():
    from protocol_frames import build_status_frame
    frame = build_status_frame(0x02, 10, "正在读取配置文件")
    msg_bytes = frame[3:51]
    assert msg_bytes[:24] == "正在读取配置文件".encode('utf-8')
    long = "A" * 100
    frame = build_status_frame(0x02, 0, long)
    assert len(frame) == 52
```

- [ ] **Step 10: Run test to verify it fails**

Run: `python -m pytest tests/test_frames.py::test_build_status_frame_minimal -v`
Expected: `AttributeError: ... has no attribute 'build_status_frame'`

- [ ] **Step 11: Implement build_status_frame**

```python
# protocol_frames.py (append)
def build_status_frame(state: int, seq_num: int, task_message: str) -> bytes:
    """构造 0x0C Agent Status 帧（52 字节，未封装 A5 5A）"""
    msg_bytes = task_message.encode('utf-8')[:AGENT_TASK_MSG_LEN].ljust(AGENT_TASK_MSG_LEN, b'\x00')
    frame = struct.pack('<BBB48s', FRAME_TYPE_AGENT_STATUS, state, seq_num & 0xFF, msg_bytes)
    frame += bytes([crc8(frame)])
    assert len(frame) == 52
    return frame
```

- [ ] **Step 12: Run test to verify it passes**

Run: `python -m pytest tests/test_frames.py::test_build_status_frame_minimal tests/test_frames.py::test_build_status_frame_utf8 -v`
Expected: 2 PASSED

- [ ] **Step 13: Write failing test for build_approval_frame**

```python
# tests/test_frames.py (append)
def test_build_approval_frame():
    from protocol_frames import build_approval_frame, crc8
    frame = build_approval_frame(1234, 0x02, "Write to main.py", "src/main.py", "+10 -3")
    ft, tid, risk = struct.unpack_from('<BHB', frame, 0)
    assert ft == 0x0D
    assert tid == 1234
    assert risk == 0x02
    assert frame[4:36].startswith(b'Write to main.py')
    assert frame[36:68].startswith(b'src/main.py')
    assert frame[68:108].startswith(b'+10 -3')
    assert frame[108] == crc8(frame[:108])
    assert len(frame) == 109
```

- [ ] **Step 14: Run test to verify it fails**

Run: `python -m pytest tests/test_frames.py::test_build_approval_frame -v`
Expected: `AttributeError`

- [ ] **Step 15: Implement build_approval_frame**

```python
# protocol_frames.py (append)
def build_approval_frame(task_id: int, risk: int, title: str, target: str, diff: str) -> bytes:
    """构造 0x0D Agent Approval Request 帧（109 字节）"""
    t = title.encode('utf-8')[:APPROVAL_TITLE_LEN].ljust(APPROVAL_TITLE_LEN, b'\x00')
    tg = target.encode('utf-8')[:APPROVAL_TARGET_LEN].ljust(APPROVAL_TARGET_LEN, b'\x00')
    d = diff.encode('utf-8')[:APPROVAL_DIFF_LEN].ljust(APPROVAL_DIFF_LEN, b'\x00')
    frame = struct.pack('<BHB32s32s40s', FRAME_TYPE_AGENT_APPROVAL_REQ, task_id & 0xFFFF, risk, t, tg, d)
    frame += bytes([crc8(frame)])
    assert len(frame) == 109
    return frame
```

- [ ] **Step 16: Run test to verify it passes**

Run: `python -m pytest tests/test_frames.py::test_build_approval_frame -v`
Expected: PASSED

- [ ] **Step 17: Write failing test for parse_approval_reply**

```python
# tests/test_frames.py (append)
def test_parse_approval_reply():
    from protocol_frames import parse_approval_reply, crc8
    payload = struct.pack('<BHB', 0x0E, 5678, 0x00)
    frame = payload + bytes([crc8(payload)])
    result = parse_approval_reply(frame)
    assert result == {"task_id": 5678, "action": 0x00}

def test_parse_approval_reply_crc_fail():
    from protocol_frames import parse_approval_reply
    frame = b'\x0E\x2E\x16\x00\xFF'
    assert parse_approval_reply(frame) is None
```

- [ ] **Step 18: Run test to verify it fails**

Run: `python -m pytest tests/test_frames.py::test_parse_approval_reply -v`
Expected: `AttributeError`

- [ ] **Step 19: Implement parse_approval_reply**

```python
# protocol_frames.py (append)
def parse_approval_reply(data: bytes) -> dict | None:
    """解析 0x0E Agent Approval Reply 帧（5 字节）"""
    if len(data) != 5:
        return None
    ft, tid, action, fcrc = struct.unpack('<BHBB', data)
    if ft != FRAME_TYPE_AGENT_APPROVAL_REPLY or crc8(data[:4]) != fcrc:
        return None
    return {"task_id": tid, "action": action}
```

- [ ] **Step 20: Run all Task 1 tests**

Run: `python -m pytest tests/test_frames.py -v`
Expected: 9 PASSED

- [ ] **Step 21: Commit Task 1**

```bash
cd C:/Users/4090/Desktop/dfk10_arduino_demo-master
git add examples/52_codebuddy_ai_box/pc_tools/protocol_frames.py examples/52_codebuddy_ai_box/pc_tools/tests/test_frames.py
git commit -m "feat(52): 协议帧构造模块 (0x0C/0x0D/0x0E + CRC8)

- protocol_frames.py: 严格对齐固件 struct (52/109/5 字节)
- 9 个单元测试全部通过"
```

---

### Task 2: Daemon 串口管理模块

**Files:**
- Create: `examples/52_codebuddy_ai_box/pc_tools/serial_manager.py`
- Create: `examples/52_codebuddy_ai_box/pc_tools/tests/test_serial_manager.py`

**Interfaces:**
- Consumes: `protocol_frames.wrap_frame()`, `protocol_frames.parse_approval_reply()`, `protocol_frames.crc8()`
- Produces:
  - `class SerialManager(port, baudrate=115200, timeout=0.1, auto_reconnect=True)`
  - `def send_frame(payload: bytes) -> bool` — 发送封装帧
  - `def read_reply(timeout_ms: int) -> bytes | None` — 阻塞读回执（去 A5 5A 后的 payload）
  - `def is_connected() -> bool`
  - `def try_reconnect() -> bool` — 5s 间隔重连

Due to file length constraints, I'll write Task 2 through Task 5 in condensed form with key implementation code only. Full step-by-step TDD can be expanded during execution.

- [ ] **Implement serial_manager.py with SerialManager class**

Key methods: `__init__`, `_open`, `send_frame` (wraps with A5 5A), `read_reply` (poll with 10ms sleep, parse A5 5A frames, verify CRC), `try_reconnect` (5s cooldown), `_mark_disconnected`

- [ ] **Write tests/test_serial_manager.py covering init/send/read/reconnect**

7 tests: init states, send when disconnected, send success (mock), read timeout, read valid frame, reconnect cooldown, reconnect success after 5s

- [ ] **Run tests and verify 7 PASSED**

Run: `python -m pytest tests/test_serial_manager.py -v`

- [ ] **Commit Task 2**

```bash
git add examples/52_codebuddy_ai_box/pc_tools/serial_manager.py examples/52_codebuddy_ai_box/pc_tools/tests/test_serial_manager.py
git commit -m "feat(52): 串口管理模块 (断线检测 + 5s 自动重连)

- SerialManager: 封帧发送/读取/CRC 校验/重连
- 7 个单元测试通过"
```

---

### Task 3: Daemon IPC 服务器与状态节流

**Files:**
- Create: `examples/52_codebuddy_ai_box/pc_tools/atkbox_daemon.py`
- Create: `examples/52_codebuddy_ai_box/pc_tools/tests/test_daemon_ipc.py`

**Interfaces:**
- Consumes: `SerialManager`, `protocol_frames.build_status_frame()`
- Produces:
  - `class DaemonServer(ipc_port=47100, serial_port="COM6")`
  - `def start() / stop()` — 启动/停止守护进程
  - `def _handle_status(req: dict) -> dict` — 状态推送 + 1s 节流

- [ ] **Implement atkbox_daemon.py DaemonServer skeleton**

TCP server on 127.0.0.1:47100, threading per connection, JSON request/response, `_handle_status` maps state string to 0x01-0x04, throttles by (state, tool) key with 1.0s window, seq_num counter 0-255

- [ ] **Write tests/test_daemon_ipc.py for server lifecycle and throttling**

4 tests: init, start/stop, throttling logic (3 requests: pass, throttle at 0.5s, pass at 1.5s), real socket IPC

- [ ] **Run tests and verify 4 PASSED**

Run: `python -m pytest tests/test_daemon_ipc.py -v`

- [ ] **Commit Task 3**

```bash
git add examples/52_codebuddy_ai_box/pc_tools/atkbox_daemon.py examples/52_codebuddy_ai_box/pc_tools/tests/test_daemon_ipc.py
git commit -m "feat(52): Daemon IPC 服务器 + 状态推送节流

- DaemonServer: TCP 47100 多线程
- 状态推送 0x0C + 1s 去重
- 4 个集成测试通过"
```

---

### Task 4: 审批请求阻塞等待

**Files:**
- Modify: `examples/52_codebuddy_ai_box/pc_tools/atkbox_daemon.py`
- Create: `examples/52_codebuddy_ai_box/pc_tools/tests/test_daemon_approval.py`

**Interfaces:**
- Consumes: `protocol_frames.build_approval_frame()`, `protocol_frames.parse_approval_reply()`, `SerialManager.read_reply()`
- Produces:
  - `def _handle_approval(req: dict) -> dict` — 发 0x0D → 轮询 0x0E → 返回 allow/deny

- [ ] **Write failing test for approval blocking**

Mock serial to return 0x0E after 2 polling cycles (500ms delay), verify response is `{"ok": True, "decision": "allow"}`

- [ ] **Implement _handle_approval in atkbox_daemon.py**

```python
def _handle_approval(self, req: dict) -> dict:
    tool = req.get("tool", "Write")
    file = req.get("file", "unknown")
    preview = req.get("preview", "")[:200]
    
    # 推断风险等级
    if "/test/" in file or ".test." in file:
        risk = 0x00  # LOW
    elif "/config/" in file or "settings" in file:
        risk = 0x01  # MEDIUM
    else:
        risk = 0x02  # HIGH
    
    # 分配 task_id
    task_id = self._task_id
    self._task_id = (self._task_id + 1) % 65536
    
    # 构造并发送 0x0D
    from protocol_frames import build_approval_frame
    title = f"{tool} to {file.split('/')[-1]}"[:32]
    target = file[-32:] if len(file) > 32 else file
    diff = preview[:40]
    payload = build_approval_frame(task_id, risk, title, target, diff)
    
    if not self.serial_mgr.send_frame(payload):
        return {"ok": False, "error": "Serial send failed"}
    
    logger.info(f"Approval sent: task_id={task_id}, file={file}")
    
    # 阻塞轮询等待 0x0E
    max_polls = 10000  # 理论无限，实际设个大值防挂死
    for _ in range(max_polls):
        reply_payload = self.serial_mgr.read_reply(timeout_ms=250)
        if reply_payload:
            from protocol_frames import parse_approval_reply
            parsed = parse_approval_reply(reply_payload)
            if parsed and parsed["task_id"] == task_id:
                action = parsed["action"]
                if action == 0x00:
                    return {"ok": True, "decision": "allow"}
                elif action == 0x01:
                    return {"ok": True, "decision": "deny"}
                elif action == 0x02:
                    logger.info("User requested diff, continue waiting")
                    continue  # 继续等待下一个 0x0E
                else:
                    return {"ok": True, "decision": "deny"}  # 0xFF 超时视为拒绝
        
        # 检查串口断线
        if not self.serial_mgr.is_connected():
            return {"ok": False, "error": "Serial disconnected"}
    
    return {"ok": False, "error": "Polling limit exceeded"}
```

- [ ] **Write test for approval CRC mismatch / wrong task_id**

Send 0x0E with wrong task_id, verify daemon ignores it and continues polling

- [ ] **Run tests and verify 3 PASSED**

Run: `python -m pytest tests/test_daemon_approval.py -v`

- [ ] **Commit Task 4**

```bash
git add examples/52_codebuddy_ai_box/pc_tools/atkbox_daemon.py examples/52_codebuddy_ai_box/pc_tools/tests/test_daemon_approval.py
git commit -m "feat(52): 审批请求阻塞等待 (0x0D/0x0E)

- _handle_approval: 发送 0x0D, 250ms 轮询 0x0E, 无超时
- 风险等级推断 + task_id 匹配
- 3 个单元测试通过"
```

---

### Task 5: Hook 客户端与配置安装

**Files:**
- Create: `examples/52_codebuddy_ai_box/pc_tools/hook_client.py`
- Create: `examples/52_codebuddy_ai_box/pc_tools/install_hooks.py`
- Modify: `~/.claude/settings.json` (通过 install_hooks.py)

**Interfaces:**
- Consumes: Daemon IPC (TCP 127.0.0.1:47100)
- Produces:
  - `hook_client.py user_prompt|pre_tool|post_tool|stop` — 瘦客户端
  - `install_hooks.py` — 合并 hook 配置到 settings.json

- [ ] **Implement hook_client.py**

```python
#!/usr/bin/env python3
import sys, json, socket

DAEMON_HOST = "127.0.0.1"
DAEMON_PORT = 47100

def send_request(req: dict) -> dict:
    try:
        sock = socket.socket()
        sock.settimeout(5.0)
        sock.connect((DAEMON_HOST, DAEMON_PORT))
        sock.sendall((json.dumps(req) + "\n").encode('utf-8'))
        resp = sock.recv(4096).decode('utf-8').strip()
        sock.close()
        return json.loads(resp)
    except Exception as e:
        return {"ok": False, "error": str(e)}

def main():
    if len(sys.argv) < 2:
        sys.exit(1)
    
    event_type = sys.argv[1]
    hook_input = json.load(sys.stdin)
    
    if event_type == "user_prompt":
        send_request({"type": "status", "state": "THINKING", "detail": hook_input.get("prompt", "")[:50]})
    elif event_type == "pre_tool":
        tool = hook_input["tool_name"]
        if tool in ["Write", "Edit", "NotebookEdit"]:
            tool_input = hook_input.get("tool_input", {})
            file = tool_input.get("file_path") or tool_input.get("notebook_path") or "unknown"
            preview = tool_input.get("content") or tool_input.get("new_source") or ""
            resp = send_request({"type": "approval", "tool": tool, "file": file, "preview": preview[:200]})
            if resp.get("ok") and resp.get("decision") == "allow":
                print(json.dumps({"permissionDecision": "allow"}))
            else:
                print(json.dumps({"permissionDecision": "deny"}))
    elif event_type == "post_tool":
        tool = hook_input["tool_name"]
        ti = hook_input.get("tool_input", {})
        detail = ti.get("file_path") or ti.get("path") or ti.get("pattern") or ti.get("command") or ""
        send_request({"type": "status", "state": "RUNNING", "tool": tool, "detail": detail[:30]})
    elif event_type == "stop":
        send_request({"type": "status", "state": "DONE"})
    sys.exit(0)

if __name__ == "__main__":
    main()
```

- [ ] **Implement install_hooks.py**

```python
#!/usr/bin/env python3
import json
from pathlib import Path

SETTINGS_PATH = Path.home() / ".claude" / "settings.json"
PC_TOOLS_DIR = Path(__file__).parent.absolute()
HOOK_CLIENT = str(PC_TOOLS_DIR / "hook_client.py")

HOOKS_CONFIG = {
    "hooks": {
        "UserPromptSubmit": [{
            "hooks": [{"type": "command", "command": f"python {HOOK_CLIENT} user_prompt"}]
        }],
        "PreToolUse": [{
            "matcher": "Write|Edit|NotebookEdit",
            "hooks": [{"type": "command", "command": f"python {HOOK_CLIENT} pre_tool"}]
        }],
        "PostToolUse": [{
            "matcher": "*",
            "hooks": [{"type": "command", "command": f"python {HOOK_CLIENT} post_tool"}]
        }],
        "Stop": [{
            "hooks": [{"type": "command", "command": f"python {HOOK_CLIENT} stop"}]
        }]
    }
}

def main():
    if SETTINGS_PATH.exists():
        with open(SETTINGS_PATH, 'r', encoding='utf-8') as f:
            settings = json.load(f)
    else:
        settings = {}
    
    # 合并 hooks（保留现有其他字段）
    settings.update(HOOKS_CONFIG)
    
    SETTINGS_PATH.parent.mkdir(parents=True, exist_ok=True)
    with open(SETTINGS_PATH, 'w', encoding='utf-8') as f:
        json.dump(settings, f, indent=2)
    
    print(f"✅ Hooks installed to {SETTINGS_PATH}")
    print(f"   Hook client: {HOOK_CLIENT}")

if __name__ == "__main__":
    main()
```

- [ ] **Test hook_client.py manually**

Start daemon: `python atkbox_daemon.py`
Test: `echo '{"prompt":"test"}' | python hook_client.py user_prompt`
Check daemon log for "Status sent: THINKING"

- [ ] **Run install_hooks.py and verify settings.json**

Run: `python install_hooks.py`
Verify: `cat ~/.claude/settings.json` shows 4 hooks

- [ ] **Commit Task 5**

```bash
git add examples/52_codebuddy_ai_box/pc_tools/hook_client.py examples/52_codebuddy_ai_box/pc_tools/install_hooks.py
git commit -m "feat(52): Hook 客户端 + 配置安装脚本

- hook_client.py: 4 种事件处理 (user_prompt/pre_tool/post_tool/stop)
- install_hooks.py: 合并 hooks 到 ~/.claude/settings.json
- PreToolUse 只拦 Write/Edit/NotebookEdit"
```

---

### Task 6: Daemon 主程序入口与日志

**Files:**
- Modify: `examples/52_codebuddy_ai_box/pc_tools/atkbox_daemon.py`
- Create: `examples/52_codebuddy_ai_box/pc_tools/README_HOOKS.md`

**Interfaces:**
- Consumes: `DaemonServer`
- Produces:
  - `if __name__ == "__main__":` — 命令行入口，argparse 解析参数
  - 日志写入 `~/.claude/atkbox_daemon.log`

- [ ] **Add __main__ block to atkbox_daemon.py**

```python
# atkbox_daemon.py (append at end)
import argparse
import signal

def main():
    parser = argparse.ArgumentParser(description="ATK BOX Daemon for Claude Code Hooks")
    parser.add_argument("--port", default="COM6", help="Serial port (default: COM6)")
    parser.add_argument("--ipc-port", type=int, default=47100, help="IPC TCP port")
    parser.add_argument("--log-level", default="INFO", choices=["DEBUG", "INFO", "WARNING", "ERROR"])
    args = parser.parse_args()
    
    # 配置日志到文件
    log_path = Path.home() / ".claude" / "atkbox_daemon.log"
    log_path.parent.mkdir(parents=True, exist_ok=True)
    file_handler = logging.FileHandler(log_path, encoding='utf-8')
    file_handler.setFormatter(logging.Formatter('[%(asctime)s] [%(levelname)s] %(message)s'))
    logger.addHandler(file_handler)
    logger.setLevel(getattr(logging, args.log_level))
    
    daemon = DaemonServer(ipc_port=args.ipc_port, serial_port=args.port)
    
    # 信号处理（优雅关闭）
    def signal_handler(sig, frame):
        logger.info("Received shutdown signal")
        daemon.stop()
        sys.exit(0)
    
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)
    
    daemon.start()
    logger.info(f"Daemon running (serial: {args.port}, IPC: {args.ipc_port})")
    logger.info(f"Log file: {log_path}")
    
    # 保持运行
    try:
        while daemon.is_running():
            time.sleep(1)
            daemon.serial_mgr.try_reconnect()  # 定期检查重连
    except KeyboardInterrupt:
        pass
    
    daemon.stop()

if __name__ == "__main__":
    main()
```

- [ ] **Write README_HOOKS.md deployment guide**

```markdown
# Claude Code Hook 集成部署指南

## 1. 启动守护进程

**开发调试**（前台运行，Ctrl+C 停止）：
\`\`\`powershell
cd C:\Users\4090\Desktop\dfk10_arduino_demo-master\examples\52_codebuddy_ai_box\pc_tools
python atkbox_daemon.py --port COM6 --ipc-port 47100 --log-level INFO
\`\`\`

**后台运行**（Windows，用 pythonw 或 nssm）：
\`\`\`powershell
pythonw atkbox_daemon.py --port COM6
\`\`\`

**日志位置**：`~/.claude/atkbox_daemon.log`（即 `C:\Users\4090\.claude\atkbox_daemon.log`）

## 2. 注册 Hooks

运行安装脚本：
\`\`\`powershell
python install_hooks.py
\`\`\`

验证：
\`\`\`powershell
cat ~/.claude/settings.json
# 应看到 4 个 hooks: UserPromptSubmit, PreToolUse, PostToolUse, Stop
\`\`\`

## 3. 测试

**测试 1：状态推送**
1. 启动 daemon
2. 在 Claude Code CLI 提交 prompt："Read the README file"
3. 观察 daemon 日志：`Status sent: THINKING ...`
4. 观察 ATK BOX 屏幕：应显示 "THINKING"

**测试 2：审批拦截**
1. 提交 prompt："Add a comment to main.py"
2. Daemon 日志：`Approval sent: task_id=0, file=main.py`
3. ATK BOX 弹出 Screen 8 审批界面
4. 触摸 OK → daemon 日志：`Approval allowed`
5. Claude Code 继续执行 Write 操作

**测试 3：断线重连**
1. 拔掉 Dongle USB
2. Daemon 日志：`Serial marked as disconnected`
3. 5 秒后自动重连：`Attempting to reconnect...`
4. 插回 Dongle，下一次重连应成功

## 4. 故障排查

**Hook 没触发**：
- 检查 `~/.claude/settings.json` 是否包含 hooks 配置
- 在 Claude Code 里输入 `/hooks` 查看已注册列表

**Daemon 连不上串口**：
- 确认 Dongle 在 COM6（设备管理器查看）
- 修改 `--port COM7` 参数

**审批阻塞超时**：
- 检查 ATK BOX 是否正常运行（串口监视器看日志）
- 检查 Dongle 的 ESP-NOW 转发是否工作（LED 应闪烁）

**日志级别调整**：
- 启动时加 `--log-level DEBUG` 查看详细帧收发
\`\`\`

- [ ] **Test daemon startup with --help**

Run: `python atkbox_daemon.py --help`
Verify: shows argparse help

- [ ] **Test daemon runs and writes log**

Run: `python atkbox_daemon.py --port COM999 --log-level DEBUG` (invalid port)
Verify: `~/.claude/atkbox_daemon.log` created with "Serial open failed"

- [ ] **Commit Task 6**

```bash
git add examples/52_codebuddy_ai_box/pc_tools/atkbox_daemon.py examples/52_codebuddy_ai_box/pc_tools/README_HOOKS.md
git commit -m "feat(52): Daemon 主程序入口 + 部署文档

- __main__: argparse + 信号处理 + 日志文件
- README_HOOKS.md: 启动/测试/故障排查指南"
```

---

### Task 7: 端到端集成测试（模拟串口）

**Files:**
- Create: `examples/52_codebuddy_ai_box/pc_tools/tests/test_e2e_mock.py`

**Interfaces:**
- Consumes: `DaemonServer`, `hook_client.py`
- Produces: 端到端测试，用 mock 串口模拟 0x0E 回执

- [ ] **Write E2E test: user prompt → status frame sent**

Mock SerialManager, verify THINKING 0x0C frame sent when hook_client.py user_prompt called

- [ ] **Write E2E test: approval request → blocking wait → allow**

Start daemon with mock serial that returns 0x0E after 500ms, send approval IPC, verify blocks 500ms then returns allow

- [ ] **Write E2E test: approval request → deny**

Mock serial returns 0x0E with action=0x01, verify daemon returns deny

- [ ] **Write E2E test: approval during disconnect**

Mock serial throws exception during read_reply, verify daemon returns {"ok": false, "error": "..."}

- [ ] **Run all E2E tests**

Run: `python -m pytest tests/test_e2e_mock.py -v`
Expected: 4 PASSED

- [ ] **Commit Task 7**

```bash
git add examples/52_codebuddy_ai_box/pc_tools/tests/test_e2e_mock.py
git commit -m "test(52): 端到端集成测试 (mock 串口)

- 4 个场景: 状态推送/审批允许/审批拒绝/断线
- 验证 hook_client.py → daemon → 串口帧完整链路"
```

---

### Task 8: 固件联调验证

**Files:**
- Create: `examples/52_codebuddy_ai_box/pc_tools/manual_test_firmware.py`

**Interfaces:**
- Consumes: 真实串口 + ATK BOX 固件
- Produces: 手动测试脚本，验证固件正确接收 0x0C/0x0D 并回复 0x0E

- [ ] **Write manual_test_firmware.py**

```python
#!/usr/bin/env python3
"""手动测试脚本 - 验证固件联调"""
import sys, time
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))

from serial_manager import SerialManager
from protocol_frames import build_status_frame, build_approval_frame, parse_approval_reply

def test_status_frame(port="COM6"):
    print("=== Test 1: 0x0C Status Frame ===")
    sm = SerialManager(port=port, auto_reconnect=False)
    if not sm.is_connected():
        print("❌ Serial not connected")
        return False
    
    # 发送 THINKING 状态
    payload = build_status_frame(0x01, 42, "Manual test: Thinking")
    if sm.send_frame(payload):
        print("✅ Status frame sent")
        print("   Check ATK BOX screen - should show 'THINKING' state")
        return True
    else:
        print("❌ Send failed")
        return False

def test_approval_frame(port="COM6"):
    print("\n=== Test 2: 0x0D/0x0E Approval ===")
    sm = SerialManager(port=port, auto_reconnect=False)
    if not sm.is_connected():
        print("❌ Serial not connected")
        return False
    
    # 发送审批请求
    payload = build_approval_frame(9999, 0x02, "Write to test.py", "pc_tools/test.py", "+5 -2 lines")
    if not sm.send_frame(payload):
        print("❌ Send failed")
        return False
    
    print("✅ Approval frame sent (task_id=9999)")
    print("   Check ATK BOX - Screen 8 should show approval UI")
    print("   Touch OK to approve, UP to reject")
    print("   Waiting for 0x0E reply (60s timeout)...")
    
    # 阻塞等待 0x0E
    start = time.time()
    while time.time() - start < 60:
        reply = sm.read_reply(timeout_ms=500)
        if reply:
            parsed = parse_approval_reply(reply)
            if parsed and parsed["task_id"] == 9999:
                action = parsed["action"]
                if action == 0x00:
                    print(f"✅ Received APPROVE (elapsed: {time.time()-start:.1f}s)")
                    return True
                elif action == 0x01:
                    print(f"✅ Received REJECT (elapsed: {time.time()-start:.1f}s)")
                    return True
                else:
                    print(f"⚠️  Received action={action:#x}")
            else:
                print(f"⚠️  Mismatched task_id: {parsed}")
    
    print("❌ Timeout - no 0x0E reply received")
    return False

if __name__ == "__main__":
    port = sys.argv[1] if len(sys.argv) > 1 else "COM6"
    print(f"Testing with port: {port}\n")
    
    test_status_frame(port)
    time.sleep(2)
    test_approval_frame(port)
```

- [ ] **Run manual test with real hardware**

Prerequisites:
- ATK BOX flashed with latest firmware
- Dongle connected to PC (COM6)
- ESP-NOW pairing established

Run: `python manual_test_firmware.py COM6`

Expected:
- Test 1: ATK BOX screen shows "THINKING" state
- Test 2: ATK BOX pops Screen 8 approval UI → touch OK → script prints "✅ Received APPROVE"

- [ ] **Verify CRC8 consistency between PC and firmware**

If Test 2 fails, check daemon log for sent frame hex, compare with ATK BOX serial monitor received frame. CRC mismatch indicates protocol bug.

- [ ] **Commit Task 8**

```bash
git add examples/52_codebuddy_ai_box/pc_tools/manual_test_firmware.py
git commit -m "test(52): 固件联调手动测试脚本

- manual_test_firmware.py: 发送 0x0C/0x0D, 等待 0x0E
- 验证 PC↔Dongle↔ATK BOX 完整链路
- CRC8 与固件一致性验证"
```

---

### Task 9: Claude Code 实战测试

**Files:**
- None (实际操作测试)

**Interfaces:**
- Consumes: 完整系统（daemon + hooks + 固件）
- Produces: 验证报告

- [ ] **Prepare test environment**

1. 启动 daemon: `python atkbox_daemon.py --port COM6 --log-level DEBUG`
2. 确认 hooks 已注册: `cat ~/.claude/settings.json | grep UserPromptSubmit`
3. ATK BOX 开机，默认 Screen 5

- [ ] **Test scenario 1: Read file → status display**

In Claude Code CLI:
```
> Read the content of README.md
```

Expected:
- Daemon log: `Status sent: THINKING ...`
- ATK BOX Screen 5: 状态文字更新为 "THINKING"
- Daemon log: `Status sent: RUNNING Read ...`
- ATK BOX: 状态更新为 "RUNNING"
- Claude reads file and responds

- [ ] **Test scenario 2: Write file → approval → allow**

In Claude Code CLI:
```
> Add a comment "# Test hook" to the top of hook_client.py
```

Expected:
- Daemon log: `Approval sent: task_id=0, file=hook_client.py`
- ATK BOX: 弹出 Screen 8 审批界面，显示 "Write to hook_client.py"
- **触摸 OK 按钮**
- Daemon log: `Approval allowed: task_id=0`
- Claude writes the comment
- Verify: `head -1 hook_client.py` shows `# Test hook`

- [ ] **Test scenario 3: Write file → approval → deny**

In Claude Code CLI:
```
> Add another comment to hook_client.py
```

Expected:
- ATK BOX: 再次弹 Screen 8
- **触摸 UP 按钮（拒绝）**
- Daemon log: `Approval rejected: task_id=1`
- Claude receives deny, adjusts approach or asks user

- [ ] **Test scenario 4: Disconnect during approval**

In Claude Code CLI:
```
> Write a new file test_disconnect.py
```

When Screen 8 appears:
- **拔掉 Dongle USB**
- Daemon log: `Serial marked as disconnected`
- Hook returns deny (safe default)
- Claude receives deny

Reconnect:
- Daemon log: `Attempting to reconnect...` (5s later)
- **插回 Dongle**
- Daemon log: `Serial opened: COM6`

- [ ] **Test scenario 5: Throttling**

In Claude Code CLI:
```
> List all Python files in this directory
```

Expected:
- Claude calls `Glob *.py` which triggers PostToolUse hook multiple times rapidly
- Daemon log shows throttling: `Status throttled: RUNNING + Glob`
- Only ~1 frame/second sent, ATK BOX not flooded

- [ ] **Document test results**

Create verification checklist in daemon log or separate file:
- [ ] Scenario 1: Status display ✅/❌
- [ ] Scenario 2: Approval allow ✅/❌
- [ ] Scenario 3: Approval deny ✅/❌
- [ ] Scenario 4: Disconnect handling ✅/❌
- [ ] Scenario 5: Throttling ✅/❌

- [ ] **Commit verification notes**

```bash
git add examples/52_codebuddy_ai_box/pc_tools/VERIFICATION.md
git commit -m "docs(52): Claude Code 实战测试验证清单

- 5 个场景端到端测试通过
- 状态可视化/审批/断线/节流全部工作正常"
```

---

### Task 10: 清理与文档完善

**Files:**
- Delete: `examples/52_codebuddy_ai_box/pc_tools/agent_status_bridge.py` (旧实现，有 bug)
- Modify: `examples/52_codebuddy_ai_box/README.md` 或项目根 README 添加 Hook 集成说明

**Interfaces:**
- Consumes: None
- Produces: 清理后的代码库 + 更新文档

- [ ] **Delete obsolete agent_status_bridge.py**

```bash
git rm examples/52_codebuddy_ai_box/pc_tools/agent_status_bridge.py
git commit -m "refactor(52): 删除旧 agent_status_bridge.py

- 该实现有协议 bug (134字节 vs 固件52字节)
- 已被 atkbox_daemon.py 完全替代"
```

- [ ] **Update project documentation**

Add section to `examples/52_codebuddy_ai_box/README.md` or create `CLAUDE_CODE_INTEGRATION.md`:

```markdown
## Claude Code Hook 集成

ATK BOX 现已支持作为 Claude Code 的物理状态显示器和审批设备。

### 快速开始

1. **启动守护进程**
   \`\`\`bash
   cd pc_tools
   python atkbox_daemon.py
   \`\`\`

2. **注册 Hooks**
   \`\`\`bash
   python install_hooks.py
   \`\`\`

3. **使用 Claude Code**
   - Claude 思考时，ATK BOX 显示 THINKING
   - Claude 执行工具时，显示 RUNNING + 工具名
   - Claude 写文件前，弹审批界面，触摸确认

详细文档见 `pc_tools/README_HOOKS.md`
```

- [ ] **Run final test suite**

Run all tests:
```bash
cd pc_tools
python -m pytest tests/ -v --tb=short
```

Expected: ~25+ tests PASSED

- [ ] **Commit documentation updates**

```bash
git add examples/52_codebuddy_ai_box/README.md
git commit -m "docs(52): 添加 Claude Code Hook 集成说明

- 快速开始指南
- 指向详细文档 README_HOOKS.md"
```

---

## 验收标准

完成所有 10 个 Task 后，系统应满足：

1. **✅ 状态可视化**：Claude Code 的 THINKING/RUNNING/DONE 状态实时显示在 ATK BOX 屏幕
2. **✅ 物理审批**：Write/Edit/NotebookEdit 操作触发 Screen 8 审批界面，触摸确认后 Claude 才继续
3. **✅ 无超时阻塞**：审批请求一直等待用户触摸，不会超时自动放行
4. **✅ 节流优化**：高频工具调用不会淹没 ESP-NOW，1秒内同状态只发一次
5. **✅ 断线重连**：Dongle 拔插后 5 秒自动重连，审批请求断线时返回 deny
6. **✅ 测试覆盖**：单元测试 + 集成测试 + 固件联调 + Claude Code 实战全部通过

---

## 执行选择

计划已完成并保存到 `docs/superpowers/plans/2026-09-03-claude-code-hook-integration.md`。

两种执行方式：

**1. Subagent-Driven (推荐)** — 我为每个 Task 派发一个新 subagent，task 间有 review 检查点，快速迭代

**2. Inline Execution** — 在本会话内用 executing-plans skill 批量执行，阶段性检查点

你选哪个？

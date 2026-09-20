# Claude Code Hook 集成设计规格

**日期**: 2026-09-03  
**项目**: CodeBuddy AI BOX (ATK ESP32-S3 BOX)  
**目标**: 将 ATK BOX 的 Agent 工作流可视化和物理审批功能接入 Claude Code 的 hook 事件系统

---

## 1. 背景与目标

### 1.1 现状

- **固件侧**：ATK BOX 已实现 0x0C（状态推送）、0x0D（审批请求）、0x0E（审批回执）三个协议帧的接收和 UI 展示
  - Screen 8 审批界面支持触摸确认（OK / UP / DOWN 三按钮）
  - espnow_recv_cb 能正确解析帧并更新状态
- **PC 侧**：现有 `agent_status_bridge.py` 采用**轮询 history.jsonl** 的方式猜测状态，事后推送，无法实现真正的阻塞审批
- **协议 bug**：`agent_status_bridge.py` 发送的 0x0C 帧长度为 134 字节，但固件定义的 `agent_status_frame_t` 只有 52 字节，导致帧被固件丢弃

### 1.2 目标

**彻底重构 PC 端集成方案**，从"事后轮询"升级到"事件驱动 + 同步阻塞"：

1. **状态可视化（非阻塞）**  
   - Claude Code 在 THINKING / RUNNING / DONE 状态时，通过 hook 实时推送到 ATK BOX 屏幕
   - 显示当前执行的工具名称（如 "Reading config.py"、"Running tests"）
   - 状态推送做节流（1 秒内同一状态只发一次），避免淹没 ESP-NOW

2. **物理审批（阻塞式）**  
   - Claude Code 执行 Write / Edit / NotebookEdit 工具前，通过 PreToolUse hook **同步拦截**
   - 将操作信息推送到 ATK BOX，弹出 Screen 8 审批界面（文件名 + 风险等级 + 三个触摸按钮）
   - Hook 脚本**阻塞等待用户触摸确认**，无超时限制，直到收到 0x0E 回执或检测到断线
   - 用户触摸 OK → 返回 `allow`，触摸 UP → 返回 `deny`，Claude 据此继续或调整方案

3. **架构优化**  
   - 用守护进程独占串口，hook 脚本通过 TCP socket（本地回环）与守护进程通信
   - 守护进程处理断线重连、状态节流、task_id 分配、审批请求的阻塞等待
   - Hook 脚本是瘦客户端，启动快（<5ms），不维护状态

---

## 2. 整体架构

```
┌─────────────────────────────────────────────────────────────────┐
│  Claude Code (CLI) — 运行在 Windows PowerShell                   │
│  ┌──────────────┬──────────────┬──────────────┬──────────────┐ │
│  │UserPromptSubmit│ PreToolUse   │ PostToolUse  │    Stop      │ │
│  │  (非阻塞)     │ (阻塞·审批)  │  (非阻塞)    │  (非阻塞)    │ │
│  └──────┬───────┴──────┬───────┴──────┬───────┴──────┬───────┘ │
│         │              │              │              │          │
│    每个 hook 调用 hook_client.py <event_type>                    │
└─────────┼──────────────┼──────────────┼──────────────┼──────────┘
          │              │              │              │
          └──────────────┴─── TCP 127.0.0.1:47100 ─────┴─ (JSON 单行)
                              │
                   ┌──────────▼──────────┐
                   │  atkbox_daemon.py    │  常驻守护进程 (Python)
                   │  - 独占串口 COM6     │  - 状态帧节流 (1s去重)
                   │  - 断线重连 (5s重试) │  - 审批阻塞等待 (无超时)
                   │  - task_id 序列分配  │  - 读 0x0E 回执匹配
                   │  - 多线程处理 IPC    │  - 日志写 ~/.claude/
                   └──────────┬──────────┘
                              │ 串口 A5 5A 帧 (115200)
                   ┌──────────▼──────────┐
                   │  Dongle (COM6)       │  USB ↔ ESP-NOW 桥
                   │  pc_link.c 已实现    │  转发 0x0C/0x0D/0x0E
                   └──────────┬──────────┘
                              │ ESP-NOW 2.4GHz
                   ┌──────────▼──────────┐
                   │  ATK BOX (固件已就绪) │  0x0C → 状态显示
                   │  espnow_recv_cb     │  0x0D → Screen 8 审批
                   │  Screen 8 触摸 UI   │  0x0E ← 触摸回执
                   └─────────────────────┘
```

**三个新增 PC 端组件**（固件不动）：

1. **atkbox_daemon.py** — 守护进程，独占串口，处理所有通信
2. **hook_client.py** — 瘦客户端，被 Claude Code hook 调用，连 daemon 发 JSON 请求
3. **.claude/settings.json** — 注册 4 个 hook（UserPromptSubmit / PreToolUse / PostToolUse / Stop）

---

## 3. 状态机设计

### 3.1 ATK BOX 状态机

```
IDLE (空闲，默认 Screen 5 AI Status)
  ↓ 用户提交 prompt (UserPromptSubmit hook)
THINKING (Claude 正在思考，还没调工具)
  ↓ 第一个工具执行完 (PostToolUse hook)
RUNNING (正在执行工具："Reading config.py" / "Running tests" / ...)
  ↓ 遇到需要审批的操作 (PreToolUse hook 拦 Write/Edit/NotebookEdit)
APPROVAL (Screen 8 审批界面，等待触摸确认)
  ↓ 用户触摸 OK/UP → 审批完成
RUNNING (继续工作)
  ↓ 任务完成 (Stop hook)
DONE (显示 "Task completed"，5 秒后回 IDLE)
  ↓ 5 秒定时器
IDLE
```

**状态码映射**（对应固件 `agent_state_t`）：
- IDLE → `AGENT_STATE_READY` (0x00)
- THINKING → `AGENT_STATE_THINKING` (0x01)
- RUNNING → `AGENT_STATE_RUNNING` (0x02)
- DONE → `AGENT_STATE_DONE` (0x03)
- （预留）ERROR → `AGENT_STATE_ERROR` (0x04)，断线或审批被拒后 Claude 卡住时使用

### 3.2 Hook 事件与状态转换

| Hook 事件         | 触发时机                      | Daemon 动作              | ATK BOX 状态    |
|-------------------|-------------------------------|--------------------------|----------------|
| UserPromptSubmit  | 用户提交 prompt               | 发 0x0C (THINKING)       | IDLE → THINKING |
| PostToolUse       | 工具执行完成                  | 发 0x0C (RUNNING + 工具名) | THINKING → RUNNING |
| PreToolUse        | 工具执行前（拦 Write/Edit）   | 发 0x0D (审批请求)，阻塞  | RUNNING → APPROVAL |
| Stop              | Claude 认为任务完成           | 发 0x0C (DONE)           | RUNNING → DONE |

**审批流程特殊处理**：
- PreToolUse hook 遇到 Write/Edit/NotebookEdit 时，发 0x0D 并**阻塞 hook 脚本**
- Daemon 轮询读串口（250ms/次），等 0x0E 回执
- 收到 0x0E 后解除阻塞，hook 返回 `allow` 或 `deny`
- 审批被拒后，不额外发 0x0C 状态帧，等 Claude 的下一个动作触发状态更新

**RUNNING 状态更新语义（消除歧义）**：
- PostToolUse hook 对**每个**工具执行完都触发（`matcher: "*"`），每次都发 0x0C (RUNNING + 当前工具名)
- 屏幕因此实时反映"Claude 刚执行完什么工具"（Read→Grep→Edit 逐个刷新）
- 节流规则（1 秒去重）作用在 `state + tool` 组合上：连续两次相同工具 1 秒内只发一次，不同工具立即发
- THINKING → RUNNING 的转换由第一个 PostToolUse 自然完成，无需特殊判断"是不是第一个工具"

---

## 4. IPC 协议（Hook ↔ Daemon）

### 4.1 传输层

- **协议**: TCP socket，本地回环 `127.0.0.1:47100`
- **格式**: JSON 单行（每条消息以 `\n` 结尾），UTF-8 编码
- **连接模型**: 短连接——hook 脚本连接 → 发请求 → 收响应 → 断开，单次耗时 <10ms（审批除外）
- **并发**: Daemon 用多线程或 select 处理并发 hook 请求，状态推送请求非阻塞返回，审批请求阻塞到收到 0x0E

### 4.2 请求格式（Hook → Daemon）

**状态推送请求**（非阻塞）：
```json
{
  "type": "status",
  "state": "THINKING" | "RUNNING" | "DONE",
  "detail": "Analyzing code structure..."  // 可选，<50 字符
}
```
```json
{
  "type": "status",
  "state": "RUNNING",
  "tool": "Read",  // 工具名称
  "detail": "config.py"  // 目标文件/参数，<30 字符
}
```

**审批请求**（阻塞）：
```json
{
  "type": "approval",
  "tool": "Write" | "Edit" | "NotebookEdit",
  "file": "src/main.py",  // 目标文件路径，取最后 32 字符
  "preview": "def main():\n    print('hello')\n...",  // 前 200 字符预览
  "risk": "high" | "medium" | "low"  // 可选，默认根据文件路径推断
}
```

### 4.3 响应格式（Daemon → Hook）

**成功响应**：
```json
{"ok": true}  // 状态推送成功
```
```json
{"ok": true, "decision": "allow"}   // 审批通过
{"ok": true, "decision": "deny"}    // 审批拒绝
```

**失败响应**：
```json
{"ok": false, "error": "Dongle disconnected"}  // 串口断线
{"ok": false, "error": "Timeout waiting for approval"}  // （理论上不会出现，因为无超时）
```

---

## 5. 串口协议帧格式

### 5.1 协议帧对齐规则

**关键约束**：固件的 `espnow_protocol.h` 定义是权威来源，PC 端必须**字节对齐** packed struct。

**已发现 bug**：现有 `agent_status_bridge.py` 发送的 0x0C 帧长度为 134 字节，但固件定义的 `agent_status_frame_t` 只有 52 字节，导致固件无法接收。

**修复策略**：Daemon 严格按固件 struct 构造帧，长度必须精确匹配。

### 5.2 0x0C Agent Status 帧（状态推送，非阻塞）

**固件定义** (`espnow_protocol.h:222-228`)：
```c
typedef struct __attribute__((packed)) {
    uint8_t  frame_type;               // = 0x0C (FRAME_TYPE_AGENT_STATUS)
    uint8_t  state;                    // agent_state_t: 0x00~0x04
    uint8_t  seq_num;                  // 序列号，递增，用于固件去重
    char     task_message[48];         // UTF-8 任务回显，如 "Reading config.py"
    uint8_t  crc8;                     // CRC8 校验
} agent_status_frame_t;  // 总长 52 字节
```

**Daemon 构造规则**：
1. `frame_type` = 0x0C
2. `state` = 根据 IPC 请求映射（THINKING→0x01, RUNNING→0x02, DONE→0x03）
3. `seq_num` = Daemon 维护全局计数器（0-255 循环）
4. `task_message` = 拼接 `detail` 或 `tool + detail`，UTF-8 编码，**最多 48 字节**，不足补 `\0`
   - RUNNING 状态优先显示工具名：`"Read: config.py"`
   - THINKING 状态显示 prompt 前 48 字符
5. `crc8` = 对前 51 字节计算 CRC8（多项式 0x07，初值 0x00）

**节流规则**：同一 `state` + `tool` 组合，1 秒内只发一次（防止 PostToolUse 高频触发淹没 ESP-NOW）。

### 5.3 0x0D Agent Approval Request 帧（审批请求，阻塞）

**固件定义** (`espnow_protocol.h:242-250`)：
```c
typedef struct __attribute__((packed)) {
    uint8_t  frame_type;               // = 0x0D (FRAME_TYPE_AGENT_APPROVAL_REQ)
    uint16_t task_id;                  // 任务 ID，小端序，用于匹配 0x0E 回执
    uint8_t  risk_level;               // 0=LOW/1=MEDIUM/2=HIGH
    char     title[32];                // "Write to src/main.py"
    char     target[32];               // "src/main.py" (取路径最后 32 字符)
    char     diff_summary[40];         // "+5 -2 lines" 或预览前 40 字符
    uint8_t  crc8;                     // CRC8 校验
} agent_approval_request_frame_t;  // 总长 109 字节
```

**Daemon 构造规则**：
1. `frame_type` = 0x0D
2. `task_id` = Daemon 维护全局计数器（0-65535 循环），每个审批请求分配唯一 ID
3. `risk_level` = 根据 IPC 请求的 `risk` 字段或文件路径推断：
   - 包含 `/test/` 或 `.test.` → LOW (0x00)
   - 包含 `/config/` 或 `settings` → MEDIUM (0x01)
   - 其他 → HIGH (0x02)
4. `title` = `"Write to <file>"`，UTF-8，最多 32 字节
5. `target` = 文件路径最后 32 字节（从右截断，保留文件名）
6. `diff_summary` = IPC 请求的 `preview` 字段前 40 字节
7. `crc8` = 对前 108 字节计算

**阻塞等待逻辑**：
- 发送 0x0D 后，Daemon 进入**轮询读串口模式**：每 250ms 读一次，检查是否收到 0x0E
- **无超时限制**——持续轮询直到收到 0x0E 或检测到串口断线（read 抛 `IOError`/`OSError`）
- 收到 0x0E 后，用 `task_id` 匹配请求，解析 `action` 字段（0x00=approve, 0x01=reject, 0x02=view_diff）
- `action=0x00` → 返回 `{"decision": "allow"}`
- `action=0x01` → 返回 `{"decision": "deny"}`
- `action=0x02` → 视为"查看详情后仍未决定"，**继续等待下一个 0x0E**（用户再次触摸 OK/UP）
- 断线 → 返回 `{"ok": false, "error": "..."}`，hook 默认返回 `deny`（安全侧策略）

### 5.4 0x0E Agent Approval Reply 帧（审批回执，固件→PC）

**固件定义** (`espnow_protocol.h:261-266`)：
```c
typedef struct __attribute__((packed)) {
    uint8_t  frame_type;    // = 0x0E (FRAME_TYPE_AGENT_APPROVAL_REPLY)
    uint16_t task_id;       // 对应 0x0D 的 task_id，小端序
    uint8_t  action;        // 0x00=approve, 0x01=reject, 0x02=view_diff, 0xFF=timeout
    uint8_t  crc8;          // CRC8 校验
} agent_approval_reply_frame_t;  // 总长 5 字节
```

**Daemon 解析规则**：
1. 校验 CRC8，不匹配则丢弃
2. 用 `task_id` 匹配当前阻塞的审批请求（可能有多个并发审批，用 task_id 区分）
3. `action=0x00` → 审批通过
4. `action=0x01` → 审批拒绝
5. `action=0x02` → 用户触摸了"查看详情"，继续等待下一个 0x0E
6. `action=0xFF` → 固件发送的超时标志（理论上不会出现，因为固件没有超时逻辑）

---

## 6. Daemon 实现细节

### 6.1 核心职责

1. **串口管理**
   - 启动时打开串口（COM6, 115200, 8N1）
   - 读超时设置为 100ms（避免阻塞主循环）
   - 检测断线：连续 5 次读超时 + 写失败 → 标记断线，关闭串口
   - 断线后每 5 秒尝试重连，重连成功后重新发送最后一次状态帧

2. **IPC 服务器**
   - 监听 TCP `127.0.0.1:47100`
   - 用 `socket.select()` 或 `asyncio` 处理并发连接
   - 每个连接独立处理：读 JSON 请求 → 处理 → 写 JSON 响应 → 关闭连接
   - 审批请求（`type=approval`）会阻塞该连接的 socket，直到收到 0x0E 或断线

3. **状态帧节流**
   - 维护 `last_status_cache = {"state": None, "tool": None, "timestamp": 0}`
   - 收到状态推送请求时，检查：
     - 如果 `state + tool` 与 cache 一致，且 `time.time() - timestamp < 1.0` → 跳过发送，直接返回 `{"ok": true}`
     - 否则更新 cache，构造 0x0C 帧发送

4. **task_id 序列分配**
   - 维护全局计数器 `next_task_id = 0`（0-65535 循环）
   - 每次发送 0x0D 时递增，写入帧的 `task_id` 字段
   - 收到 0x0E 时用 `task_id` 匹配对应的阻塞请求

5. **日志与诊断**
   - 所有串口收发、IPC 请求、状态转换写 `~/.claude/atkbox_daemon.log`
   - 日志格式：`[2026-09-03 14:32:01.123] [INFO] Sent 0x0C: RUNNING, tool=Read, seq=42`
   - 支持 `SIGUSR1`（Linux）或监听文件 `~/.claude/atkbox_daemon.debug`（Windows）切换日志级别

### 6.2 启动与生命周期

**手动启动**（开发调试）：
```powershell
cd C:\Users\4090\Desktop\dfk10_arduino_demo-master\examples\52_codebuddy_ai_box\pc_tools
python atkbox_daemon.py --port COM6 --ipc-port 47100 --log-level INFO
```

**后台启动**（生产使用）：
- Windows：用 `pythonw` 或 `nssm` 安装为服务
- Linux/Mac：用 `systemd` 或 `launchd` 托管

**优雅关闭**：
- 收到 `SIGTERM` / `SIGINT` 时，关闭所有 socket，发送 0x0C (IDLE) 到 ATK BOX，关闭串口，退出

**崩溃恢复**：
- Daemon 崩溃后，hook 脚本连接失败会返回默认行为（状态推送静默失败，审批请求返回 `deny`）
- 用户需手动重启 Daemon，或配置进程管理器自动重启

---

## 7. Hook 脚本实现

### 7.1 hook_client.py 通用逻辑

**单个脚本，根据命令行参数 `sys.argv[1]` 决定行为**：

```python
#!/usr/bin/env python3
"""
Claude Code Hook Client - 瘦客户端，连 atkbox_daemon 发 IPC 请求
Usage: python hook_client.py <event_type>
  event_type: user_prompt | pre_tool | post_tool | stop
"""
import sys, json, socket

DAEMON_HOST = "127.0.0.1"
DAEMON_PORT = 47100
TIMEOUT = 5.0  # 连接超时（审批请求会阻塞更久，但 socket 本身不超时）

def send_request(request: dict) -> dict:
    """发送 JSON 请求到 daemon，返回响应"""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(TIMEOUT)
        sock.connect((DAEMON_HOST, DAEMON_PORT))
        sock.sendall((json.dumps(request) + "\n").encode("utf-8"))
        response = sock.recv(4096).decode("utf-8").strip()
        sock.close()
        return json.loads(response)
    except Exception as e:
        return {"ok": False, "error": str(e)}

def main():
    if len(sys.argv) < 2:
        print(json.dumps({"error": "Missing event_type argument"}), file=sys.stderr)
        sys.exit(1)
    
    event_type = sys.argv[1]
    hook_input = json.load(sys.stdin)  # Claude Code 通过 stdin 传 JSON
    
    if event_type == "user_prompt":
        prompt = hook_input.get("prompt", "")[:50]
        resp = send_request({"type": "status", "state": "THINKING", "detail": prompt})
        # 非阻塞，不需要输出 JSON，静默退出
        sys.exit(0)
    
    elif event_type == "pre_tool":
        tool_name = hook_input["tool_name"]
        tool_input = hook_input.get("tool_input", {})
        
        # 只拦截 Write / Edit / NotebookEdit
        if tool_name in ["Write", "Edit", "NotebookEdit"]:
            file_path = tool_input.get("file_path", tool_input.get("notebook_path", "unknown"))
            preview = tool_input.get("content", tool_input.get("new_source", ""))[:200]
            
            resp = send_request({
                "type": "approval",
                "tool": tool_name,
                "file": file_path,
                "preview": preview
            })
            
            if resp.get("ok") and resp.get("decision") == "allow":
                print(json.dumps({"permissionDecision": "allow"}))
            else:
                print(json.dumps({"permissionDecision": "deny"}))
        # 其他工具：不输出 JSON，默认 allow
        sys.exit(0)
    
    elif event_type == "post_tool":
        tool_name = hook_input["tool_name"]
        tool_input = hook_input.get("tool_input", {})
        
        # 提取目标文件/参数（优先级：file_path > path > pattern > command）
        detail = (tool_input.get("file_path") or 
                 tool_input.get("path") or 
                 tool_input.get("pattern") or 
                 tool_input.get("command", ""))[:30]
        
        resp = send_request({"type": "status", "state": "RUNNING", "tool": tool_name, "detail": detail})
        sys.exit(0)
    
    elif event_type == "stop":
        resp = send_request({"type": "status", "state": "DONE"})
        sys.exit(0)
    
    else:
        print(json.dumps({"error": f"Unknown event_type: {event_type}"}), file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()
```

**关键点**：
- PreToolUse hook 只对 `Write|Edit|NotebookEdit` 输出 JSON（`permissionDecision`），其他工具静默放行
- 审批请求会阻塞在 `sock.recv()` 直到 daemon 返回结果（daemon 内部轮询等 0x0E）
- 连接失败 / daemon 崩溃 → 审批请求返回 `deny`（安全侧），状态推送静默失败

### 7.2 .claude/settings.json 配置

**settings.json 官方格式**（`matcher` + `hooks` 数组嵌套结构，Windows 路径用正斜杠）：

用户设置文件 `.claude/settings.json` 采用直接格式（无 `{"hooks": {...}}` 外层包裹，那是 plugin hooks.json 专用）。每个事件是数组，数组元素含 `matcher`（工具名正则）和 `hooks`（动作数组）：

```json
{
  "hooks": {
    "UserPromptSubmit": [
      {
        "hooks": [
          {
            "type": "command",
            "command": "python C:/Users/4090/Desktop/dfk10_arduino_demo-master/examples/52_codebuddy_ai_box/pc_tools/hook_client.py user_prompt"
          }
        ]
      }
    ],
    "PreToolUse": [
      {
        "matcher": "Write|Edit|NotebookEdit",
        "hooks": [
          {
            "type": "command",
            "command": "python C:/Users/4090/Desktop/dfk10_arduino_demo-master/examples/52_codebuddy_ai_box/pc_tools/hook_client.py pre_tool"
          }
        ]
      }
    ],
    "PostToolUse": [
      {
        "matcher": "*",
        "hooks": [
          {
            "type": "command",
            "command": "python C:/Users/4090/Desktop/dfk10_arduino_demo-master/examples/52_codebuddy_ai_box/pc_tools/hook_client.py post_tool"
          }
        ]
      }
    ],
    "Stop": [
      {
        "hooks": [
          {
            "type": "command",
            "command": "python C:/Users/4090/Desktop/dfk10_arduino_demo-master/examples/52_codebuddy_ai_box/pc_tools/hook_client.py stop"
          }
        ]
      }
    ]
  }
}
```

**注意事项**：
- `PreToolUse` 用 `matcher: "Write|Edit|NotebookEdit"` 在配置层过滤，只有这三个工具会触发 hook（hook_client.py 里的工具名判断作为二次保险）
- `UserPromptSubmit` / `Stop` 无 `matcher`（这两个事件不针对特定工具）
- `install_hooks.py` 辅助脚本负责把上述配置**合并**进用户现有的 `settings.json`（保留 `enabledPlugins`、`env` 等已有字段，不覆盖）

**注意 settings.json 中 `env.ANTHROPIC_BASE_URL` 已被代理占用**（`http://127.0.0.1:10048`），IPC 端口 47100 与其无冲突。

**测试 hook 注册**：
```powershell
# 在 Claude Code CLI 里输入任意 prompt，观察 daemon 日志是否收到 IPC 请求
# 或用 /hooks 命令查看已注册的 hook 列表
```

---

## 8. 实现计划

### 8.1 前置任务：修复现有协议 bug

**问题**：`agent_status_bridge.py` 的 `build_agent_status_frame` 发送 134 字节帧，但固件定义的 `agent_status_frame_t` 只有 52 字节。

**修复**：
1. 删除 `agent_status_bridge.py`（整体被 Daemon 替代）
2. 在新 `atkbox_daemon.py` 中，严格按 52 字节构造 0x0C 帧
3. 用 `struct.pack` 确保字节对齐：
   ```python
   frame = struct.pack(
       "<BBB48sB",  # <小端, B=uint8, 48s=char[48]
       0x0C,        # frame_type
       state,       # agent_state_t
       seq_num,     # 0-255
       task_message.encode("utf-8").ljust(48, b'\x00'),  # 补齐48字节
       crc8         # 对前51字节计算
   )
   assert len(frame) == 52
   ```

### 8.2 实现阶段（5 个迭代）

**阶段 1：Daemon 基础框架**（1 天）
- [ ] 实现串口打开/关闭、断线检测、5s 重连
- [ ] 实现 TCP IPC 服务器（监听 47100，accept 连接，读 JSON 请求）
- [ ] 实现日志系统（写 `~/.claude/atkbox_daemon.log`）
- [ ] 单元测试：发送模拟 IPC 请求，验证 JSON 解析

**阶段 2：状态帧发送与节流**（0.5 天）
- [ ] 实现 `build_status_frame` 函数（严格 52 字节，包含 CRC8）
- [ ] 实现状态节流逻辑（1 秒去重）
- [ ] 处理 `type=status` IPC 请求，发送 0x0C
- [ ] 集成测试：用 `hook_client.py` 发状态请求，用逻辑分析仪或串口监听工具验证帧格式

**阶段 3：审批请求与阻塞等待**（1 天）
- [ ] 实现 `build_approval_request_frame` 函数（严格 109 字节）
- [ ] 实现 task_id 分配与匹配逻辑
- [ ] 实现审批阻塞等待：发 0x0D → 轮询读串口 250ms → 解析 0x0E → 返回 decision
- [ ] 处理 `type=approval` IPC 请求
- [ ] 单元测试：模拟 0x0E 回执，验证 task_id 匹配和 decision 解析

**阶段 4：Hook 脚本与配置**（0.5 天）
- [ ] 编写 `hook_client.py`（user_prompt / pre_tool / post_tool / stop）
- [ ] 编写 `.claude/settings.json` 配置示例
- [ ] 手动测试：注册 hook，在 Claude Code 里执行操作，观察 daemon 日志

**阶段 5：端到端测试与调优**（1 天）
- [ ] 场景 1：用户提交 prompt → ATK BOX 显示 THINKING
- [ ] 场景 2：Claude 读文件 → ATK BOX 显示 RUNNING + 工具名
- [ ] 场景 3：Claude 写文件 → ATK BOX 弹审批页 → 触摸 OK → Claude 继续
- [ ] 场景 4：审批拒绝 → Claude 收到 deny → 调整方案
- [ ] 场景 5：审批期间断线 → Hook 返回 deny，daemon 进入重连
- [ ] 性能优化：状态帧节流阈值调整、审批轮询间隔优化

---

## 9. 测试与验证

### 9.1 单元测试

**Daemon 核心函数**：
- `build_status_frame(state, seq_num, task_message)` → 验证长度 52 字节、CRC8 正确
- `build_approval_request_frame(task_id, risk, title, target, diff)` → 验证长度 109 字节
- `parse_approval_reply(raw_bytes)` → 验证 task_id 匹配、action 解析

**IPC 协议**：
- 用 Python `socket` 客户端发送各种 JSON 请求，验证 daemon 响应格式

### 9.2 集成测试

**串口回环测试**（无需 Dongle，用 USB 转串口模块短接 RX/TX）：
- Daemon 发送 0x0C/0x0D → 回环接收 → 验证帧完整性

**固件联调**（ATK BOX + Dongle）：
1. 刷新 ATK BOX 固件（确保 `espnow_recv_cb` 正确解析 0x0C/0x0D）
2. 启动 Daemon，手动发 IPC 请求：
   ```python
   import socket, json
   s = socket.socket()
   s.connect(("127.0.0.1", 47100))
   s.sendall(b'{"type":"status","state":"THINKING","detail":"Test"}\n')
   print(s.recv(1024))  # 应返回 {"ok": true}
   ```
3. 观察 ATK BOX 屏幕是否显示 "THINKING"

**Hook 端到端测试**：
1. 注册 4 个 hook 到 `.claude/settings.json`
2. 在 Claude Code CLI 提交 prompt："Read the README file"
   - 预期：ATK BOX 显示 THINKING → RUNNING (Read: README.md)
3. 提交 prompt："Add a new function to main.py"
   - 预期：ATK BOX 弹审批页（Write to main.py）→ 触摸 OK → Claude 继续
4. 审批期间拔掉 Dongle USB
   - 预期：Daemon 检测断线 → hook 返回 deny → Claude 收到拒绝

### 9.3 性能与稳定性

**压力测试**：
- Claude 连续调用 20 个工具（Read/Grep/Write 混合）→ 验证状态节流、无帧丢失
- 审批请求并发测试：同时发 3 个审批请求 → 验证 task_id 匹配无混淆

**长时间运行**：
- Daemon 运行 24 小时，随机断线重连 → 验证内存无泄漏、日志无异常

---

## 10. 安全与边界条件

### 10.1 安全考量

1. **IPC 端口绑定**
   - 只监听 `127.0.0.1`（不绑 `0.0.0.0`），防止局域网其他机器发送恶意请求
   - 不需要认证（本地进程可信）

2. **审批拒绝的安全侧策略**
   - 断线 / daemon 崩溃 / 解析失败 → 默认返回 `deny`
   - 避免"无法联系审批服务就自动批准"的危险行为

3. **CRC8 校验**
   - 所有帧（0x0C/0x0D/0x0E）必须校验 CRC8，不匹配则丢弃
   - 防止串口噪声或 ESP-NOW 丢包导致错误解析

### 10.2 边界条件处理

**Daemon 启动时串口被占用**：
- 启动失败，输出错误日志，退出（不静默失败）
- 提示用户检查其他程序（Arduino IDE / PlatformIO Monitor）是否占用串口

**审批请求期间 ATK BOX 断电**：
- Dongle 仍在线，但 ESP-NOW 无响应
- Daemon 持续轮询，用户重启 ATK BOX 后恢复
- 可选优化：轮询超过 120 秒无响应 → 发 0x0C (ERROR) 提示用户检查设备

**多个 Claude Code 会话同时运行**：
- 只有一个 Daemon 实例，所有会话的 hook 共享 IPC 连接
- 审批请求用 task_id 区分，不会混淆
- 状态推送后发请求覆盖前者（最后一次生效）

**Hook 脚本启动慢（Python 解释器冷启动）**：
- 首次调用 hook 可能耗时 200-300ms（Python 启动 + import）
- 后续调用因 Windows 文件系统缓存加速到 50ms
- 可选优化：用 PyInstaller 打包 `hook_client.py` 为 exe，启动时间降到 <10ms

---

## 11. 未来扩展

### 11.1 短期优化（v1.1）

- **审批详情查看**：用户触摸 "View Diff" → ATK BOX 显示 Screen 9（滚动文本，完整 diff）
- **状态历史记录**：ATK BOX 保留最近 10 条状态记录，支持上下翻页查看
- **声音提示**：审批请求时播放短促提示音（ES8311 speaker 输出）

### 11.2 中期扩展（v2.0）

- **多设备支持**：Daemon 同时管理多个 ATK BOX（不同 MAC 地址），审批请求广播到所有设备
- **Web 控制台**：Daemon 暴露 HTTP API（127.0.0.1:47101），提供 Web UI 查看实时状态、审批历史
- **审批策略配置**：用户可配置"自动批准低风险操作"（test 目录写入、日志文件修改）

### 11.3 长期愿景（v3.0）

- **Claude Desktop 集成**：直接作为 Claude Desktop 的内置功能（无需手动配置 hook）
- **移动端伴侣 App**：手机 App 通过蓝牙或 WiFi 连接 ATK BOX，远程查看状态、审批操作
- **AI 辅助审批**：Daemon 内置轻量级 LLM，预分析操作风险，标注"建议批准/拒绝"理由

---

## 12. 附录

### 12.1 完整文件清单

**PC 端新增**（位于 `examples/52_codebuddy_ai_box/pc_tools/` 目录）：
- `atkbox_daemon.py` — 守护进程主程序（约 500 行）
- `hook_client.py` — Hook 瘦客户端（约 100 行）
- `test_daemon.py` — 单元测试脚本
- `install_hooks.py` — 一键注册 hook 到 `.claude/settings.json` 的辅助脚本（合并配置，不覆盖已有字段）

**配置文件**：
- `.claude/settings.json` — 添加 4 个 hook 配置（UserPromptSubmit / PreToolUse / PostToolUse / Stop）

**文档**：
- `docs/superpowers/specs/2026-09-03-claude-code-hook-integration-design.md` — 本设计文档
- `pc_tools/README_hook_integration.md` — 用户手册（安装、配置、故障排查）

**固件侧无需修改**（已在上一轮实现）：
- `espnow_protocol.h` — 协议定义（0x0C/0x0D/0x0E）
- `51_mic_wifi.ino` — ATK BOX 主程序（`espnow_recv_cb` 接收帧，Screen 8 审批界面）

### 12.2 参考资料

- **Claude Code Hook 官方文档**: [hook-development.md](skills/hook-development/hook-development.md)
- **固件协议定义**: [espnow_protocol.h](examples/52_codebuddy_ai_box/espnow_protocol.h)
- **固件主程序**: [51_mic_wifi.ino](examples/52_codebuddy_ai_box/51_mic_wifi.ino)
- **ESP-NOW 规格**: [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/network/esp_now.html)

---

**设计文档版本**: v1.0  
**作者**: Claude Code (Opus 4.8)  
**审核**: 待用户审阅

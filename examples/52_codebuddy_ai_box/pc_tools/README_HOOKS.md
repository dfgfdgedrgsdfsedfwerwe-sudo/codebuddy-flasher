# Claude Code Hook 集成部署指南

将 ATK BOX 接入 Claude Code 的 hook 事件系统，实现 **Agent 状态实时可视化** 和 **物理触摸审批**。

## 功能概述

| 功能 | 说明 |
|------|------|
| **状态可视化** | Claude 思考/执行工具时，ATK BOX 屏幕实时显示 THINKING/RUNNING/DONE |
| **物理审批** | Claude 执行 Write/Edit/NotebookEdit 前，弹出 Screen 8 审批界面，触摸 OK 才继续 |
| **无超时阻塞** | 审批请求一直等待用户触摸，不会自动放行 |
| **节流优化** | 高频工具调用 1 秒内同状态只发一次，不淹没 ESP-NOW |
| **断线重连** | Dongle 拔插后 5 秒自动重连 |

## 架构

```
Claude Code (hooks)
   │ TCP 127.0.0.1:47100 (JSON 单行)
   ▼
atkbox_daemon.py  ← 独占串口，状态节流，审批阻塞等待
   │ 串口 A5 5A 帧 (115200)
   ▼
Dongle (COM6)  ← USB↔ESP-NOW 桥
   │ ESP-NOW 2.4GHz
   ▼
ATK BOX  ← 0x0C 状态显示 / 0x0D 审批 UI / 0x0E 触摸回执
```

## 组件文件

| 文件 | 作用 |
|------|------|
| `protocol_frames.py` | 协议帧构造/解析（CRC8、0x0C/0x0D/0x0E） |
| `serial_manager.py` | 串口管理（发送、接收、断线重连） |
| `atkbox_daemon.py` | 守护进程主体（IPC 服务器、状态节流、审批阻塞） |
| `hook_client.py` | Hook 瘦客户端（被 Claude Code 调用） |
| `install_hooks.py` | Hook 配置安装脚本 |
| `manual_test_firmware.py` | 硬件联调手动测试 |

## 快速开始

### 1. 启动守护进程

**开发调试**（前台运行，Ctrl+C 停止）：
```powershell
cd C:\Users\4090\Desktop\dfk10_arduino_demo-master\examples\52_codebuddy_ai_box\pc_tools
python atkbox_daemon.py --port COM6 --ipc-port 47100 --log-level INFO
```

**后台运行**（Windows，用 pythonw）：
```powershell
pythonw atkbox_daemon.py --port COM6
```

**日志位置**：`~/.claude/atkbox_daemon.log`（即 `C:\Users\<user>\.claude\atkbox_daemon.log`）

### 2. 注册 Hooks

```powershell
python install_hooks.py
```

验证：
```powershell
cat ~/.claude/settings.json
# 应看到 4 个 hooks: UserPromptSubmit, PreToolUse, PostToolUse, Stop
```

卸载：
```powershell
python install_hooks.py --uninstall
```

### 3. 重启 Claude Code

Hooks 在 Claude Code 启动时加载。修改 settings.json 后需重启才生效。

## 测试

### 硬件联调（无需 Claude Code）

```powershell
python manual_test_firmware.py COM6
```

预期：
- Test 1：ATK BOX 屏幕依次显示 THINKING → RUNNING → DONE
- Test 2：ATK BOX 弹出 Screen 8 审批界面 → 触摸 OK → 脚本打印 "Received APPROVE"

### Claude Code 实战测试

**测试 1：状态推送**
1. 启动 daemon
2. 在 Claude Code 提交 prompt："Read the README file"
3. 观察 daemon 日志：`Status sent: THINKING ...`
4. 观察 ATK BOX 屏幕：状态更新为 THINKING → RUNNING

**测试 2：审批拦截**
1. 提交 prompt："Add a comment to main.py"
2. Daemon 日志：`Approval sent: task_id=0, file=main.py`
3. ATK BOX 弹出 Screen 8 审批界面
4. 触摸 OK → daemon 日志：`Approval allowed`
5. Claude 继续执行 Write 操作

**测试 3：审批拒绝**
1. 再次触发写操作
2. 触摸 UP（拒绝）
3. Daemon 日志：`Approval rejected`
4. Claude 收到 deny，调整方案

## 单元测试

```powershell
cd pc_tools
python -m pytest tests/ -v
```

预期：约 25+ 个测试通过（帧构造、串口管理、IPC、审批、端到端）

## IPC 协议

### 请求格式（Hook → Daemon）

**状态推送**（非阻塞）：
```json
{"type": "status", "state": "THINKING", "detail": "Analyzing..."}
{"type": "status", "state": "RUNNING", "tool": "Read", "detail": "config.py"}
```

**审批请求**（阻塞）：
```json
{"type": "approval", "tool": "Write", "file": "src/main.py", "preview": "def main():..."}
```

### 响应格式（Daemon → Hook）

```json
{"ok": true}                          // 状态推送成功
{"ok": true, "decision": "allow"}     // 审批通过
{"ok": true, "decision": "deny"}      // 审批拒绝
{"ok": false, "error": "..."}         // 失败
```

## 串口帧格式

外层包装：`A5 5A <length> <payload> <outer_crc8>`

| 帧类型 | 长度 | 说明 |
|--------|------|------|
| 0x0C Status | 52 字节 | frame_type + state + seq_num + task_message[48] + crc8 |
| 0x0D Approval Req | 109 字节 | frame_type + task_id + risk + title[32] + target[32] + diff[40] + crc8 |
| 0x0E Approval Reply | 5 字节 | frame_type + task_id + action + crc8 |

CRC8：多项式 0x07，初值 0x00（与固件 `espnow_crc8` 一致）

## 故障排查

**Hook 没触发**
- 检查 `~/.claude/settings.json` 是否包含 hooks 配置
- 在 Claude Code 里输入 `/hooks` 查看已注册列表
- 确认重启了 Claude Code

**Daemon 连不上串口**
- 确认 Dongle 在 COM6（设备管理器查看）
- 修改 `--port COM7` 参数
- 确认没有其他程序占用串口（如 realtime_monitor.py）

**审批阻塞不返回**
- 检查 ATK BOX 是否正常运行（串口监视器看日志）
- 检查 Dongle 的 ESP-NOW 转发是否工作（LED 应闪烁）
- 检查 daemon 日志的 `Approval sent` 后是否卡住

**帧校验失败**
- daemon 日志加 `--log-level DEBUG` 查看帧 hex
- 对比 ATK BOX 串口监视器接收的帧
- CRC 不匹配说明协议 bug（检查 protocol_frames.py 与固件 struct 对齐）

**IPC 端口冲突**
- 47100 与现有代理端口 10048 无冲突
- 如仍冲突，改 `--ipc-port` 参数（同时改 hook_client.py 的 DAEMON_PORT）

## 已知限制

- **单机审批**：daemon 一次只处理一个审批请求（阻塞式），并发审批需扩展
- **无持久化**：daemon 重启后 task_id 从 0 重新计数
- **手动启动**：daemon 需手动启动，未做开机自启（可用 nssm 配置为服务）

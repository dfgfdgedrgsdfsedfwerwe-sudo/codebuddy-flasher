# ATK BOX Prompt 优化测试

## 配置内容

已在 `~/.claude/CLAUDE.md` 添加全局指令，让 Claude 在遇到多选决策时自动同步到 ATK BOX 硬件显示。

### 触发条件
- ✅ 2-4 个离散选项的多选决策
- ❌ 开放式文本输入
- ❌ 单选项 Yes/No 问题
- ❌ 超过 4 个选项的问题

### 实现方式
当 Claude 调用 `AskUserQuestion` 且符合条件时，会**同时调用** `mcp__atkbox__ask_on_atkbox`，实现 PC + 硬件双界面。

## 测试步骤

### 前提条件
1. ATK BOX 硬件已连接 (COM11/COM12)
2. Dongle 已连接 (COM6)
3. `atkbox_daemon.py` 正在运行:
   ```bash
   cd C:\Users\4090\Desktop\dfk10_arduino_demo-master\examples\52_codebuddy_ai_box\pc_tools
   python atkbox_daemon.py COM6
   ```

### 测试用例

#### 用例 1: 应该触发硬件显示（3 选项）
**提示词**:
```
我在开发一个 Web 应用，数据库应该选哪个？MySQL、PostgreSQL 还是 MongoDB？
```

**预期行为**:
- ✅ PC 端显示 `AskUserQuestion` 对话框
- ✅ ATK BOX 同时显示触摸选择界面
- ✅ 用户可以在任一界面选择

#### 用例 2: 应该触发硬件显示（2 选项）
**提示词**:
```
这个 bug 修复应该立即部署还是等下次发版？
```

**预期行为**:
- ✅ PC + ATK BOX 双界面
- ✅ 选项: "立即部署" / "等下次发版"

#### 用例 3: 不应触发硬件（开放式问题）
**提示词**:
```
你觉得这个函数应该叫什么名字？
```

**预期行为**:
- ✅ 仅 PC 端文本输入框
- ❌ ATK BOX 不显示（无法在触摸屏输入任意文本）

#### 用例 4: 不应触发硬件（超过 4 选项）
**提示词**:
```
选择编程语言: Python、Java、C++、Go、Rust、JavaScript
```

**预期行为**:
- ✅ 仅 PC 端选择
- ❌ ATK BOX 不显示（硬件限制最多 4 选项）

## 验证方法

### 1. 检查 Claude 行为
在 Claude Code 会话中观察工具调用:
```
[Tool Call] AskUserQuestion(...)          ← PC 端对话框
[Tool Call] mcp__atkbox__ask_on_atkbox(...) ← 硬件同步
```

### 2. 检查 daemon 日志
`atkbox_daemon.py` 终端应输出:
```
[IPC] Client connected
[IPC] Received: {"type": "decision", "title": "...", "options": [...]}
[Decision] Sending to ATK BOX: ...
```

### 3. 检查 ATK BOX 串口
COM12 串口监控应显示:
```
[Decision] Showing screen with 3 options
Touch at (120, 160) -> Option 1
[ESP-NOW] Sending reply frame 0x0B
```

## 故障排查

### 问题: Claude 不调用硬件工具
**原因**: 
- 问题不是多选类型
- 选项数量不在 2-4 范围
- MCP 服务器未正确加载

**解决**: 
```bash
# 验证 MCP 配置
cat ~/.claude/mcp_servers.json

# 重启 Claude Code 以重新加载配置
```

### 问题: MCP 工具调用失败
**原因**: daemon 未运行或端口冲突

**解决**:
```bash
# 检查 daemon 是否运行
netstat -an | grep 47100

# 重启 daemon
python atkbox_daemon.py COM6
```

### 问题: 硬件不显示但 daemon 收到请求
**原因**: ESP-NOW 通信失败

**解决**:
1. 检查 Dongle MAC 地址是否正确 (`e0:72:a1:d4:8f:e0`)
2. 监控 COM12 查看 ATK BOX 是否收到 0x0A 帧
3. 验证 CRC8 计算正确

## 预期效果

配置成功后，开发 ATK BOX 相关功能时:
- 所有技术决策（数据库选型、架构方案、配置选择等）会自动在硬件上同步显示
- 用户可以选择在熟悉的 PC 界面操作，或者体验触摸硬件交互
- daemon 连接失败时优雅降级到 PC-only 模式，不阻塞对话

## 成功标志

✅ 在新会话中问 Claude 一个 2-4 选项的问题  
✅ Claude 自动调用两个工具（AskUserQuestion + mcp__atkbox__ask_on_atkbox）  
✅ ATK BOX 屏幕同步显示选项  
✅ 在硬件上触摸选择后，PC 端 Claude 收到答案并继续执行

---
**配置文件**: `C:\Users\4090\.claude\CLAUDE.md`  
**MCP 服务器**: `C:\Users\4090\.claude\mcp_servers.json`  
**创建时间**: 2026-09-11

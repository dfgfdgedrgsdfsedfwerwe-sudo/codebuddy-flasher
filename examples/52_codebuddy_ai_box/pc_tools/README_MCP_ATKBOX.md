# ATK BOX MCP Server - 主动决策推送工具

让 Claude 在任何会话中主动推送决策选项到 AI BOX 硬件，用户触摸选择后返回结果。

---

## 功能

### MCP 工具：`ask_on_atkbox`

**作用**：在 AI BOX 上显示决策提示，等待用户物理触摸选择。

**参数**：
- `title` (string, 必需) - 问题标题，最多 31 字符（建议英文）
- `options` (array, 必需) - 选项列表，1-4 个，每个最多 23 字符

**返回**：
- 成功：`"User chose option 0: Option A"`
- 超时/取消：`"User cancelled or timeout"`
- 错误：`"Error: daemon not running"`

**阻塞行为**：工具会阻塞最多 40 秒等待用户选择。

---

## 安装步骤

### 1. 确保 CodeBuddyBridge 正在运行

MCP server 需要通过 daemon (127.0.0.1:47100) 连接 AI BOX。

启动 `CodeBuddyBridge.exe` 并点击"启动服务"。

### 2. 配置 MCP Server 到 Claude Code

**方法 A：手动编辑配置文件**

打开/创建 `~/.claude/mcp_servers.json`（Windows: `C:\Users\<你的用户名>\.claude\mcp_servers.json`）

添加以下配置：

```json
{
  "atkbox": {
    "command": "python",
    "args": [
      "C:/Users/4090/Desktop/dfk10_arduino_demo-master/examples/52_codebuddy_ai_box/pc_tools/mcp_atkbox_server.py"
    ],
    "type": "stdio"
  }
}
```

**注意**：
- 修改 `args` 中的路径为你电脑上的实际路径
- 使用正斜杠 `/` 或双反斜杠 `\\`
- 确保 Python 在 PATH 中（或使用完整 Python 路径）

**方法 B：使用快速安装脚本**

运行：
```bash
python install_mcp_atkbox.py
```

它会自动生成正确的配置并写入 `~/.claude/mcp_servers.json`。

### 3. 重启 Claude Code

关闭所有 Claude Code 窗口，重新启动。

### 4. 验证安装

启动 Claude Code 后，输入：
```
/tools list
```

如果看到 `ask_on_atkbox` 工具，说明安装成功。

---

## 使用示例

### 示例 1：简单选择

**你说**：
> 我有两个选项不知道选哪个：保存当前进度还是重新开始。帮我在 AI BOX 上显示让我选。

**Claude 调用**：
```json
{
  "tool": "ask_on_atkbox",
  "arguments": {
    "title": "Choose action",
    "options": ["Save Progress", "Restart"]
  }
}
```

**AI BOX 显示**：
```
┌───────────────────────┐
│ Choose action         │
├───────────────────────┤
│ > Save Progress       │
│   Restart             │
└───────────────────────┘
```

你触摸选择后，Claude 收到：`"User chose option 0: Save Progress"`

### 示例 2：游戏设计选择

**你说**：
> 我们一起设计一个游戏，先让我选游戏类型。

**Claude 调用**：
```json
{
  "tool": "ask_on_atkbox",
  "arguments": {
    "title": "Game Type?",
    "options": ["Snake", "2048", "Brick Breaker", "Tic-Tac-Toe"]
  }
}
```

### 示例 3：代码审批（自定义）

**你说**：
> 分析这段代码并问我是否要优化。

**Claude 分析后调用**：
```json
{
  "tool": "ask_on_atkbox",
  "arguments": {
    "title": "Optimize code?",
    "options": ["Yes, optimize", "No, keep as-is", "Show diff first"]
  }
}
```

---

## 工作原理

```
Claude (ask_on_atkbox 工具)
    ↓ MCP stdio 协议
mcp_atkbox_server.py
    ↓ IPC (TCP 127.0.0.1:47100)
CodeBuddyBridge daemon
    ↓ 串口 + 协议帧 (0x0A)
Dongle (ESP-NOW 转发)
    ↓
AI BOX (Screen 7 决策界面)
    ↓ 用户触摸选择
0x0B 回执帧原路返回
    ↓
Claude 收到结果
```

---

## 限制与注意事项

### 技术限制
- **最多 4 个选项**（固件限制）
- **标题 31 字符，选项 23 字符**（协议帧大小限制）
- **阻塞 40 秒**：调用后 Claude 会等待，期间无法响应其他操作
- **需要 daemon 运行**：CodeBuddyBridge.exe 必须在后台运行

### 使用建议
- **标题简洁**：`"Choose:"` 比 `"Please select one of the following options:"` 更好
- **选项简短**：`"Yes"` / `"No"` / `"Cancel"` 比长句子更清晰
- **英文优先**：中文会占用更多字节，容易超长
- **4 选项以内**：如果有更多选择，考虑分批提问
- **超时处理**：如果用户 30 秒没选择，AI BOX 会超时返回 255

### 调试
- **daemon 日志**：`~/.claude/atkbox_daemon.log` 或 CodeBuddyBridge GUI 日志窗口
- **MCP stderr**：`~/.claude/logs/mcp_atkbox.log`（如果配置了日志）
- **测试命令**：`python send_decision.py "Test" "A" "B"` 直接测试推送

---

## 故障排除

### "Error: [Errno 10061] Connection refused"

**原因**：CodeBuddyBridge daemon 没运行或端口不是 47100

**解决**：
1. 打开 CodeBuddyBridge.exe
2. 点"启动服务"
3. 确认状态显示"IPC: ● 监听中"（绿色）

### "Error: Timeout waiting for user decision"

**原因**：用户在 AI BOX 上 40 秒内没有选择

**解决**：AI BOX 已超时返回 255，Claude 会告诉你超时了。重新调用工具即可。

### "User cancelled or timeout"

**正常行为**：用户主动取消或超时，不是错误。

### 工具没出现在 /tools list

**原因**：
1. mcp_servers.json 配置错误（语法、路径）
2. Claude Code 没重启
3. Python 路径不对

**解决**：
1. 检查 `~/.claude/mcp_servers.json` 语法是否正确（用 JSON 校验器）
2. 检查 `args` 路径是否存在：`python <那个路径>` 能否运行
3. 完全退出 Claude Code，重新启动

---

## 高级：自定义应用

### 游戏循环

```python
# 伪代码示例
while True:
    result = ask_on_atkbox("Your turn", ["Move Up", "Move Down", "Exit"])
    if result.chosen == 2:  # Exit
        break
    # 处理移动...
```

### 多步决策流程

```
Claude 问你问题 A → 你在 AI BOX 选 → Claude 根据你的选择问问题 B → ...
```

### 与其他工具组合

```
Claude 分析代码 (Read) → 发现问题 → 问你是否修复 (ask_on_atkbox) → 你选 Yes → 执行修复 (Write)
```

---

## 与 AskUserQuestion 的对比

| 特性 | AskUserQuestion (内置) | ask_on_atkbox (MCP) |
|------|----------------------|---------------------|
| 显示位置 | CLI 终端（键盘操作） | AI BOX 硬件（触摸屏） |
| 选项数量 | 无限制 | 最多 4 个 |
| 能否被 hook 捕获 | ❌ 否 | N/A（主动调用） |
| 阻塞时间 | 无限制 | 40 秒超时 |
| 物理交互 | ❌ 否 | ✅ 是 |
| 需要额外硬件 | ❌ 否 | ✅ 是（AI BOX） |

**使用场景**：
- **复杂选择、长列表** → 用 AskUserQuestion（CLI 键盘操作更方便）
- **快速确认、物理触摸体验** → 用 ask_on_atkbox（硬件酷炫，适合演示/简单选择）

---

## 文件清单

- `mcp_atkbox_server.py` - MCP server 主程序
- `install_mcp_atkbox.py` - 快速安装脚本
- `README_MCP_ATKBOX.md` - 本文档
- `send_decision.py` - 命令行测试工具（绕过 MCP 直接测试）

---

**享受用 AI BOX 做决策的乐趣！** 🎮

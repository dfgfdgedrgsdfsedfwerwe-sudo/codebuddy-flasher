# ATK BOX MCP 决策推送 - 测试步骤

**目标**：验证 Claude 能通过 `ask_on_atkbox` 工具主动推送选项到 AI BOX，你触摸选择后结果返回给 Claude。

---

## 一、测试前准备（检查清单）

在开始前，逐项确认：

- [ ] **AI BOX 已开机**，屏幕正常显示
- [ ] **Dongle 已插入电脑**（USB）
- [ ] **CodeBuddyBridge.exe 已打开**
- [ ] CodeBuddyBridge 状态面板显示：
  - [ ] 串口: ● 已连接（绿色）
  - [ ] IPC: ● 监听中（绿色）
- [ ] 如果串口是灰色/红色，点"停止服务"再"启动服务"重连

---

## 二、独立测试（不依赖 Claude，先验证硬件链路）

**目的**：确认 daemon → AI BOX 的决策推送链路正常，排除硬件问题。

### 步骤

1. 打开一个新的终端（PowerShell 或 CMD）

2. 运行测试命令：
   ```
   cd C:\Users\4090\Desktop\dfk10_arduino_demo-master\examples\52_codebuddy_ai_box\pc_tools
   python send_decision.py "Test Menu" "Apple" "Banana" "Orange"
   ```

3. **观察 AI BOX**：
   - 屏幕应切换到 Screen 7（决策界面）
   - 显示标题 "Test Menu"
   - 显示 3 个选项：Apple / Banana / Orange

4. **在 AI BOX 上触摸选择**任意一个选项

5. **观察终端输出**：
   ```
   User chose: 1 - Banana
   ```
   （数字对应你选的选项索引）

### 结果判断

| 现象 | 说明 | 下一步 |
|------|------|--------|
| ✅ AI BOX 显示选项，触摸后终端返回结果 | 硬件链路正常 | 进入第三步 MCP 测试 |
| ❌ AI BOX 没反应 | 串口/ESP-NOW 问题 | 检查 CodeBuddyBridge 串口状态，重启服务 |
| ❌ 报错 "Connection refused" | daemon 没运行 | 打开 CodeBuddyBridge 点"启动服务" |
| ❌ 报错 "Timeout" | 40 秒没触摸 | 重新运行，及时触摸 |

**只有独立测试通过，才继续 MCP 测试。**

---

## 三、MCP 工具测试（核心测试）

**目的**：验证 Claude 能调用 `ask_on_atkbox` 工具。

### 步骤 1：完全重启 Claude Code

⚠️ **关键**：MCP 配置只在启动时加载。必须完全重启。

1. **退出所有 Claude Code 窗口**
   - 在每个 CLI 窗口输入 `exit` 或按 Ctrl+C 两次
   - 确保没有残留的 claude 进程

2. **重新启动 Claude Code CLI**
   ```
   cd C:\Users\4090\Desktop\dfk10_arduino_demo-master
   claude
   ```

### 步骤 2：验证工具已加载

在 Claude Code 里输入：
```
/mcp
```
或
```
/tools
```

**期望看到**：列表中有 `atkbox` server 和 `ask_on_atkbox` 工具。

| 现象 | 说明 |
|------|------|
| ✅ 看到 ask_on_atkbox | MCP 配置成功，继续步骤 3 |
| ❌ 没看到 | 见下方"故障排查" |

### 步骤 3：让 Claude 调用工具

对 Claude 说（任选一句）：

**测试语句 1（直接要求）**：
```
用 ask_on_atkbox 工具问我：晚饭吃什么？选项：火锅、烧烤、快餐
```

**测试语句 2（自然对话）**：
```
我在纠结晚饭吃什么，帮我在 AI BOX 上列几个选项让我选。
```

**测试语句 3（游戏场景）**：
```
我们玩个游戏，先在 AI BOX 上让我选难度：简单、普通、困难。
```

### 步骤 4：在 AI BOX 上选择

1. Claude 调用工具后，**AI BOX 自动显示选项**
2. 你**触摸选择**一个
3. **Claude 收到你的选择**，并根据结果继续对话

### 预期完整流程

```
你: 用 ask_on_atkbox 问我晚饭吃什么，选项：火锅、烧烤、快餐

Claude: [调用 ask_on_atkbox 工具，显示 "正在等待你在 AI BOX 上选择..."]

[AI BOX Screen 7 显示]
┌─────────────────┐
│ 晚饭吃什么?      │
│ > 火锅          │
│   烧烤          │
│   快餐          │
└─────────────────┘

[你触摸"烧烤"]

Claude: 你选择了烧烤！那我推荐...
```

---

## 四、进阶测试（可选）

### 测试 A：多轮决策

```
我们设计一个游戏。第一步先让我在 AI BOX 上选游戏类型（贪吃蛇/2048/井字棋），
选完后根据我的选择，再让我选难度。
```

验证 Claude 能连续多次调用工具，形成交互流程。

### 测试 B：4 选项上限

```
在 AI BOX 上让我选一个方向：上、下、左、右
```

验证 4 个选项（最大数量）正常显示。

### 测试 C：超时行为

```
在 AI BOX 上问我一个问题，选项随便给两个
```

然后**故意不触摸**，等 40 秒。验证超时后 Claude 收到"用户取消或超时"。

---

## 五、故障排查

### 问题 1：`/mcp` 或 `/tools` 看不到 ask_on_atkbox

**检查配置文件**：
```
cat C:\Users\4090\.claude\mcp_servers.json
```
应该包含：
```json
{
  "atkbox": {
    "command": "python",
    "args": ["C:/Users/4090/Desktop/.../mcp_atkbox_server.py"],
    "type": "stdio"
  }
}
```

**如果配置缺失**，重新运行：
```
cd C:\Users\4090\Desktop\dfk10_arduino_demo-master\examples\52_codebuddy_ai_box\pc_tools
python install_mcp_atkbox.py
```
然后**再次完全重启 Claude Code**。

**如果配置存在但工具没加载**：
- 检查 Python 路径：终端运行 `python --version` 确认 python 命令可用
- 检查脚本路径：确认 mcp_atkbox_server.py 文件存在
- 手动测试 server：
  ```
  echo {"jsonrpc":"2.0","id":1,"method":"initialize","params":{}} | python mcp_atkbox_server.py
  ```
  应输出一行 JSON（包含 "atkbox"）

### 问题 2：工具调用后 AI BOX 没反应

**原因**：daemon 没运行或串口断开

**解决**：
1. 检查 CodeBuddyBridge 状态面板串口是否绿色
2. 先用第二步的独立测试（send_decision.py）验证
3. 如果独立测试也失败，是硬件/串口问题，不是 MCP 问题

### 问题 3：工具调用报 "Connection refused"

**原因**：CodeBuddyBridge daemon 没在 47100 端口监听

**解决**：
1. 打开 CodeBuddyBridge.exe
2. 点"启动服务"
3. 确认 IPC 状态变绿

### 问题 4：Claude 一直等待，不返回

**正常**：工具会阻塞最多 40 秒等你触摸。及时在 AI BOX 上选择即可。

---

## 六、测试结果记录

测试完成后，记录结果：

| 测试项 | 结果 | 备注 |
|--------|------|------|
| 独立测试（send_decision.py） | ⬜ 通过 / ⬜ 失败 | |
| /mcp 看到工具 | ⬜ 是 / ⬜ 否 | |
| Claude 调用工具 | ⬜ 成功 / ⬜ 失败 | |
| AI BOX 显示选项 | ⬜ 是 / ⬜ 否 | |
| 触摸选择返回结果 | ⬜ 是 / ⬜ 否 | |
| 多轮决策 | ⬜ 通过 / ⬜ 失败 | |

---

## 快速命令参考

```powershell
# 进入工作目录
cd C:\Users\4090\Desktop\dfk10_arduino_demo-master\examples\52_codebuddy_ai_box\pc_tools

# 独立测试决策推送
python send_decision.py "标题" "选项1" "选项2" "选项3"

# 重新安装 MCP 配置
python install_mcp_atkbox.py

# 卸载 MCP 配置
python install_mcp_atkbox.py --uninstall

# 手动测试 MCP server 协议
echo {"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}} | python mcp_atkbox_server.py

# 查看 daemon 日志
type C:\Users\4090\.claude\atkbox_daemon.log
```

---

## 关键提醒

1. **三个东西必须同时运行**：AI BOX 开机 + Dongle 插着 + CodeBuddyBridge 启动服务
2. **改了 MCP 配置必须完全重启 Claude Code**（不是新开窗口，是完全退出再启动）
3. **先做独立测试**（send_decision.py），排除硬件问题后再测 MCP
4. **英文标题/选项更稳**（中文占字节多，容易超长被截断）
5. **最多 4 个选项**

准备好了就开始测试吧！有问题随时问我。

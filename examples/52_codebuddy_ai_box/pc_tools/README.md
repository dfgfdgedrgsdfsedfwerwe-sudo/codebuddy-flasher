# CodeBuddy AI BOX PC 端工具使用指南

**版本**: 1.1  
**日期**: 2026-09-03

---

## 📦 工具包内容

```
pc_tools/
├── codebuddy_bridge_gui.py    # 【推荐】GUI 主程序（一键安装+配置）
├── atkbox_daemon.py           # 守护进程（IPC + 串口桥接）
├── mcp_atkbox_server.py       # MCP Server（Claude Code 工具集成）
├── install_mcp_atkbox.py      # MCP 自动安装脚本
├── install_claude_md.py       # 全局指令自动安装脚本
├── realtime_monitor.py        # 实时状态监控（Token/Git/AI情绪）
├── realtime_monitor.bat       # 监控启动脚本（Windows，推荐，避免中文乱码）
├── ask_user_via_box.py        # 决策交互工具
├── ask_user_via_box.bat       # 决策启动脚本（Windows，推荐，避免中文乱码）
├── agent_status_bridge.py     # 【新】Agent 工作流可视化桥接
├── agent_status_bridge.bat    # 【新】Agent 桥接启动脚本（Windows）
├── README.md                  # 本文档
└── DEPLOYMENT_GUIDE.md        # 【新】新电脑快速部署指南
```

**⚠️ Windows 中文显示说明**：
- 直接运行 `.py` 脚本可能出现中文乱码
- **推荐使用 `.bat` 脚本启动**（已配置 UTF-8 编码）
- 用法与 `.py` 完全相同，只需把文件名改为 `.bat` 即可

---

## 🔧 环境要求

### 硬件
- **AI BOX**：正点原子 ESP32-S3 BOX（已烧录 52_codebuddy_ai_box 固件）
- **Dongle**：ESP32-S3 Dongle（已烧录 dongle_firmware 固件）
- **连接**：
  - Dongle USB 连接到 PC（串口 COM6，Windows）
  - AI BOX 与 Dongle 通过 ESP-NOW 无线通信（2.4GHz）

### 软件
- Python 3.7+
- 依赖库：`pip install pyserial`

### 端口确认
```bash
# Windows: 查看串口
python -m platformio device list

# 应该看到:
# COM6 - USB Serial Port (Dongle)
```

---

## 🖥️ 换电脑？一键部署（GUI）

在新电脑上使用，最简单的方式是用 GUI 一键安装：

```bash
cd pc_tools
python codebuddy_bridge_gui.py
```

然后依次点击：
1. **扫描** → 选 Dongle 串口（COM6）
2. **安装 MCP** → 注册 `ask_on_atkbox` 工具到 Claude Code
3. **安装全局指令** → 让 Claude 在 2-4 选项决策时自动同步到硬件
4. **应用设置** → **启动服务**
5. 完全重启 Claude Code（`exit` 后重新 `claude`）

详细步骤（含命令行方式、故障排查）见 **`DEPLOYMENT_GUIDE.md`**。

---

## 🚀 快速开始

### 1. 实时监控（状态显示）

**功能**：每隔几秒自动推送 Token 用量、Git 项目状态、AI 情绪到 AI BOX 屏幕。

**两种模式**：

#### 模式 A：多窗口自动检测（推荐）

自动检测所有活跃 Claude Code 窗口，聚合显示：

```bash
# Windows 推荐
cd pc_tools
realtime_monitor.bat COM6 --auto [更新间隔秒数]

# Python 直接调用
python realtime_monitor.py COM6 --auto 5
```

**效果**：
- **Screen 1**: Token 用量（所有窗口共享，从 history.jsonl 估算）
- **Screen 2**: 每个活跃窗口的项目状态（最多显示 6 个）
  - 示例：`dongle_firmware` → Coding, `52_codebuddy_ai_box` → Review
- **Screen 5**: AI 情绪（按优先级聚合：busy→Coding > thinking→Thinking > idle→Done）

**活跃判定**：session 文件 15 分钟内更新，或进程仍在运行

#### 模式 B：单仓库监控

监控指定的一个 Git 仓库：

```bash
# Windows 推荐
cd pc_tools
realtime_monitor.bat COM6 <仓库路径> [更新间隔秒数]

# 示例
realtime_monitor.bat COM6 .. 5                      # 当前项目
realtime_monitor.bat COM6 D:/projects/my-app 10     # 指定仓库

# Python 直接调用
python realtime_monitor.py COM6 .. 5
```

**效果**：
- **Screen 2**: 仅显示该仓库的状态

**停止**：按 `Ctrl+C`

**AI BOX 显示位置**：按 **B 键**切换屏幕查看 Screen 1 / 2 / 5

---

### 2. 决策交互（触摸选择）

**功能**：向 AI BOX 发送选择题，用户在触摸屏上选择，结果返回给 PC。

**基本用法（Windows 推荐）**：
```bash
ask_user_via_box.bat COM6 <标题> <选项1> <选项2> [选项3] [选项4]
```

**基本用法（Python 直接调用）**：
```bash
python ask_user_via_box.py COM6 <标题> <选项1> <选项2> [选项3] [选项4]
```

**语言支持**：
- **标题和选项支持中文**：脚本自动将常见中文词汇翻译成英文发送到 BOX（LVGL 字体限制）
- **PC 端显示中文**，BOX 端显示对应英文
- 内置映射词汇：是/否、确认/取消、启用/禁用、选项A/B/C/D 等 30+ 词
- 未映射的中文会标记为 `[CN:原文]` 提示添加到映射表

**示例**：
```bash
# 中文标题+选项（自动翻译，Windows 用 .bat）
ask_user_via_box.bat COM6 "是否部署？" "是" "否"

# 英文标题+选项（直接发送）
ask_user_via_box.bat COM6 "Deploy now?" "Yes" "No"

# 4 个选项
ask_user_via_box.bat COM6 "选择数据库" "PostgreSQL" "MySQL" "MongoDB" "Redis"
```

**超时设置**：
- **默认：永久等待**，直到用户在 BOX 上选择或按 B 键取消
- 如需超时：编辑脚本，改 `timeout=None` 为 `timeout=30`（秒）

**显示效果**：

**PC 端**（命令行窗口）：
```
============================================================
  是否部署？
============================================================
  [0]  是
  [1]  否
============================================================
请在 AI BOX 屏幕上触摸选择（永久等待，按B键取消）

------------------------------------------------------------
[OK] 用户选择: [1] 否
------------------------------------------------------------
```

**AI BOX 端**（触摸屏）：
- 自动弹出决策界面（Screen 7）
- 标题：Deploy?（翻译后的英文）
- 选项按钮：Yes / No（翻译后的英文）
- 触摸选择 + 长按 **A 键**确认

**返回值**：
- 成功：退出码 `0`，标准输出包含选择的索引（0-3）
- 失败：退出码 `1`（超时或取消）

**Python 脚本调用**：
```python
import subprocess

# 使用 .bat 启动（Windows 推荐，避免中文乱码）
result = subprocess.run(
    ['ask_user_via_box.bat', 'COM6', 'Deploy?', 'Yes', 'No'],
    capture_output=True,
    text=True
)

# 或直接调用 Python 脚本
result = subprocess.run(
    ['python', 'ask_user_via_box.py', 'COM6', 'Deploy?', 'Yes', 'No'],
    capture_output=True,
    text=True
)

if result.returncode == 0:
    # 从输出解析选择（例：[OK] 用户选择: [1] No）
    print("用户同意部署")
else:
    print("用户取消或超时")
```

**高级用法（Python 函数调用）**：
```python
from ask_user_via_box import ask_decision

# 直接调用函数
choice = ask_decision('COM6', 'Choose approach', ['Option A', 'Option B'], timeout=30)

if choice is not None:
    print(f"用户选择了选项 {choice}")
else:
    print("用户取消或超时")
```

---

## 📊 数据说明

### Token 用量（realtime_monitor.py）
- **数据源**：`~/.claude/history.jsonl` 文件大小
- **估算方法**：1 KB ≈ 250 tokens
- **误差**：±20%（history.jsonl 只记录用户输入，不含完整响应）
- **多窗口模式**：所有窗口共享同一个 Token 池（全局数据）
- **显示**：Screen 1，进度条 + 百分比

### Git 项目状态（realtime_monitor.py）
- **数据源**：`git status --porcelain`
- **单仓库模式**：显示指定仓库的状态
- **多窗口模式**：显示每个活跃窗口的工作目录状态（最多 6 个）
  - 示例：`dongle_firmware` → Coding, `52_codebuddy_ai_box` → Review
- **显示内容**：
  - **项目名** — 工作目录名称
  - **状态** — Planning / Coding / Review / Completed / Idle
- **状态映射**：
  - Planning（蓝色）：仅有新文件（Untracked > 0, Modified = 0）
  - Coding（紫色）：大量改动（Modified > 10 或 Untracked > 5）
  - Review（橙色）：少量改动（Modified > 0）
  - Completed（绿色）：干净仓库（无改动）
  - Idle（灰色）：非 Git 仓库
- **显示**：Screen 2，列表形式（每行一个项目）

### AI 情绪（realtime_monitor.py）
- **数据源**：`~/.claude/sessions/{pid}.json` 的 `status` 字段
- **单窗口**：直接显示该窗口状态
- **多窗口模式**：按优先级聚合
  - 任一窗口 `busy` → 显示 Coding
  - 否则任一 `thinking` → 显示 Thinking
  - 全部 `idle` → 显示 Done
- **映射**：
  - `busy` → Coding（皱眉 + 动嘴 + "Coding..."）
  - `thinking` → Thinking（挑眉 + 闭嘴 + "Thinking..."）
  - `idle` → Done（平眉 + 微笑 + "Done!"）
- **显示**：Screen 5，云朵表情 + 文字

---

## 🤖 Agent 工作流可视化（NEW）

### 功能说明

**agent_status_bridge.py** 实时推送 Claude Code Agent 工作流状态到 AI BOX：

- **THINKING**：Agent 正在思考（收到用户输入，未开始工具调用）
- **RUNNING**：Agent 正在执行工具（Read/Edit/Bash/等）
- **DONE**：Agent 完成任务（空闲状态）
- **审批请求**：检测到 `AskUserQuestion` 工具调用时，推送审批界面到 AI BOX
- **离线检测**：与 Dongle 断开时显示离线横幅（未来实现）

**与现有工具的区别**：
- `realtime_monitor.py`：推送**静态数据**（Token 用量、项目状态、AI 情绪）
- `agent_status_bridge.py`：推送**动态工作流**（Agent 当前在做什么、是否需要审批）

### 快速开始

```bash
# Windows 推荐（UTF-8 编码，避免乱码）
cd pc_tools
agent_status_bridge.bat COM6

# Python 直接调用
python agent_status_bridge.py COM6 --interval 2
```

**参数**：
- `COM6`：Dongle 串口
- `--interval 2`：状态刷新间隔（秒，默认 2）

### 运行效果

**终端输出示例**：
```
=== Agent Status Bridge ===
串口: COM6
刷新间隔: 2s
监听 Claude Code 会话...

[→] Agent: THINKING | Thinking...
[→] Agent: RUNNING | Running: Read
[→] Agent: RUNNING | Running: Edit
[→] 审批请求: Should I proceed with this change?
[→] Agent: DONE | Idle
```

**AI BOX 显示**：
- **THINKING 态**：屏幕显示 "Agent: Thinking..." + 任务描述
- **RUNNING 态**：屏幕显示 "Agent: Running: <工具名>" + 详情
- **DONE 态**：屏幕显示 "Agent: Idle"
- **审批请求**：自动切换到 Screen 8（审批界面），显示标题 + 描述 + OK/UP/DOWN 三键

### 状态检测逻辑

工具通过扫描 `~/.claude/sessions/` 下的活跃会话（15 分钟内更新）：

1. **解析 history.jsonl**：读取最后 10 条消息
2. **反向扫描**：
   - 发现 `assistant` role + `tool_use` → **RUNNING**（提取工具名）
   - 发现 `AskUserQuestion` 工具 → **THINKING** + **审批请求**
   - 发现 `user` role → **THINKING**
   - 其他 → **IDLE**
3. **聚合多会话**：按优先级选择（RUNNING > THINKING > DONE > IDLE）
4. **避免重复发送**：缓存上次发送的状态，仅在变化时发送

### 审批交互流程

1. **PC 端检测到审批请求**（如 `AskUserQuestion` 调用）
2. **推送审批帧**（0x0D）到 Dongle → AI BOX
3. **AI BOX 弹出 Screen 8**：显示标题 + 描述 + 三个触摸按钮
   - **OK**：批准（发送 APPROVE 回执）
   - **UP**：拒绝（发送 REJECT 回执）
   - **DOWN**：查看详情（发送 VIEW_DIFF 回执，暂不退出界面）
4. **PC 端接收回执**（0x0E）并继续工作流

**注意**：当前版本的审批回执接收逻辑**未实现**（需在 `agent_status_bridge.py` 中添加串口读取循环），审批界面仅作演示。

### 与其他工具配合

**推荐组合**：

```bash
# 终端 1：启动状态监控（Token/项目/AI 情绪）
realtime_monitor.bat COM6 --auto 5

# 终端 2：启动 Agent 工作流桥接（动态状态）
agent_status_bridge.bat COM6 --interval 2
```

两个工具**可以同时运行**，因为：
- `realtime_monitor.py` 使用**运行时协调机制**（`.runtime/pause.flag`）
- `agent_status_bridge.py` 独立串口连接，不冲突

**注意**：如果遇到串口占用错误，先停止 `realtime_monitor.py`，或使用 `--auto` 模式（自动检测暂停）。

---

## ⚙️ 高级配置

### 修改更新频率（realtime_monitor.py）

命令行第三个参数：
```bash
python realtime_monitor.py COM6 .. 3   # 每 3 秒更新（快）
python realtime_monitor.py COM6 .. 10  # 每 10 秒更新（省资源）
```

### 修改决策超时（ask_user_via_box.py）

编辑脚本，找到：
```python
choice = ask_decision('COM6', title, options, timeout=30)  # 改这里
```

### 后台运行监控（Windows）

```powershell
# 启动后台监控（无窗口）
Start-Process python -ArgumentList "realtime_monitor.py","COM6","..","5" -WindowStyle Hidden

# 查看运行中的 Python 进程
Get-Process python

# 停止所有监控
Get-Process python | Stop-Process
```

### 后台运行监控（Linux/macOS）

```bash
# 启动后台
nohup python realtime_monitor.py COM6 .. 5 > monitor.log 2>&1 &

# 查看日志
tail -f monitor.log

# 停止
pkill -f realtime_monitor
```

---

## 🐛 故障排除

### 问题 1：串口被占用

**错误**：`PermissionError: could not open port 'COM6'`

**原因**：其他程序占用了串口

**解决**：
```powershell
# Windows: 停止所有 Python 进程
Get-Process python | Stop-Process -Force

# 或者重新插拔 Dongle USB
```

### 问题 2：AI BOX 没反应

**可能原因**：

1. **Dongle 未连接** → 检查 COM6 是否存在
2. **AI BOX 未开机** → 检查 AI BOX 屏幕是否亮
3. **ESP-NOW 未配对** → 重启 AI BOX 和 Dongle（拔插 USB/重新上电）
4. **AI BOX 停在错误界面** → 按 **B 键**切回主界面

### 问题 3：决策超时

**原因**：30 秒内没有在 AI BOX 上选择并确认

**解决**：
- 触摸选择选项
- **长按 A 键**确认（不是短按）
- 或按 **B 键**取消

### 问题 4：数据不更新

**检查清单**：
1. 监控脚本是否还在运行？（`Get-Process python`）
2. Git 仓库路径是否正确？（第二个参数）
3. AI BOX 是否停在对应界面？（按 B 切到 Token/Project/AI 界面）
4. 串口连接是否正常？（脚本输出是否有 "串口未连接" 错误）

### 问题 5：中文乱码

**原因**：AI BOX 固件的 LVGL 字体不支持中文

**解决**：决策标题和选项必须用**英文**

---

## 📝 限制说明

| 限制 | 原因 | 影响 |
|------|------|------|
| 决策选项最多 4 个 | 协议设计 + 屏幕空间 | 超过 4 个的选择需分多次询问 |
| 标题最多 32 字符 | 协议限制 | 超长标题会被截断 |
| 选项最多 24 字符 | 协议限制 | 超长选项会被截断 |
| 仅支持英文 | LVGL 字体限制 | 中文显示为乱码 |
| Token 估算误差 ±20% | 无法读取真实 API usage | 仅供参考，不精确 |
| 决策超时 30 秒 | 防止无限等待 | 需要及时在 BOX 上确认 |

---

## 💡 使用场景

### 场景 1：开发过程监控
```bash
# 持续运行监控，观察 Token 消耗和 Git 变化
python realtime_monitor.py COM6 . 5
```

### 场景 2：部署前确认
```bash
# 在脚本里加入决策点
python ask_user_via_box.py COM6 "Deploy to production?" "Deploy" "Cancel"

if [ $? -eq 0 ]; then
    echo "开始部署..."
    # 部署命令
else
    echo "用户取消部署"
fi
```

### 场景 3：Claude Code 集成

在 Claude Code 的工作流里，让 AI 在需要时自动询问：

```python
# Claude 调用决策工具
from ask_user_via_box import ask_decision

choice = ask_decision('COM6', 'Delete old files?', ['Yes', 'No'], timeout=30)

if choice == 0:
    # 用户选了 Yes
    delete_files()
else:
    # 用户选了 No 或超时
    print("保留旧文件")
```

---

## 📂 文件结构

```
pc_tools/
├── realtime_monitor.py        # 实时监控主脚本
│   ├── get_token_usage()      # Token 用量提取
│   ├── get_git_projects()     # Git 状态解析
│   ├── get_ai_emotion()       # AI 情绪判断
│   └── RealtimeMonitor 类     # 监控主循环
│
├── ask_user_via_box.py        # 决策交互主脚本
│   ├── ask_decision()         # 决策函数（推荐用这个）
│   ├── build_decision_frame() # 协议打包
│   └── wait_for_reply()       # 等待回执
│
└── README.md                  # 本文档
```

---

## 🔗 相关文档

- **协议文档**：`../PC上位机联调进度.md` — 完整通信协议说明
- **功能清单**：`../功能清单.md` — 所有功能总览
- **固件源码**：
  - AI BOX：`../51_mic_wifi.ino`
  - Dongle：`../../../dongle_firmware/`

---

## 🆘 获取帮助

遇到问题？检查：

1. **串口号是否正确**：`python -m platformio device list`
2. **Dongle 是否在线**：COM6 应该存在
3. **AI BOX 是否响应**：按 B 键切界面测试
4. **Python 依赖**：`pip install pyserial`
5. **固件版本**：确保 AI BOX 和 Dongle 都是最新固件

---

## 📌 版本历史

**v1.0** (2026-09-01)
- ✅ 实时监控：Token/Git/AI情绪
- ✅ 决策交互：双端显示 + 触摸选择
- ✅ 自动重连 + 错误处理
- ✅ 可调更新间隔

---

**祝使用愉快！ 🎉**

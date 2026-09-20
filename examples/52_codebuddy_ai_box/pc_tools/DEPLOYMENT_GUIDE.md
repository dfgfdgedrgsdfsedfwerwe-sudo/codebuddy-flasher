# 新电脑快速部署指南

当你需要在另一台电脑上使用 ATK BOX 的 Claude Code 集成功能时，按以下步骤操作。

## 前置条件

1. **硬件连接**
   - ATK BOX 通过 USB 连接到新电脑（通常显示为 COM11/COM12）
   - Dongle 通过 USB 连接到新电脑（通常显示为 COM6）

2. **软件安装**
   - Python 3.7+ （运行 `python --version` 验证）
   - pyserial 库：`pip install pyserial`
   - Claude Code CLI（从 claude.ai/code 安装）

## 方式 1：使用 exe 一键安装（推荐，无需 Python）

### 步骤 1：获取 exe 文件

**选项 A：使用已编译版本**
```bash
# 从项目获取（约 10MB，包含所有依赖）
pc_tools/dist/CodeBuddyBridge.exe
```

**选项 B：自己编译**
```bash
cd pc_tools
pip install pyinstaller  # 如果未安装
python -m PyInstaller CodeBuddyBridge.spec
# 编译后的 exe 在 dist/CodeBuddyBridge.exe
```

### 步骤 2：安装串口驱动（如果新电脑未装）

ATK BOX / Dongle 使用 CH340 串口芯片，下载驱动：
- [CH340官方驱动](http://www.wch.cn/downloads/CH341SER_EXE.html)

### 步骤 3：双击运行 exe

- 首次运行会加载 GUI 库，稍等 2-3 秒
- 弹出 CodeBuddy Bridge 主窗口

### 步骤 4：在 GUI 中依次点击

1. **扫描** 按钮 → 选择 Dongle 串口（通常 COM6）
2. **安装 MCP** 按钮
   - 提示成功后会显示需要重启 Claude Code
3. **安装全局指令** 按钮
   - 这会配置 `~/.claude/CLAUDE.md`，让 Claude 自动同步决策到硬件
4. **应用设置** 按钮
5. **启动服务** 按钮

### 步骤 5：重启 Claude Code

```bash
# 完全退出所有 Claude Code 窗口（重要！）
exit

# 重新启动
claude
```

### 步骤 5：验证安装

在 Claude Code 会话中输入：
```
/mcp
```

应该能看到 `ask_on_atkbox` 工具出现在列表中。

### 步骤 6：测试双界面交互

在对话中说：
```
帮我选数据库，MySQL、PostgreSQL 还是 MongoDB？
```

✅ **预期结果**：PC 终端和 ATK BOX 屏幕同时显示选项，触摸硬件或在终端输入都能选择。

---

## 方式 2：使用 Python 源码部署（需要 Python 环境）

适合开发环境或可联网安装 Python 的电脑。

### 步骤 1：复制整个项目文件夹

将 `dfk10_arduino_demo-master` 整个文件夹复制到新电脑任意位置，例如：
```
D:\Projects\dfk10_arduino_demo-master
```

### 步骤 2：安装依赖

```bash
cd D:\Projects\dfk10_arduino_demo-master\examples\52_codebuddy_ai_box\pc_tools
pip install pyserial
```

### 步骤 3：启动 GUI 程序

```bash
python codebuddy_bridge_gui.py
```

### 步骤 4-6：同方式 1 的步骤 4-6

（在 GUI 中安装 MCP、安装全局指令、重启 Claude Code、验证、测试）

**预期行为**：
- ✅ PC 终端显示选择对话框
- ✅ ATK BOX 屏幕同步显示 3 个触摸选项
- ✅ 你可以在任一界面选择，Claude 都能收到答案

---

## 方式 2：命令行手动安装（高级）

如果你不想用 GUI，可以手动执行安装脚本。

### 步骤 1：复制项目文件夹（同方式 1）

### 步骤 2：运行安装脚本

```bash
cd dfk10_arduino_demo-master/examples/52_codebuddy_ai_box/pc_tools

# 安装 MCP Server
python install_mcp_atkbox.py

# 安装全局 CLAUDE.md 指令
python install_claude_md.py
```

### 步骤 3：启动 daemon

```bash
python atkbox_daemon.py COM6
```

保持这个窗口运行，不要关闭。

### 步骤 4：重启 Claude Code + 测试（同方式 1 步骤 4-6）

---

## 配置文件位置说明

以下文件会被自动创建/修改，**跨电脑时无需手动复制**（安装脚本会自动生成）：

| 文件 | 位置 | 作用 |
|------|------|------|
| MCP 配置 | `~/.claude.json` | 注册 `ask_on_atkbox` 工具 |
| 全局指令 | `~/.claude/CLAUDE.md` | 让 Claude 自动双重调用 |
| daemon 配置 | `pc_tools/config.json` | 串口、端口等配置 |

**重要**：不同电脑的串口号可能不同（COM6 vs COM7），安装后需在 GUI 里扫描并重新选择。

---

## 卸载指令

如果你不想在某台电脑上使用这个功能：

### GUI 方式
1. 打开 CodeBuddyBridge GUI
2. 点击 **卸载全局指令** → **卸载 MCP** → **卸载 Hooks**（如果安装了）
3. 重启 Claude Code

### 命令行方式
```bash
python install_claude_md.py --uninstall
python install_mcp_atkbox.py --uninstall
```

---

## 常见问题

### Q1: GUI 启动后提示 "无可用端口"
**原因**：硬件未连接或驱动未安装  
**解决**：
- 检查设备管理器，确认 COM 口存在
- 安装 CP2102/CH340 USB 转串口驱动

### Q2: Claude 调用 ask_on_atkbox 工具失败
**原因**：daemon 未运行  
**解决**：在 GUI 点"启动服务"，或命令行运行 `python atkbox_daemon.py COM6`

### Q3: 重启 Claude Code 后仍看不到 MCP 工具
**原因**：
- 未完全退出（多个窗口残留）
- `~/.claude.json` 格式错误

**解决**：
- Windows 任务管理器里强制结束所有 `claude.exe` / `node.exe` 进程
- 检查 `~/.claude.json` 文件格式是否正确（有效 JSON）

### Q4: Claude 不自动调用硬件工具
**原因**：
- 全局指令未安装（`~/.claude/CLAUDE.md` 文件不存在或缺少 ATK BOX 段）
- 问题不是 2-4 选项（开放式问题不触发）

**解决**：
- 在 GUI 点"安装全局指令"
- 确保问题是明确的多选（"A、B 还是 C？"）

### Q5: 双界面显示但只有 PC 端选择生效
**原因**：ESP-NOW 通信失败，BOX 的回复帧未到达 PC  
**解决**：
- 检查 `51_mic_wifi.ino` 顶部的 `dongle_mac` 是否为 `e0:72:a1:d4:8f:e0`
- 用串口监控 COM12，查看 BOX 是否发送了 0x0B 回复帧

---

## 文件清单（需要复制到新电脑的）

```
dfk10_arduino_demo-master/
└── examples/52_codebuddy_ai_box/pc_tools/
    ├── atkbox_daemon.py           # daemon 主程序
    ├── codebuddy_bridge_gui.py    # GUI 主程序
    ├── install_mcp_atkbox.py      # MCP 安装脚本
    ├── install_claude_md.py       # 全局指令安装脚本
    ├── mcp_atkbox_server.py       # MCP 服务器
    ├── config_manager.py          # 配置管理
    ├── serial_manager.py          # 串口管理
    └── ...（其他工具脚本）
```

**建议**：复制整个 `dfk10_arduino_demo-master` 文件夹，保持相对路径不变。

---

## 成功标志

✅ GUI 状态面板显示：串口 ● 已连接、IPC ● 监听中  
✅ `/mcp` 命令能看到 `ask_on_atkbox` 工具  
✅ 问一个多选问题，PC + ATK BOX 同时显示  
✅ 在 BOX 触摸选择后，Claude 收到答案并继续执行  

---

**版本**: v1.0.0  
**更新日期**: 2026-09-11  
**适用系统**: Windows 10/11  
**硬件要求**: ATK ESP32-S3 BOX + ESP32-S3 Dongle

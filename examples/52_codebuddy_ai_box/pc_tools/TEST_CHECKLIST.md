# 验证清单：跨电脑部署功能测试

## 测试环境
- **场景**: 模拟在新电脑上首次部署 ATK BOX + Claude Code 集成
- **前置条件**: 
  - 已复制 `dfk10_arduino_demo-master` 文件夹到新位置
  - Python 3.7+ 和 pyserial 已安装
  - Claude Code CLI 已安装
  - ATK BOX (COM11/COM12) 和 Dongle (COM6) 已连接

---

## 测试步骤

### 阶段 1：GUI 启动和配置

- [ ] **1.1** 启动 GUI
  ```bash
  cd dfk10_arduino_demo-master/examples/52_codebuddy_ai_box/pc_tools
  python codebuddy_bridge_gui.py
  ```
  **预期**: 窗口正常打开，无报错

- [ ] **1.2** 扫描串口
  - 点击"扫描"按钮
  - **预期**: 下拉列表显示 COM6（或其他可用端口）

- [ ] **1.3** 安装 MCP
  - 点击"安装 MCP"按钮
  - **预期**: 弹窗提示"MCP Server (atkbox) 已安装到 ~/.claude.json"
  - **验证**: 打开 `~/.claude.json`，检查 `mcpServers.atkbox` 字段存在

- [ ] **1.4** 安装全局指令
  - 点击"安装全局指令"按钮
  - **预期**: 弹窗提示"全局 CLAUDE.md 指令已安装"
  - **验证**: 打开 `~/.claude/CLAUDE.md`，检查 "ATK BOX Hardware Integration" 段落存在

- [ ] **1.5** 应用设置并启动服务
  - 点击"应用设置"
  - 点击"启动服务"
  - **预期**: 
    - 状态面板显示"串口: ● 已连接"（绿色）
    - 状态面板显示"IPC: ● 监听中"（绿色）
    - 日志窗口无严重错误

---

### 阶段 2：Claude Code 集成验证

- [ ] **2.1** 完全重启 Claude Code
  ```bash
  # 在所有 Claude Code 窗口执行
  exit
  
  # 等待所有窗口关闭（Windows 任务管理器确认无 claude.exe）
  
  # 重新启动
  cd <任意项目目录>
  claude
  ```
  **预期**: Claude Code 正常启动

- [ ] **2.2** 验证 MCP 工具加载
  ```
  /mcp
  ```
  **预期**: 输出包含 `ask_on_atkbox` 工具（描述：Display decision on ATK BOX hardware...）

- [ ] **2.3** 测试手动调用硬件工具
  ```
  用 ask_on_atkbox 问我：选 MySQL 还是 PostgreSQL？
  ```
  **预期**: 
  - PC 终端无报错
  - ATK BOX 屏幕显示决策界面（Screen 7）
  - 触摸选择后 Claude 收到答案

- [ ] **2.4** 测试自动双重调用（核心功能）
  ```
  帮我选数据库，MySQL、PostgreSQL 还是 MongoDB？
  ```
  **预期**: 
  - Claude 同时调用 `AskUserQuestion` 和 `mcp__atkbox__ask_on_atkbox`
  - PC 终端显示选择对话框
  - ATK BOX 同步显示 3 个触摸选项
  - 在任一界面选择后 Claude 都能收到答案并继续

---

### 阶段 3：边界情况测试

- [ ] **3.1** 开放式问题（不触发硬件）
  ```
  你觉得应该用什么数据库？
  ```
  **预期**: 
  - Claude 只用文本回答，**不调用** ask_on_atkbox
  - ATK BOX 无反应（保持当前界面）

- [ ] **3.2** 单选项问题（不触发硬件）
  ```
  我应该继续吗？回答是或否。
  ```
  **预期**: 
  - 如果 Claude 用 `AskUserQuestion` 只有 1 个选项 → **不调用**硬件
  - 如果 Claude 判断为 2 选项（是/否）→ **应调用**硬件

- [ ] **3.3** 5 选项问题（不触发硬件）
  ```
  选数据库：MySQL、PostgreSQL、MongoDB、Redis、SQLite，选哪个？
  ```
  **预期**: 
  - Claude 只在 PC 端提问（或分两轮询问）
  - **不调用**硬件（超过 4 选项限制）

- [ ] **3.4** daemon 未运行降级
  - 在 GUI 点击"停止服务"
  - 在 Claude 中问 2-4 选项问题
  - **预期**: 
    - Claude 调用 `ask_on_atkbox` 但超时
    - 优雅降级到 PC-only 输入（不阻塞对话）

---

### 阶段 4：卸载功能验证

- [ ] **4.1** 卸载全局指令
  - 在 GUI 点击"卸载全局指令"
  - **预期**: 弹窗提示"全局 CLAUDE.md 指令已卸载"
  - **验证**: `~/.claude/CLAUDE.md` 中 "ATK BOX Hardware Integration" 段落被移除

- [ ] **4.2** 验证卸载后行为
  - 重启 Claude Code
  - 问一个 2-4 选项问题
  - **预期**: Claude **不再自动调用**硬件工具（只用 `AskUserQuestion`）

- [ ] **4.3** 卸载 MCP
  - 在 GUI 点击"卸载 MCP"
  - **预期**: 弹窗提示"MCP Server (atkbox) 已从 ~/.claude.json 卸载"
  - **验证**: 
    - 重启 Claude Code
    - `/mcp` 命令不再显示 `ask_on_atkbox` 工具

---

## 故障排查测试

- [ ] **F.1** 串口被占用
  - 用 `python -m platformio device monitor -p COM6` 占用串口
  - 在 GUI 点击"启动服务"
  - **预期**: 
    - 日志显示"串口被占用"错误
    - 点击"释放串口"按钮 → 强制关闭占用进程
    - 重新"启动服务"成功

- [ ] **F.2** 配置文件格式错误
  - 手动编辑 `~/.claude.json`，故意破坏 JSON 格式（删除一个逗号）
  - 在 GUI 点击"安装 MCP"
  - **预期**: 
    - 弹窗报错"读取现有配置失败，将创建新配置"
    - 安装继续进行，生成正确的 JSON

- [ ] **F.3** 重复安装幂等性
  - 在 GUI 多次点击"安装全局指令"
  - **预期**: 
    - 第一次：创建配置
    - 第二次及以后：跳过（日志显示"ATK BOX 配置段已存在"）
    - `~/.claude/CLAUDE.md` 中只有一个 ATK BOX 段落（无重复）

---

## 成功标志 ✅

全部通过以下验证即为成功：

1. ✅ GUI 能正常启动和扫描串口
2. ✅ MCP 和全局指令安装后文件正确生成
3. ✅ Claude Code 重启后能看到 `ask_on_atkbox` 工具
4. ✅ 问 2-4 选项问题时 PC + ATK BOX 同时显示
5. ✅ 触摸或 PC 选择后 Claude 都能收到答案
6. ✅ 开放式/单选/5选项问题不触发硬件（边界正确）
7. ✅ daemon 停止时降级到 PC-only（不阻塞）
8. ✅ 卸载后功能正确移除

---

## 测试环境记录

- **测试日期**: ___________
- **操作系统**: Windows 10/11
- **Python 版本**: ___________
- **Claude Code 版本**: ___________
- **串口号**: Dongle=_______, ATK BOX=_______
- **测试人员**: ___________

---

## 已知问题

无（如发现问题请记录到此处）

---

**版本**: v1.0.0  
**文档日期**: 2026-09-11

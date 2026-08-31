# Claude Code ⇄ K10 双向触摸决策系统 设计文档

- 日期: 2026-08-31
- 状态: 已评审通过, 待实现
- 相关: [CLAUDE.md 上位机协议节], `examples/52_codebuddy_ai_box/`, `dongle_firmware/`

## 1. 目标

让 Claude Code (PC 上的 CLI 编程工具) 在**每个需要用户决策的节点**把选项推送到 K10 屏幕,
用户通过 K10 电容触摸屏选择并确认, 选择结果回传并**真正控制** Claude Code 的下一步执行。

覆盖两类决策点:
- **工具权限确认**: Claude 要执行工具 (Bash/Write/Edit 等) 时, 推送 "允许 / 拒绝 / 总是允许"
- **多选题 (AskUserQuestion)**: Claude 主动提问时, 把它给出的问题和选项原样推送

## 2. 关键设计决策 (及被否决的方案)

### 2.1 采用 Agent SDK 的 canUseTool 回调, 而非 hook
调研结论: `canUseTool` 回调 (Python `can_use_tool(tool_name, input_data, context)`)
可以**无限阻塞等待** ("Execution remains paused until your callback returns"), 在回调内
`await` 硬件响应完全合法; 且 `AskUserQuestion` 也走同一回调, 天然覆盖多选题。

- **否决**: PreToolUse hook。虽然也能阻塞返回 allow/deny, 但只能处理权限、不能优雅处理
  AskUserQuestion, 且数据结构不如 SDK 回调清晰。
- **代价**: 用户以后需经由中间层程序 `cc_bridge` 启动 Claude Code, 而非直接 `claude`。已确认接受。

### 2.2 "决策点驱动" 而非 "每轮凭空生成选项"
用户最初设想 "每轮 Claude 答完都凭空生成下一步选项"。调研确认 hook 和 SDK 都**无法**凭空
生成选项并驱动新对话轮。改为在 Claude **真正需要决策的节点**推送——这些是天然的选项时刻,
且选择能真正控制 Claude。已与用户确认接受此调整。

### 2.3 PC↔Dongle 复用现有 UART0 串口, 不改 TinyUSB 描述符
- **否决**: 新增 USB CDC 虚拟串口 (要动 USB 复合描述符, 有把已验证的 HID+UAC 改坏的风险)。
- **否决**: 外接第二个 USB-UART 适配器 (需额外硬件, 对演示过重)。
- **采用**: 复用 Dongle 现有 UART0 (GPIO43/44, 即烧录/日志口)。控制台改走 USB-Serial/JTAG
  (`CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y`, 独立外设、不冲突 TinyUSB 的 OTG PHY), 腾出 UART0
  给 `pc_link` 双向收发。**这是唯一需实测验证的点**, 实现时先单独验证串口双向通再往下做。

## 3. 整体架构 (双向数据流)

```
┌─────────────────────────────────────────────────────────────┐
│ PC: cc_bridge (新增, Python, 用 Claude Agent SDK 包裹 CC)     │
│                                                               │
│   query(Claude Code) 正常跑                                   │
│        │                                                      │
│   canUseTool 回调被触发 (要用工具 / AskUserQuestion)          │
│        │  ①把"问题+选项"打包成决策帧 (含 decision_id)         │
│        ▼                                                      │
│   串口写 COM6 ──②──► Dongle ──③ ESP-NOW 0x0A──► K10           │
│        ▲                                          │           │
│        │                                     你触摸选择        │
│   ⑥回调 return                              ④K10 组回执帧      │
│   allow/deny 或答案                              │           │
│        ▲                                          ▼           │
│   串口读 COM6 ◄─⑤ Dongle ◄── ESP-NOW 0x0B ◄──────┘           │
│        │  按 decision_id 唤醒对应等待                          │
│   Claude 按你的选择继续执行                                   │
└─────────────────────────────────────────────────────────────┘
```

三个可独立测试的单元:
- **cc_bridge (PC)**: 输入 = Claude 的决策点; 输出 = 用户选择。依赖 claude-agent-sdk + pyserial。
- **pc_link (Dongle)**: 输入 = 串口帧 / ESP-NOW 帧; 输出 = 双向转发。不涉及 USB 描述符。
- **decision screen (K10)**: 输入 = 0x0A 决策帧; 输出 = 0x0B 回执帧。复用现有 touch.scan()。

## 4. 协议扩展 (espnow_protocol.h — 两端必须同步)

现有帧类型到 0x09。新增两个, **修改后必须在 K10 与 Dongle 两侧保持字节一致**:

### 0x0A DECISION_REQUEST (PC → Dongle → K10)
```c
#define DECISION_TITLE_LEN   32
#define DECISION_OPT_LEN     24
#define DECISION_MAX_OPTS     4
typedef struct __attribute__((packed)) {
    uint8_t  frame_type;                 /* = 0x0A */
    uint16_t decision_id;                /* 递增, 用于回执对齐 */
    uint8_t  kind;                       /* 0=工具权限 1=多选题 */
    char     title[DECISION_TITLE_LEN];  /* 如 "允许执行 Bash: rm -rf ..." */
    uint8_t  opt_count;                  /* 有效选项数 (1~4) */
    char     opts[DECISION_MAX_OPTS][DECISION_OPT_LEN];
    uint8_t  crc8;
} decision_request_frame_t;              /* 1+2+1+32+1+96+1 = 134 字节, <250 OK */
```

### 0x0B DECISION_REPLY (K10 → Dongle → PC)
```c
typedef struct __attribute__((packed)) {
    uint8_t  frame_type;    /* = 0x0B */
    uint16_t decision_id;   /* 对应请求 */
    uint8_t  chosen_index;  /* 用户选中的选项下标 (0~opt_count-1); 0xFF=超时/取消 */
    uint8_t  crc8;
} decision_reply_frame_t;   /* 5 字节 */
```

## 5. PC↔Dongle 串口帧格式 (pc_link 小协议)

UART0 与日志分流后仍需分帧 (防噪声/重同步):
```
0xA5 0x5A | inner_type(1) | len(1) | payload(len) | crc8(1)
```
- inner_type 0x0A → payload = decision_request 的 body (去掉外层已有的 frame_type)
- 方向: PC→Dongle 走 0x0A; Dongle→PC 走 0x0B。魔术头错位时丢弃并重新找头。

## 6. 各端职责

### 6.1 PC 端 cc_bridge (新增, tools/cc_bridge/)
- Python, 依赖 `claude-agent-sdk`, `pyserial`
- **运行时注入系统提示 (不改 CLAUDE.md 文件)**: 通过 SDK 的 system_prompt append 机制
  (`{"type":"preset","preset":"claude_code","append":"..."}`) 追加一句:
  "遇到有多个合理方案的决策点时, 优先用 AskUserQuestion 让用户选, 而不是直接选定。"
  这样**仅当经 cc_bridge 启动时才带此提示**, 直接 `claude` 不受影响。目的: 提高 K10 上
  多选题 (AskUserQuestion) 的出现频率。(SDK 确切参数名实现时核实。)
- `async def can_use_tool(tool_name, input_data, context)`:
  - 工具权限类: title = tool_name + 关键参数摘要; opts = ["允许","拒绝","总是允许"]
  - AskUserQuestion 类: 从 input_data 取 Claude 的问题与选项原样填入
  - 递增 decision_id → 发 0x0A → `await` asyncio 事件, 直到读线程收到匹配 decision_id 的 0x0B
  - 按 chosen_index 返回 PermissionResultAllow / Deny (updatedInput 可选) 或多选答案
- 后台线程专读串口 0x0B, 按 decision_id 唤醒对应等待
- 可配置超时 (默认 120s) 兜底: 超时返回 deny 并向 K10 发取消, 避免永久卡死

### 6.2 Dongle 端 (dongle_firmware, 扩展 pc_link.c/h)
- 控制台改走 USB-Serial/JTAG; UART0 driver 归 pc_link 双向使用
- 收串口 0x0A → esp_now_send 给 pairing_get_peer_mac() 的 K10
- ESP-NOW 收到 K10 的 0x0B → 经 UART0 TX 带魔术头写回 PC
- 未配对时收到 0x0A → 回 "设备离线" 错误帧给 PC

### 6.3 K10 端 (examples/52_codebuddy_ai_box, 51_mic_wifi.ino)
- espnow_recv_cb 加 0x0A 分支: 收到即 switch_screen 到新建的决策界面
- 决策界面: 标题 label + 纵向选项按钮 (LVGL button), 用现有 touch.scan() 命中检测
- 纯触摸: 点选项高亮, 再点确认键发出 0x0B (或点即选中+底部确认键, 实现时定)
- 决策界面显示时暂停界面轮换; 15s 无操作发 chosen_index=0xFF (超时)
- 所有 LVGL 操作持 xGuiSemaphore (项目既有约束)

## 7. 错误处理

| 场景 | 处理 |
|------|------|
| K10 未连/未选 | 回调可无限阻塞是安全的; cc_bridge 120s 超时兜底返回 deny |
| decision_id 不匹配/过期 | 回执丢弃 |
| Dongle 无配对 K10 | 回 "设备离线" 错误帧, cc_bridge fallback 到终端确认 |
| CRC 失败 / 魔术头错位 | 丢弃并重新找头, 不崩 |
| UART0/console 改动 | 回归测试确认音频/键盘/USB 枚举仍正常 |

## 8. 测试计划 (分阶段, 先验证再叠加)

1. **串口双向回环**: cc_bridge 发 0x0A → Dongle 解析后立刻造假 0x0B 回写 → cc_bridge 收到并唤醒。验证 PC↔Dongle 双向串口 + console 迁移不破坏 TinyUSB。
2. **接 K10 决策界面**: 手动从 cc_bridge 发决策 → K10 弹界面 → 触摸选择 → cc_bridge 收到正确 index。
3. **接真 Claude Code**: SDK 跑一个触发工具确认的任务 (让它写文件) → K10 弹 "允许写 xxx?" → 触摸允许 → Claude 真的写了; 再测一个 AskUserQuestion 多选。
4. **回归**: 确认音频/键盘仍正常 (UART0/console 改动不影响 TinyUSB)。

## 9. 影响文件清单

**新增**:
- `tools/cc_bridge/` (Python: bridge 主程序 + 串口收发 + 配置)
- `dongle_firmware/main/pc_link.c/h`

**修改**:
- `examples/52_codebuddy_ai_box/espnow_protocol.h` 与 `dongle_firmware/main/espnow_protocol.h` (加 0x0A/0x0B, 字节一致)
- `dongle_firmware/sdkconfig.defaults` (console 改 USB-Serial/JTAG)
- `dongle_firmware/main/espnow_receiver.c` (0x0B 接收转发) / `main.c` (启动 pc_link 任务) / `CMakeLists.txt`
- `examples/52_codebuddy_ai_box/51_mic_wifi.ino` (0x0A 分支 + 决策界面)

**K10 端无需改动接收框架**: 0x07/0x08/0x09 既有接收逻辑保留, 仅新增 0x0A 分支。

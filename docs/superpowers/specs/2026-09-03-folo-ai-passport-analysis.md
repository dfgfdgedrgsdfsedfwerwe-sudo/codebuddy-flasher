# FoloToy AI Passport 架构分析与借鉴

**分析日期**: 2026-09-03  
**参考仓库**: https://github.com/zhaohuaxiaoy/folo-ai-passport-voice  
**目标**: 提取 Agent 审批流架构设计，应用到 CodeBuddy AI BOX 决策交互增强

---

## 1. 核心架构

### 1.1 三层分工

| 层 | 组件 | 职责 |
|---|---|---|
| **设备端** | ESP32-C3 固件 (ESP-IDF + LVGL 9) | 纯 C 状态机、UI 快照渲染、双通道传输、功耗管理 |
| **桌面端** | Python companion (Mac/Win) | BLE/USB 接入、火山 ASR 转发、悬浮窗、剪贴板注入 |
| **云端** | 火山引擎流式 ASR | 音频 → 文本 (partial 全量累积 / final 定稿) |

**数据流**:  
设备采集 16kHz 音频 → 100ms 帧经 BLE/USB → companion → 火山 ASR → 中间结果回显到悬浮窗+设备屏幕 → 定稿注入输入框

**反向通道**: 承载 Agent 状态、审批请求、按键裁决

### 1.2 纯 C 状态机设计 (`main/app_state.c`, 772 行)

**核心思想**: `state + event → action[]` (reducer 模式)

```c
void app_state_reduce(app_state_t *s, const app_event_t *ev, 
                      app_action_t *out, uint8_t *n, uint8_t max);
```

**特性**:
- 零 ESP-IDF 依赖 → PC 上直接跑 8 组 ctest
- 所有 LVGL 调用在外层 (`app_ui.c`) 根据 action 执行，状态机本身不碰 UI
- 单事件最多产出 6 个 action（`APP_ACT_MAX = 6`，注释详细说明为什么是 6）

**状态转移**:
```
HOME → READY → LISTENING → TRANSCRIBING → AGENT_RUNNING → APPROVAL → DONE
```

---

## 2. 审批流实现（核心借鉴点）

### 2.1 协议设计 (`main/app_protocol.c`)

**下行 (PC → 设备)**: JSON over BLE GATT / USB-Serial-JTAG

```json
{
  "type": "agent.approval_request",
  "taskId": "task-001",
  "title": "Deploy to production",
  "target": "api.example.com",
  "diffSummary": "+12 -3 in deploy.sh",
  "riskLevel": "high"   // "low" | "medium" | "high"
}
```

**上行 (设备 → PC)**: JSON 事件

```json
{
  "event": "agent.action",
  "taskId": "task-001",
  "action": "approve"   // "approve" | "reject" | "details"
}
```

**关键点**:
- `riskLevel` 三级：low / medium / high → UI 上显示不同颜色横幅
- `diffSummary` 是文本摘要（64 字符上限），不是完整 diff（设备屏幕装不下）
- `taskId` 用于回传时对账（防止 PC 发了多个请求后设备回传错乱）

### 2.2 设备端状态机处理 (`app_state.c:585-615`, `app_state.c:396-428`)

**收到 `APP_EV_APPROVAL_REQUEST` 事件时**:

```c
case APP_EV_APPROVAL_REQUEST:
    // 1. 强制解锁亮屏（审批必须被看见，防口袋盲批）
    if (s->locked) {
        s->locked = false;
        s->panel_on = true;
        s->screen_on = true;
        emit(out, out_n, max, (app_action_t){ .type = APP_ACT_UI_PANEL_ON });
        emit(out, out_n, max, (app_action_t){ .type = APP_ACT_UI_SCREEN_ON });
    }
    
    // 2. 审批打断录音（必须先停流，否则管线永远泄漏）
    if (s->state == APP_ST_LISTENING) {
        emit(out, out_n, max, (app_action_t){ .type = APP_ACT_STREAM_STOP });
        emit(out, out_n, max, (app_action_t){ .type = APP_ACT_SEND_VOICE_END });
    }
    
    // 3. 拷贝审批数据到状态结构
    str_cpy(s->task_id, sizeof(s->task_id), ev->u.approval.task_id);
    str_cpy(s->approval_title, sizeof(s->approval_title), ev->u.approval.title);
    str_cpy(s->approval_target, sizeof(s->approval_target), ev->u.approval.target);
    str_cpy(s->approval_diff, sizeof(s->approval_diff), ev->u.approval.diff_summary);
    s->approval_risk = ev->u.approval.risk < APP_RISK_COUNT ? ev->u.approval.risk : APP_RISK_MEDIUM;
    
    // 4. 切换到审批状态
    s->state = APP_ST_APPROVAL;
    s->state_since_ms = now_ms;
    
    // 5. 播放审批提示音 + 刷新 UI
    emit(out, out_n, max, (app_action_t){ .type = APP_ACT_PLAY_TONE, .u.tone = APP_TONE_APPROVAL });
    emit(out, out_n, max, (app_action_t){ .type = APP_ACT_UI_REFRESH });
    break;
```

**APPROVAL 状态下的按键处理**:

```c
case APP_ST_APPROVAL:
    if (ev->type == APP_EV_KEY_CLICK && b == APP_BTN_OK) {
        // OK = 批准
        app_action_t a = { .type = APP_ACT_SEND_AGENT_ACTION };
        str_cpy(a.u.agent_action.task_id, sizeof(a.u.agent_action.task_id), s->task_id);
        a.u.agent_action.decision = APP_ACTION_APPROVE;
        emit(out, n, max, a);
        
        s->state = APP_ST_AGENT_RUNNING;
        s->state_since_ms = now_ms;
        str_cpy(s->agent_message, sizeof(s->agent_message), "Approved, agent continues...");
        s->transcript_final = true;
        
        emit(out, n, max, (app_action_t){ .type = APP_ACT_UI_REFRESH });
        
    } else if (ev->type == APP_EV_KEY_CLICK && b == APP_BTN_UP) {
        // UP = 拒绝
        app_action_t a = { .type = APP_ACT_SEND_AGENT_ACTION };
        str_cpy(a.u.agent_action.task_id, sizeof(a.u.agent_action.task_id), s->task_id);
        a.u.agent_action.decision = APP_ACTION_REJECT;
        emit(out, n, max, a);
        
        emit(out, n, max, (app_action_t){ .type = APP_ACT_PLAY_TONE, .u.tone = APP_TONE_REJECT });
        
        s->state = APP_ST_AGENT_RUNNING;
        s->state_since_ms = now_ms;
        str_cpy(s->agent_message, sizeof(s->agent_message), "Rejected by user");
        s->transcript_final = true;
        
        emit(out, n, max, (app_action_t){ .type = APP_ACT_UI_REFRESH });
        
    } else if (ev->type == APP_EV_KEY_CLICK && b == APP_BTN_DOWN) {
        // DOWN = 查看详情（发送 Enter 键到 PC，让 PC 端打开详情窗口）
        send_key_action(s, APP_KEY_ENTER, out, n, max);
    }
    break;
```

**关键设计**:
- 审批态**永不熄屏**（`app_state.c:454-459` 心跳里专门跳过 APPROVAL 态的息屏逻辑）
- 审批**安全关键**，事件用 `app_event_post_important()` 入队（满则等 100ms，不丢弃）
- 审批可以打断录音（先停流再切审批页），避免资源泄漏

### 2.3 UI 渲染 (`main/app_ui.c:254-276`, `app_ui.c:404-412`)

**构建审批界面** (LVGL 9):

```c
static void build_approval(void) {
    page_t *p = &s_pages[APP_ST_APPROVAL];
    p->root = lv_obj_create(NULL);   // 独立屏幕对象
    
    // 风险条横幅（根据 risk 等级变色：low=绿 / medium=黄 / high=红）
    p->ap_risk_banner = block(p->root, 20, CONTENT_Y + 8, 200, 28, UI_GRASS);
    p->ap_risk_label = label(p->ap_risk_banner, "", &lv_font_montserrat_14, UI_INK, ...);
    
    // 标题 (title)
    p->ap_title = label(p->root, "", &lv_font_montserrat_16, UI_INK, 20, 60, 200);
    
    // 目标 (target: ...)
    p->ap_target = label(p->root, "", &lv_font_montserrat_14, UI_MUTED, 20, 92, 200);
    
    // Diff 摘要 (diffSummary, 自动换行, 88px 高)
    p->ap_diff = label(p->root, "", &lv_font_montserrat_14, UI_MUTED, 20, 176, 200);
    lv_label_set_long_mode(p->ap_diff, LV_LABEL_LONG_WRAP);
    lv_obj_set_height(p->ap_diff, 88);
}
```

**快照驱动更新** (只更新变化的字段):

```c
case APP_ST_APPROVAL: {
    uint8_t r = snap->approval_risk < APP_RISK_COUNT ? snap->approval_risk : APP_RISK_MEDIUM;
    
    // 更新风险条颜色
    lv_obj_set_style_bg_color(s_pages[APP_ST_APPROVAL].ap_risk_banner, RISK_COLORS[r], 0);
    
    // 只在内容变化时更新标签（label_set_if_changed 内部比较字符串，避免无谓重绘）
    label_set_if_changed(s_pages[APP_ST_APPROVAL].ap_risk_label, RISK_NAMES[r]);
    label_set_if_changed(s_pages[APP_ST_APPROVAL].ap_title, snap->approval_title);
    label_set_fmt_if_changed(s_pages[APP_ST_APPROVAL].ap_target, "target: %s", snap->approval_target);
    label_set_if_changed(s_pages[APP_ST_APPROVAL].ap_diff, snap->approval_diff);
    break;
}
```

**快照结构** (`app_types.h:143-163`):

```c
typedef struct {
    app_stage_t    state;                  // 当前状态
    bool           link_up;                // 链路是否通
    bool           screen_on;              // 屏幕背光
    bool           panel_on;               // 面板供电
    bool           net_busy;               // 音频丢帧中
    // ... (省略电池、链路名等)
    
    // 审批相关字段
    char           task_id[APP_TASK_ID_MAX];            // 32
    char           approval_title[APP_TITLE_MAX];       // 64
    char           approval_target[APP_TARGET_MAX];     // 64
    char           approval_diff[APP_DIFF_MAX];         // 64
    uint8_t        approval_risk;                       // app_risk_t enum
    
    uint32_t       elapsed_ms;             // 状态持续时长
    char           toast[APP_TOAST_MAX];   // 64
} app_ui_snapshot_t;
```

**核心思想**: 状态机维护内部状态 → 定期生成快照 → UI 层拿新旧快照做 diff → 只更新变化部分 → 解耦 + 可测试

### 2.4 PC 端 companion (`companion/relay.py:849-881`)

**审批 demo 实现**:

```python
async def _demo_approval(self):
    """转写注入后模拟 agent 工作流: 发审批请求, 等设备按键决策。"""
    try:
        waiter = asyncio.Event()
        self._approval_waiter = waiter
        
        # 发送审批请求到设备
        req = {
            "type": "agent.approval_request",
            "taskId": "task-001",
            "title": "Deploy to production",
            "target": "api.example.com",
            "diffSummary": "+12 -3 in deploy.sh",
            "riskLevel": "high",
        }
        await self._send_ctrl(req)   # 通过 BLE GATT CTRL 特征发送 JSON
        print("[approval] 已发审批请求, 等设备按键决策(●/▲/▼)...")
        
        # 等待设备按键回传 agent.action 事件
        try:
            await asyncio.wait_for(waiter.wait(), timeout=self.timeout)   # 默认 60s
        except asyncio.TimeoutError:
            print(f"[approval] {self.timeout:.0f}s 未收到 agent.action", file=sys.stderr)
        else:
            # 设备按键决策后进 AGENT_RUNNING, 需补发 agent.status done
            await self._send_agent_done()
            
    except Exception as e:
        print(f"[approval] 审批流程异常: {e}", file=sys.stderr)
    finally:
        self._approval_waiter = None
        self._approval_task = None
```

**设备回传的 agent.action 事件处理** (`relay.py:709-710`):

```python
# 在接收到设备上行的 {"event": "agent.action", "taskId": "...", "action": "approve"} 时
if self._approval_waiter is not None:
    self._approval_waiter.set()   # 唤醒等待，继续工作流
```

**关键点**:
- 这只是一个 **demo**，不是真正对接 Claude Code
- 真实场景需要 PC 端接收 Claude Code 的 `AskUserQuestion` hook 回调，提取选项和上下文，组装成 `approval_request` 发给设备
- 设备按键后，PC 端收到 `agent.action` 事件，需要回调 Claude Code 的决策 API（如 `answer_question()` 或类似接口）

---

## 3. 对 CodeBuddy AI BOX 的借鉴

### 3.1 已完成的部分（2026-09-02）

你的决策交互（0x0A/0x0B）已基本对齐这个架构：

| FoloToy AI Passport | CodeBuddy AI BOX (52 例) | 状态 |
|---|---|---|
| `agent.approval_request` JSON | `decision_request_frame_t` 二进制帧 (0x0A) | ✅ 已实现 |
| `agent.action` JSON | `decision_reply_frame_t` 二进制帧 (0x0B) | ✅ 已实现 |
| APPROVAL 状态 | `g_decision_active` 标志位 | ✅ 已实现 |
| 触摸选项 + 确认键 | 触摸选项 + 确认键 | ✅ 已实现 |
| 60s 超时发 0xFF | 60s 超时发 0xFF | ✅ 已实现 |
| 审批态常亮 | （未实现） | ❌ 待补充 |

### 3.2 需要补充的设计（参考 FoloToy）

#### **A. 审批态防息屏**

FoloToy 的审批态**永不熄屏**（`app_state.c:454-459`）：

```c
// APPROVAL 态跳过息屏逻辑
if (s->state == APP_ST_APPROVAL) {
    return;   // 不处理息屏计时器
}
```

**建议**: 在 `51_mic_wifi.ino` 的主循环心跳中，`g_decision_active` 为 true 时跳过背光关闭逻辑。

#### **B. 审批打断其他操作**

FoloToy 的审批可以**打断录音**，先停流再切审批页（避免资源泄漏）。

**建议**: 在 `espnow_recv_cb()` 收到 0x0A 时，检查当前是否在录音（`audio_streaming` 标志），先停止音频流再设置 `g_decision_pending = true`。

#### **C. 风险等级可视化**

FoloToy 有三级风险（low/medium/high），UI 上用不同颜色横幅：

- **Low**: 绿色 (`UI_GRASS`)
- **Medium**: 黄色
- **High**: 红色 (`UI_CRIMSON`)

**建议**: 在 `decision_request_frame_t` 中加一个 `uint8_t risk` 字段（0=low, 1=medium, 2=high），Screen 7 根据 risk 改变标题背景色。

#### **D. 纯状态机重构（长期）**

FoloToy 的状态机是纯函数 `reduce(state, event) → action[]`，零硬件依赖，可在 PC 上跑单测。

你的项目目前是**直接操作型**（主循环里直接调 `lv_scr_load()` / `digitalWrite()`），耦合度较高。

**长期优化方向**:
1. 定义 `app_state_t` 结构（当前屏幕、决策状态、音频状态、按键去抖等）
2. 所有输入（按键/触摸/ESP-NOW）转成 `app_event_t` 入队
3. 主循环出队 → `app_state_reduce(s, ev) → actions[]` → 执行 action（播音/切屏/发帧）
4. 状态机逻辑提取成独立 `.c`，可在 PC 上用 GTest 跑单测

**好处**: 逻辑与硬件解耦，便于测试、维护、移植（换硬件只需重写 action 执行层）。

---

## 4. PC 端集成 Claude Code 的路径

FoloToy **没有对接 Claude Code**，它只是一个语音输入硬件 + 审批 demo。

要真正实现 "Claude Code 决策透明可控"，需要：

### 4.1 架构设计

```
Claude Code (CLI/Desktop)
    ↓ (hook: PreToolUse / AskUserQuestion)
Python 桥接脚本 (监听 hook 回调)
    ↓ (串口或 BLE)
Dongle (ESP-NOW 转发)
    ↓ (ESP-NOW)
ATK BOX (52 例, 决策界面)
    ↓ (触摸选择 + 确认)
回传决策结果 (0x0B)
    ↓
Python 桥接脚本 (调用 Claude Code API 回传答案)
    ↓
Claude Code 继续执行
```

### 4.2 需要的组件

1. **Claude Code Hook 监听器** (Python)
   - 订阅 `PreToolUse` 或 `AskUserQuestion` hook
   - 提取工具名/参数/选项 → 组装 `decision_request_frame_t`
   - 通过串口或 BLE 发给 Dongle

2. **Dongle 固件增强**
   - 接收 PC 端串口/USB 数据 → 转成 ESP-NOW 0x0A 帧发给 BOX
   - 接收 BOX 的 ESP-NOW 0x0B 回传 → 转成串口数据发给 PC

3. **Python 桥接脚本回传逻辑**
   - 收到 BOX 的决策结果（选项索引或 0xFF）
   - 调用 Claude Code API（如 `answer_question(selected_index)`）
   - 解除 hook 阻塞，Claude 继续执行

### 4.3 实现步骤（6 天计划，参考你的小智 AI 集成方案）

| 天 | 任务 | 产出 |
|---|---|---|
| D1 | 搭建 Python 桥接脚本框架（串口收发 + hook 订阅） | `pc_bridge.py` 初版 |
| D2 | Dongle 固件：串口 ↔ ESP-NOW 透传（0x0A/0x0B） | Dongle 支持 PC 上行链路 |
| D3 | PC 桥接：hook 回调 → 组装 0x0A → 发 Dongle → 等回传 | 单向下发测试通过 |
| D4 | PC 桥接：收 0x0B → 解析 → 回传 Claude Code | 双向闭环打通 |
| D5 | 端到端测试：Claude 执行 Bash → BOX 弹审批 → 批准 → 命令执行 | 完整链路验证 |
| D6 | 文档 + 用户指南（启动桥接脚本 + Claude 配置） | 交付可用方案 |

**关键技术点**:
- Claude Code Hook API 文档（需查阅官方 SDK）
- Dongle 串口协议：帧头/CRC/长度（建议复用 ESP-NOW 的帧格式，PC 端直接透传）
- 超时处理：PC 端等待 60s，BOX 端超时发 0xFF，PC 端收到后调用 `cancel_question()`

---

## 5. 关键差异对比

| 维度 | FoloToy AI Passport | CodeBuddy AI BOX (你的项目) |
|---|---|---|
| **硬件** | ESP32-C3 (400KB SRAM, 单核) | ESP32-S3 (8MB PSRAM, 双核) |
| **屏幕** | 1.47" 172x320 圆角 LCD | 240x320 ST7789 方屏 |
| **UI 框架** | LVGL 9.5 | LVGL 8.3 |
| **状态机** | 纯 C reducer (772 行, 8 组单测) | 直接操作型 (主循环直接调 LVGL) |
| **协议** | JSON over BLE/USB | 二进制帧 over ESP-NOW |
| **审批交互** | OK 批准 / UP 拒绝 / DOWN 详情 | 触摸选项 + 确认键 (无风险等级) |
| **PC 端** | Python companion (BLE/USB 双通道) | 需自建 Python 桥接脚本 |
| **Claude Code 集成** | ❌ 无（仅 demo） | ⏸ 部分完成（0x0A/0x0B 已实现，PC 桥未实现） |

---

## 6. 下一步行动建议

### 短期（本周内）

1. **补充审批态防息屏**  
   在 `51_mic_wifi.ino` 主循环里，`g_decision_active` 为 true 时跳过背光关闭逻辑。

2. **添加风险等级可视化**  
   `decision_request_frame_t` 加 `uint8_t risk` 字段，Screen 7 标题背景根据 risk 变色（绿/黄/红）。

3. **端到端实测决策链路**  
   用 `pc_tools/send_decision.py` 发 0x0A → BOX 弹决策界面 → 触摸选择 → 确认 → 观察 Dongle 串口是否收到 0x0B 回传。

### 中期（1-2 周）

4. **搭建 Python 桥接脚本**  
   参考 FoloToy 的 `relay.py`，实现串口/USB 与 Dongle 通信，组装/解析 0x0A/0x0B 帧。

5. **Dongle 固件增强**  
   实现 PC 串口 → ESP-NOW 0x0A 下发，ESP-NOW 0x0B → PC 串口回传。

6. **研究 Claude Code Hook API**  
   查阅官方文档，确认 `PreToolUse` / `AskUserQuestion` 的订阅方式和回传接口。

### 长期（1 个月+）

7. **状态机重构**  
   借鉴 FoloToy 的纯 C reducer 设计，提取状态逻辑到独立模块，增加单测覆盖。

8. **多设备支持**  
   K10 版本（51 例）和 ATK BOX 版本（52 例）共享状态机代码，只有硬件驱动层不同。

9. **完整 Claude Code 集成**  
   实现 Claude → PC 桥接 → Dongle → BOX → 回传 Claude 的完整闭环，写用户指南和演示视频。

---

## 7. 参考资料

- FoloToy AI Passport 仓库: https://github.com/zhaohuaxiaoy/folo-ai-passport-voice
- 核心文件:
  - `main/app_types.h` (类型定义, 248 行)
  - `main/app_state.c` (状态机, 772 行)
  - `main/app_protocol.c` (协议编解码, 229 行)
  - `main/app_ui.c` (UI 渲染, 418 行)
  - `companion/relay.py` (PC 端 relay, 1304 行)
- 你的项目已完成的相关 commit:
  - `1d406cd` 协议加 0x0A/0x0B 决策帧
  - `e27bd07` espnow_recv_cb 收 0x0A 决策请求
  - `749ea83` 决策界面 screen 7
  - `1cf4442` send_decision_reply 组 0x0B 回传
  - `8142813` 主循环决策界面接线

---

**撰写人**: Claude Code  
**审阅**: 待用户确认

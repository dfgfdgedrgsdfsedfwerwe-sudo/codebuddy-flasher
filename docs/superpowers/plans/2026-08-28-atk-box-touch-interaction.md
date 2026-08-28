# ATK BOX 交互重定义 实现计划（Phase 1 + Phase 2）

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 ATK BOX 的 3 物理按键 + 单点触摸屏重新定义为"按键专注语音、触摸主导浏览"的交互模型。

**Architecture:** 物理按键在 `loop()` 的按键处理块中重新分工（A=录音/Esc，B=Enter/Backspace，BOOT=返回主界面/TouchTest）；界面内交互改用 LVGL 原生事件回调（`LV_EVENT_CLICKED` / `LV_EVENT_LONG_PRESSED`）挂在各界面对象上；全局左右滑动保持现有 `touch.scan()` 手动检测不变。

**Tech Stack:** Arduino (PlatformIO), ESP32-S3, LVGL v8, LovyanGFX, CHSC5432 触摸（已注册为 LVGL POINTER indev）。

## Global Constraints

- 编译目录**只能是** `examples/52_codebuddy_ai_box`（下划线）。带空格的 `52_code_buddy_AI BOX` 是过时副本，禁止改。
- 编译命令：`cd C:/Users/4090/Desktop/dfk10_arduino_demo-master && python -m platformio run -e 52_codebuddy_ai_box`（`pio` 不在 PATH，必须用 `python -m platformio`）。
- 烧录：`python -m platformio run -t upload -e 52_codebuddy_ai_box --upload-port COM11`；串口输出在 **COM12**。
- LVGL 是 **v8**，禁止 v9 API（`lv_draw_buf_t` 除外——现有 TouchTest 已用，勿动）。
- 所有在 `loop()` 之外或事件回调里的 LVGL 操作必须持有 `xGuiSemaphore`；**但** LVGL 事件回调（`LV_EVENT_*`）由 `lv_task_handler()` 在已持锁上下文触发，回调内**不要**再取 `xGuiSemaphore`（会死锁）。
- 长按阈值统一 `LONG_PRESS_MS = 600`（物理键，现有值）；触摸长按用 LVGL indev 默认（400ms），本计划不改 indev 配置。
- 演示模式触发方式不变：A+B 同时按住 2 秒（`DEMO_TRIGGER_MS`）。
- 无单元测试框架。每个任务的"测试"= 编译通过 + 烧录后按串口输出/屏幕行为核对。
- Git 身份未配置，提交用：`git -c user.email="codebuddy@local" -c user.name="CodeBuddy" commit -m "..."`。

**目标文件（唯一）**：`examples/52_codebuddy_ai_box/51_mic_wifi.ino`

## 现有对象参考（Phase 2 事件回调挂载点）

- 界面 1 Token：`token_row_t token_rows[TOKEN_MAX_ITEMS]`，成员 `.name_label .pct_label .bar .amount_label`
- 界面 2 Project：`project_row_t project_rows[PROJECT_MAX_ITEMS]`，成员 `.dot .name_label .status_label`
- 界面 3 Inspo：`label_inspo_title` `label_inspo_content`；辅助函数 `start_typing_animation()`
- 界面 4 Profile：`profile_image`（`lv_img`）；`profile_index` / `profile_count` / `update_screen_profile()`
- 界面 5 AI：`ai_status_image`（`lv_img`）、`label_ai_status_text`、`ai_state`（`ai_state_t`）、`apply_ai_state()`

---

## 阶段范围

- **Phase 1（本计划 Task 1-4）**：物理按键重定义 + 文档同步。产出即可用：A 录音/Esc、B Enter/Backspace、BOOT 返回主界面/TouchTest。
- **Phase 2（本计划 Task 5-9）**：各界面 LVGL 触摸事件（单击/长按），含通用详情卡片组件。
- **Phase 3/4（不在本计划）**：虚拟键盘+NVS、照片管理界面 7、设置界面 8 — 另立计划。

---

# Phase 1 — 物理按键重定义

### Task 1: 按键 A 长按改为 Esc（原 Enter）

**Files:**
- Modify: `examples/52_codebuddy_ai_box/51_mic_wifi.ino`（约 1533-1536 行，A 处理块的正常模式 `else` 分支长按部分）

**Interfaces:**
- Consumes: `send_key(uint8_t keycode, const char *name)`（现有）、`LONG_PRESS_MS`
- Produces: 无新符号（仅改行为）

- [ ] **Step 1: 修改长按分支**

把正常模式 `else` 分支里的长按动作从 Enter 改为 Esc：

```cpp
                } else {
                    // 长按: Esc 取消 (UX 重定义: 原 Enter 移到按键 B 短按)
                    send_key(0x29, "Esc (cancel)");
                }
```

- [ ] **Step 2: 编译**

Run: `cd C:/Users/4090/Desktop/dfk10_arduino_demo-master && python -m platformio run -e 52_codebuddy_ai_box`
Expected: `SUCCESS`，无报错

- [ ] **Step 3: 提交**

```bash
git -c user.email="codebuddy@local" -c user.name="CodeBuddy" add "examples/52_codebuddy_ai_box/51_mic_wifi.ino"
git -c user.email="codebuddy@local" -c user.name="CodeBuddy" commit -m "feat(52): 按键A长按改为Esc取消"
```

---

### Task 2: 按键 B 短按改为 Enter（原切换界面）

**Files:**
- Modify: `examples/52_codebuddy_ai_box/51_mic_wifi.ino`（约 1563-1571 行，B 处理块释放分支）

**Interfaces:**
- Consumes: `send_key()`、`LONG_PRESS_MS`
- Produces: 无新符号

- [ ] **Step 1: 修改 B 短按释放分支**

界面切换交给触摸滑动，B 短按改发 Enter：

```cpp
        } else if (!b && prev_b) {
            uint32_t duration = now - press_time_b;
            if (duration < LONG_PRESS_MS) {
                // 短按: Enter 确认 (UX 重定义: 界面切换已交给触摸左右滑动)
                send_key(0x28, "Enter (confirm)");
            }
            // 长按已在持续期间处理 (连续 Backspace)，释放时不再动作
        }
```

- [ ] **Step 2: 编译**

Run: `cd C:/Users/4090/Desktop/dfk10_arduino_demo-master && python -m platformio run -e 52_codebuddy_ai_box`
Expected: `SUCCESS`

- [ ] **Step 3: 提交**

```bash
git -c user.email="codebuddy@local" -c user.name="CodeBuddy" add "examples/52_codebuddy_ai_box/51_mic_wifi.ino"
git -c user.email="codebuddy@local" -c user.name="CodeBuddy" commit -m "feat(52): 按键B短按改为Enter,切屏交给触摸"
```

---

### Task 3: 按键 BOOT 短按改为返回主界面（原 Esc）

**Files:**
- Modify: `examples/52_codebuddy_ai_box/51_mic_wifi.ino`（约 1583-1600 行，BOOT 处理块释放分支）

**Interfaces:**
- Consumes: `switch_screen(uint8_t)`、`SCREEN_ORDER[7]`、`order_index`、`LONG_PRESS_MS`
- Produces: 无新符号

说明：主界面 = AI Status = 界面 5 = `SCREEN_ORDER[0]`。返回主界面时须把 `order_index` 复位到 0，保证之后左右滑动顺序正确。

- [ ] **Step 1: 修改 BOOT 释放分支**

```cpp
        } else if (!boot && prev_boot) {
            uint32_t duration = now - press_time_boot;
            if (duration < LONG_PRESS_MS) {
                // 短按 BOOT: 返回主界面 (AI Status = SCREEN_ORDER[0] = 界面 5)
                order_index = 0;
                Serial.println("BOOT short -> return to main (AI Status)");
                switch_screen(SCREEN_ORDER[0]);
            } else {
                // 长按 BOOT: 跳转触摸测试界面 (保持不变)
                Serial.println("BOOT long press -> Jump to TouchTest screen");
                for (uint8_t i = 0; i < 7; i++) {
                    if (SCREEN_ORDER[i] == 6) {  // 界面 6 = TouchTest
                        order_index = i;
                        break;
                    }
                }
                switch_screen(6);
            }
        }
```

- [ ] **Step 2: 编译**

Run: `cd C:/Users/4090/Desktop/dfk10_arduino_demo-master && python -m platformio run -e 52_codebuddy_ai_box`
Expected: `SUCCESS`

- [ ] **Step 3: 烧录并验证物理按键**

Run: `python -m platformio run -t upload -e 52_codebuddy_ai_box --upload-port COM11`
串口看 COM12。手动核对：
- 短按 A → 串口 `F2 (voice input)` + `Streaming STARTED/STOPPED`
- 长按 A → 串口 `Esc (cancel)`
- 短按 B → 串口 `Enter (confirm)`（屏幕**不**切换）
- 长按 B → 串口连续 `Backspace (repeat)`
- 短按 BOOT → 屏幕跳回 AI Status（界面 5）+ 串口 `BOOT short -> return to main`
- 长按 BOOT → 跳 TouchTest（界面 6）
- 左右滑动 → 界面按 5→2→1→3→4→6→0 切换

- [ ] **Step 4: 提交**

```bash
git -c user.email="codebuddy@local" -c user.name="CodeBuddy" add "examples/52_codebuddy_ai_box/51_mic_wifi.ino"
git -c user.email="codebuddy@local" -c user.name="CodeBuddy" commit -m "feat(52): BOOT短按返回主界面,长按保持跳TouchTest"
```

---

### Task 4: 同步头部注释与 CLAUDE.md 按键表

**Files:**
- Modify: `examples/52_codebuddy_ai_box/51_mic_wifi.ino`（16-22 行头部注释块）
- Modify: `examples/52_code_buddy_AI BOX/CLAUDE.md`（按键映射表；注意这是文档，允许改）

- [ ] **Step 1: 更新 .ino 头部注释**

```cpp
 * 按键交互 (ATK BOX, UX 重定义 2026-08-28):
 * - 按键 A (KEY1): 短按=切换音频流+发送F2, 长按=发送Esc(取消)
 * - 按键 B (KEY0): 短按=发送Enter(确认), 长按=连续发送Backspace
 * - 按键 C (BOOT): 短按=返回主界面(AI Status), 长按=跳转触摸测试界面
 * - RST: 硬件复位 (不可作功能键)
 * - A+B 同时 2 秒: 触发演示模式
 * - 触摸: 左右滑动切换界面 + 界面内单击/长按交互
```

- [ ] **Step 2: 更新 CLAUDE.md 按键映射表**

将 CLAUDE.md "Button Mapping" / "Button Interaction" 段落里的旧表替换为：

```markdown
| 按键 | 短按 (<600ms) | 长按 (≥600ms) |
|------|--------------|--------------|
| KEY1 (A) | 切换录音 (F2 + 音频流) | Esc 取消 |
| KEY0 (B) | Enter 确认 | 连续 Backspace |
| BOOT (C) | 返回主界面 (AI Status) | 进入 TouchTest |

A+B 同时按 2 秒 = 演示模式。界面切换：触摸左右滑动。
```

- [ ] **Step 3: 提交**

```bash
git -c user.email="codebuddy@local" -c user.name="CodeBuddy" add "examples/52_codebuddy_ai_box/51_mic_wifi.ino" "examples/52_code_buddy_AI BOX/CLAUDE.md"
git -c user.email="codebuddy@local" -c user.name="CodeBuddy" commit -m "docs(52): 同步按键重定义到头部注释与CLAUDE.md"
```

---

# Phase 2 — 界面触摸事件

> **实现要点**：Phase 2 用 LVGL v8 原生事件。给对象加 `lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE)`（label 默认不可点击），再 `lv_obj_add_event_cb(obj, cb, LV_EVENT_CLICKED, user_data)` 或 `LV_EVENT_LONG_PRESSED`。回调内**不取** `xGuiSemaphore`（已在 `lv_task_handler` 持锁上下文）。事件回调需在对应 `create_screen_*()` 里注册一次。

### Task 5: 通用详情卡片组件（供界面 1/2 复用）

**Files:**
- Modify: `examples/52_codebuddy_ai_box/51_mic_wifi.ino`（在前向声明区 159 行附近加声明；在界面创建函数之前加实现）

**Interfaces:**
- Produces:
  - `void show_detail_card(const char *title, const char *body);` — 创建全屏半透明遮罩 + 居中卡片，显示标题与多行正文；点击遮罩关闭
  - `void close_detail_card();` — 删除卡片（`lv_obj_del`）并置空指针
  - `bool detail_card_open();` — 返回卡片是否打开（供滑动手势拦截用）
  - 全局：`static lv_obj_t *detail_card_bg = NULL;`

- [ ] **Step 1: 加全局指针与前向声明**

在 121 行（`screen_touch_test` 定义后）附近加：

```cpp
static lv_obj_t *detail_card_bg = NULL;   // 详情卡片遮罩层 (NULL=未打开)
```

在 159 行 `switch_screen` 前向声明后加：

```cpp
static void show_detail_card(const char *title, const char *body);
static void close_detail_card();
static bool detail_card_open();
static void detail_card_bg_event(lv_event_t *e);
```

- [ ] **Step 2: 实现卡片函数**

放在 `switch_screen` 函数定义之前（约 1178 行前）：

```cpp
static bool detail_card_open() { return detail_card_bg != NULL; }

static void close_detail_card() {
    if (detail_card_bg) {
        lv_obj_del(detail_card_bg);   // 删遮罩会连带删子对象(卡片)
        detail_card_bg = NULL;
    }
}

// 点击遮罩空白处关闭；点击卡片本体不关闭
static void detail_card_bg_event(lv_event_t *e) {
    lv_obj_t *target = lv_event_get_target(e);
    if (target == detail_card_bg) {
        close_detail_card();
    }
}

static void show_detail_card(const char *title, const char *body) {
    close_detail_card();  // 先关旧的，避免叠加占用 PSRAM

    // 半透明遮罩 (覆盖当前活动屏幕)
    detail_card_bg = lv_obj_create(lv_scr_act());
    lv_obj_set_size(detail_card_bg, 240, 320);
    lv_obj_align(detail_card_bg, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(detail_card_bg, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(detail_card_bg, LV_OPA_60, 0);
    lv_obj_set_style_border_width(detail_card_bg, 0, 0);
    lv_obj_set_style_radius(detail_card_bg, 0, 0);
    lv_obj_clear_flag(detail_card_bg, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(detail_card_bg, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(detail_card_bg, detail_card_bg_event, LV_EVENT_CLICKED, NULL);

    // 卡片本体
    lv_obj_t *card = lv_obj_create(detail_card_bg);
    lv_obj_set_size(card, 210, 240);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1F2937), 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x374151), 0);

    lv_obj_t *lbl_title = lv_label_create(card);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_label_set_text(lbl_title, title);

    lv_obj_t *lbl_body = lv_label_create(card);
    lv_obj_set_style_text_font(lbl_body, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_body, lv_color_hex(0xD1D5DB), 0);
    lv_label_set_long_mode(lbl_body, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl_body, 186);
    lv_obj_align(lbl_body, LV_ALIGN_TOP_LEFT, 0, 34);
    lv_label_set_text(lbl_body, body);
}
```

- [ ] **Step 3: 编译**

Run: `cd C:/Users/4090/Desktop/dfk10_arduino_demo-master && python -m platformio run -e 52_codebuddy_ai_box`
Expected: `SUCCESS`（此时卡片还没被调用，仅验证编译）

- [ ] **Step 4: 提交**

```bash
git -c user.email="codebuddy@local" -c user.name="CodeBuddy" add "examples/52_codebuddy_ai_box/51_mic_wifi.ino"
git -c user.email="codebuddy@local" -c user.name="CodeBuddy" commit -m "feat(52): 通用详情卡片组件(遮罩+居中卡片,点击外部关闭)"
```

---

### Task 6: 滑动手势拦截 — 卡片打开时不切界面

**Files:**
- Modify: `examples/52_codebuddy_ai_box/51_mic_wifi.ino`（约 1604-1644 行，滑动手势块）

**Interfaces:**
- Consumes: `detail_card_open()`（Task 5）

- [ ] **Step 1: 在滑动块开头加拦截**

在 `{ static int16_t swipe_start_x ...` 块内、`touch.scan` 之前加：

```cpp
        // 详情卡片打开时，触摸交给 LVGL 处理(点击遮罩关闭)，不做滑动切屏
        if (detail_card_open()) {
            swipe_in_progress = false;
            swipe_start_x = -1;
        } else {
```

并在该块末尾（`}` 之前）补上对应的闭合 `}`。完整结构：

```cpp
    {
        static int16_t swipe_start_x = -1;
        static int16_t swipe_start_y = -1;
        static bool swipe_in_progress = false;

        if (detail_card_open()) {
            swipe_in_progress = false;
            swipe_start_x = -1;
        } else {
            uint16_t x, y;
            bool is_pressed = touch.scan(&x, &y);
            // ... 原有滑动检测逻辑保持不变 ...
        }
    }
```

- [ ] **Step 2: 编译**

Run: `cd C:/Users/4090/Desktop/dfk10_arduino_demo-master && python -m platformio run -e 52_codebuddy_ai_box`
Expected: `SUCCESS`

- [ ] **Step 3: 提交**

```bash
git -c user.email="codebuddy@local" -c user.name="CodeBuddy" add "examples/52_codebuddy_ai_box/51_mic_wifi.ino"
git -c user.email="codebuddy@local" -c user.name="CodeBuddy" commit -m "feat(52): 详情卡片打开时拦截滑动切屏"
```

---

### Task 7: 界面 2 Project — 点击行看详情 + 点击圆点切状态

**Files:**
- Modify: `examples/52_codebuddy_ai_box/51_mic_wifi.ino`（`create_screen_project()` 约 543-590 行加事件注册；新增回调函数）

**Interfaces:**
- Consumes: `project_rows[]`、`project_data`、`show_detail_card()`、`status_map`（现有状态→颜色表）
- Produces:
  - `static void project_row_event(lv_event_t *e);` — 点击行弹详情
  - `static void project_dot_event(lv_event_t *e);` — 点击圆点循环状态

说明：用 `lv_event_get_user_data` 传行索引（`(void*)(intptr_t)i`）。状态循环 0→1→2→3→5→0（跳过 4 Error，Idle=5 后回 Planning）。改状态后置 `screen_dirty=true` 让主循环刷新（回调内不直接调 update 以复用现有刷新路径）。

- [ ] **Step 1: 加回调函数**（放在 `create_screen_project` 之前）

```cpp
static void project_row_event(lv_event_t *e) {
    uint8_t i = (uint8_t)(intptr_t)lv_event_get_user_data(e);
    if (!project_data_valid || i >= project_data.count) return;
    const char *name = project_data.items[i].name;
    static char body[96];
    uint8_t code = project_data.items[i].status_code;
    const char *st = (code < 6) ? status_map[code].text : "Unknown";
    snprintf(body, sizeof(body), "Status: %s\nProject #%d of %d", st, i + 1, project_data.count);
    show_detail_card(name, body);
}

static void project_dot_event(lv_event_t *e) {
    uint8_t i = (uint8_t)(intptr_t)lv_event_get_user_data(e);
    if (!project_data_valid || i >= project_data.count) return;
    uint8_t c = project_data.items[i].status_code;
    // 循环: Planning(0)->Coding(1)->Review(2)->Completed(3)->Idle(5)->Planning(0)
    switch (c) {
        case 0: c = 1; break;
        case 1: c = 2; break;
        case 2: c = 3; break;
        case 3: c = 5; break;
        default: c = 0; break;
    }
    project_data.items[i].status_code = c;
    Serial.printf("Project %d status -> %d\n", i, c);
    screen_dirty = true;   // 主循环 update_screen_project() 会重绘圆点与文字
}
```

- [ ] **Step 2: 在 create_screen_project 的建行循环里注册事件**

在每行创建 `.dot` 与 `.name_label` 之后（约 565-576 行之间）加：

```cpp
        // 圆点命中区放大到 40x40 (视觉仍 10px)，点击切状态
        lv_obj_set_ext_click_area(project_rows[i].dot, 15);
        lv_obj_add_flag(project_rows[i].dot, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(project_rows[i].dot, project_dot_event,
                            LV_EVENT_CLICKED, (void*)(intptr_t)i);

        // 点击项目名弹详情
        lv_obj_add_flag(project_rows[i].name_label, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(project_rows[i].name_label, project_row_event,
                            LV_EVENT_CLICKED, (void*)(intptr_t)i);
```

- [ ] **Step 3: 编译**

Run: `cd C:/Users/4090/Desktop/dfk10_arduino_demo-master && python -m platformio run -e 52_codebuddy_ai_box`
Expected: `SUCCESS`

- [ ] **Step 4: 烧录并验证**

Run: `python -m platformio run -t upload -e 52_codebuddy_ai_box --upload-port COM11`
A+B 2 秒进演示模式 → 滑到界面 2：
- 点击项目名 → 弹详情卡片 → 点击卡片外关闭
- 点击圆点 → 圆点颜色+状态文字循环变化，串口 `Project N status -> M`

- [ ] **Step 5: 提交**

```bash
git -c user.email="codebuddy@local" -c user.name="CodeBuddy" add "examples/52_codebuddy_ai_box/51_mic_wifi.ino"
git -c user.email="codebuddy@local" -c user.name="CodeBuddy" commit -m "feat(52): 界面2点击项目名看详情+点击圆点切状态"
```

---

### Task 8: 界面 3 Inspo 单击 + 界面 4 Profile 单击

**Files:**
- Modify: `examples/52_codebuddy_ai_box/51_mic_wifi.ino`（`create_screen_inspo`、`create_screen_profile` 加事件；新增回调）

**Interfaces:**
- Consumes: `label_inspo_title`、`label_inspo_content`、`start_typing_animation()`、`inspo_typing_active`、`inspo_full_text`、`profile_image`、`profile_index`、`profile_count`、`update_screen_profile()`
- Produces:
  - `static void inspo_title_event(lv_event_t *e);` — 单击标题重启打字机
  - `static void inspo_body_event(lv_event_t *e);` — 单击正文暂停/继续
  - `static void profile_img_event(lv_event_t *e);` — 单击头像切换下一张

- [ ] **Step 1: 加回调函数**（放在 `create_screen_inspo` 之前）

```cpp
static void inspo_title_event(lv_event_t *e) {
    (void)e;
    start_typing_animation();   // 清空并重新逐字显示
    Serial.println("Inspo: typing restarted (tap title)");
}

static void inspo_body_event(lv_event_t *e) {
    (void)e;
    inspo_typing_active = !inspo_typing_active;   // 暂停/继续
    Serial.printf("Inspo: typing %s (tap body)\n", inspo_typing_active ? "resumed" : "paused");
}
```

放在 `create_screen_profile` 之前：

```cpp
static void profile_img_event(lv_event_t *e) {
    (void)e;
    if (profile_count > 1) {
        profile_index = (profile_index + 1) % profile_count;
        Serial.printf("Profile: switched to user %d (tap)\n", profile_index + 1);
        update_screen_profile();   // 回调在持锁上下文，可直接调
    }
}
```

- [ ] **Step 2: 在 create_screen_inspo 注册事件**

在 `label_inspo_title` 和 `label_inspo_content` 创建后加：

```cpp
    lv_obj_add_flag(label_inspo_title, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(label_inspo_title, inspo_title_event, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(label_inspo_content, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(label_inspo_content, inspo_body_event, LV_EVENT_CLICKED, NULL);
```

- [ ] **Step 3: 在 create_screen_profile 注册事件**

在 `profile_image = lv_img_create(...)` 创建后加：

```cpp
    lv_obj_add_flag(profile_image, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(profile_image, profile_img_event, LV_EVENT_CLICKED, NULL);
```

- [ ] **Step 4: 编译**

Run: `cd C:/Users/4090/Desktop/dfk10_arduino_demo-master && python -m platformio run -e 52_codebuddy_ai_box`
Expected: `SUCCESS`

- [ ] **Step 5: 烧录并验证**

演示模式下：界面 3 点标题重启打字/点正文暂停继续（串口有输出）；界面 4 点头像切换（`profile_count>1` 时生效）。

- [ ] **Step 6: 提交**

```bash
git -c user.email="codebuddy@local" -c user.name="CodeBuddy" add "examples/52_codebuddy_ai_box/51_mic_wifi.ino"
git -c user.email="codebuddy@local" -c user.name="CodeBuddy" commit -m "feat(52): 界面3单击标题/正文,界面4单击头像切换"
```

---

### Task 9: 界面 5 AI Status 单击切情绪 + 界面 1 Token 点击看详情

**Files:**
- Modify: `examples/52_codebuddy_ai_box/51_mic_wifi.ino`（`create_screen_ai_status`、`create_screen_token` 加事件；新增回调）

**Interfaces:**
- Consumes: `ai_status_image`、`ai_state`（`ai_state_t`）、`ai_state_changed`、`AI_STATE_COUNT`、`token_rows[]`、`token_data`、`show_detail_card()`
- Produces:
  - `static void ai_face_event(lv_event_t *e);` — 单击切情绪
  - `static void token_row_event(lv_event_t *e);` — 点击行看详情

说明：AI 情绪单击循环 Thinking→Coding→Done→Thinking，置 `ai_external_override=true` 暂停自动循环、刷新 `ai_last_ext_frame=millis()`（15 秒后自动恢复），并置 `ai_state_changed=true` 让刷新逻辑应用新表情。

- [ ] **Step 1: 加回调函数**

`create_screen_ai_status` 之前：

```cpp
static void ai_face_event(lv_event_t *e) {
    (void)e;
    ai_state = (ai_state_t)(((int)ai_state + 1) % AI_STATE_COUNT);
    ai_external_override = true;         // 暂停自动循环
    ai_last_ext_frame = millis();        // 15s 后恢复自动
    ai_state_changed = true;             // 触发表情应用
    Serial.printf("AI emotion -> %d (tap)\n", (int)ai_state);
}
```

`create_screen_token` 之前：

```cpp
static void token_row_event(lv_event_t *e) {
    uint8_t i = (uint8_t)(intptr_t)lv_event_get_user_data(e);
    if (!token_data_valid || i >= token_data.count) return;
    const char *name = token_data.items[i].name;
    uint16_t pct = token_data.items[i].percent_x10;
    static char body[96];
    snprintf(body, sizeof(body), "Usage: %u.%u%%\nUsed: %lu\nTotal: %lu",
             pct / 10, pct % 10,
             (unsigned long)token_data.items[i].used,
             (unsigned long)token_data.items[i].total);
    show_detail_card(name, body);
}
```

> 注意：`token_status_item_t` 的字段名（`used`/`total`/`percent_x10`）以 `espnow_protocol.h` 实际定义为准；实现前先 grep 确认，不符则按实际字段调整 `snprintf`。

- [ ] **Step 2: 在 create_screen_ai_status 注册事件**

`ai_status_image = lv_img_create(...)` 后加：

```cpp
    lv_obj_add_flag(ai_status_image, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ai_status_image, ai_face_event, LV_EVENT_CLICKED, NULL);
```

- [ ] **Step 3: 在 create_screen_token 建行循环注册事件**

每行 `.name_label` 创建后加：

```cpp
        lv_obj_add_flag(token_rows[i].name_label, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(token_rows[i].name_label, token_row_event,
                            LV_EVENT_CLICKED, (void*)(intptr_t)i);
```

- [ ] **Step 4: 确认 token 字段名**

Run: `grep -nE "percent_x10|used|total|typedef struct" examples/52_codebuddy_ai_box/espnow_protocol.h`
按实际字段名修正 Step 1 的 `token_row_event`。

- [ ] **Step 5: 编译**

Run: `cd C:/Users/4090/Desktop/dfk10_arduino_demo-master && python -m platformio run -e 52_codebuddy_ai_box`
Expected: `SUCCESS`

- [ ] **Step 6: 烧录并验证**

- 界面 5 点击云朵图 → 表情在 Thinking/Coding/Done 间切换，串口 `AI emotion -> N`
- 界面 1 演示模式点击某行服务名 → 弹详情卡片显示用量/已用/总量 → 点卡外关闭

- [ ] **Step 7: 提交**

```bash
git -c user.email="codebuddy@local" -c user.name="CodeBuddy" add "examples/52_codebuddy_ai_box/51_mic_wifi.ino"
git -c user.email="codebuddy@local" -c user.name="CodeBuddy" commit -m "feat(52): 界面5单击切AI情绪+界面1点击看Token详情"
```

---

## 未纳入本计划（Phase 3/4，另立计划）

- 长按"关注"标记（界面 1）、长按编辑菜单（界面 3）、长按暂停自动循环（界面 5）—— 需统一 `LV_EVENT_LONG_PRESSED` 处理，且要解决 label 长按与详情卡片长按的优先级，单列一个 Phase 3 计划。
- 虚拟键盘编辑用户名/AI 文字 + NVS 持久化。
- 照片管理界面（界面 7）、设置界面（界面 8，含亮度调节）。
- TouchTest 长按分级（1s 清轨迹 / 3s 校准菜单）—— 依赖 NVS 保存校准参数，随 Phase 3。
- ESP-NOW Status 界面点击 MAC/重置计数/长按重连。

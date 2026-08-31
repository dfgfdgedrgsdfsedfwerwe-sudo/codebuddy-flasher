# 决策桥 - 发射端 (K10) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** K10 收到 Dongle 转发的 0x0A 决策请求时, 弹出决策界面显示标题 + 选项, 用户纯触摸选择并确认, 组 0x0B 回执帧经 ESP-NOW 发回 Dongle。

**Architecture:** 在既有 `52_codebuddy_ai_box/51_mic_wifi.ino` 的 ESP-NOW 接收框架 (已支持 0x07/0x08/0x09) 上加 0x0A 分支。新建一个 LVGL 决策界面, 用既有 `touch.scan()` 做选项命中检测。选中后组 0x0B 帧, 用既有 ESP-NOW 发送路径回传。

**Tech Stack:** Arduino (PlatformIO), LVGL, LovyanGFX, CHSC5432 触摸 (AtkBoxTouch.h), ESP-NOW。

## Global Constraints

- 协议帧 (`espnow_protocol.h`) K10 与 Dongle 两侧**必须字节一致**; 本计划以设计文档第4节为唯一真源照抄, 与接收端计划 Task 1 完全相同的结构体。
- 所有 LVGL 操作 (除 lv_port_* 驱动) **必须持 `xGuiSemaphore`**; 否则界面切换卡死、短按误判长按 (项目既有约束, 见 CLAUDE.md)。
- ESP-NOW 单帧 ≤ 250 字节; 0x0A = 134 字节, 0x0B = 5 字节。
- CRC8: 多项式 0x07 初值 0x00, 用 `espnow_crc8()`。
- 决策界面纯触摸交互 (点选项高亮 + 点确认键发出), 无需物理按键。
- 决策界面显示时暂停界面轮换; 15s 无操作发 chosen_index=0xFF 超时回执。
- 构建: `python -m platformio run -e 52_codebuddy_ai_box` (需先把 platformio.ini 的 default_envs 指向 52; src_dir 跟随 default_envs)。

---

## 文件结构

- `examples/52_codebuddy_ai_box/espnow_protocol.h` — 修改: 加 0x0A/0x0B (照抄 spec, 与 Dongle 一致)
- `examples/52_codebuddy_ai_box/51_mic_wifi.ino` — 修改:
  - `espnow_recv_cb()` 加 0x0A 分支 (存请求 + 置 flag)
  - 新增 `create_screen_decision()` / `update_screen_decision()` LVGL 界面
  - 新增 `send_decision_reply(id, index)` 组 0x0B + ESP-NOW 发送
  - 主循环: 检测决策 flag → 切界面; 决策界面触摸命中检测; 超时处理

---
## Task 1: 协议扩展 (K10 端 espnow_protocol.h) — 与 Dongle 字节一致

**Files:**
- Modify: `examples/52_codebuddy_ai_box/espnow_protocol.h`

**Interfaces:**
- Produces: `FRAME_TYPE_DECISION_REQ = 0x0A`, `FRAME_TYPE_DECISION_REPLY = 0x0B`; `decision_request_frame_t` (134B), `decision_reply_frame_t` (5B); 宏 `DECISION_TITLE_LEN=32` / `DECISION_OPT_LEN=24` / `DECISION_MAX_OPTS=4`。**必须与接收端计划 Task 1 逐字节相同。**

- [ ] **Step 1: frame_type_t 枚举加两个类型**

在 `FRAME_TYPE_AI_STATE = 0x09,` 后追加:
```c
    FRAME_TYPE_DECISION_REQ   = 0x0A,  /* 决策请求 (PC→Dongle→K10) */
    FRAME_TYPE_DECISION_REPLY = 0x0B,  /* 决策回执 (K10→Dongle→PC) */
```

- [ ] **Step 2: AI 情绪帧之后、CRC8 函数之前, 加决策帧结构体 (照抄, 勿改)**

```c
/* ============================================================
 * 决策帧 (Claude Code ⇄ K10 双向触摸决策)
 * ============================================================ */
#define DECISION_TITLE_LEN   32
#define DECISION_OPT_LEN     24
#define DECISION_MAX_OPTS     4

typedef struct __attribute__((packed)) {
    uint8_t  frame_type;                 /* = FRAME_TYPE_DECISION_REQ */
    uint16_t decision_id;                /* 递增, 回执对齐 */
    uint8_t  kind;                       /* 0=工具权限 1=多选题 */
    char     title[DECISION_TITLE_LEN];
    uint8_t  opt_count;                  /* 有效选项数 (1~4) */
    char     opts[DECISION_MAX_OPTS][DECISION_OPT_LEN];
    uint8_t  crc8;
} decision_request_frame_t;              /* 134 字节 */

typedef struct __attribute__((packed)) {
    uint8_t  frame_type;    /* = FRAME_TYPE_DECISION_REPLY */
    uint16_t decision_id;
    uint8_t  chosen_index;  /* 0xFF=超时/取消 */
    uint8_t  crc8;
} decision_reply_frame_t;   /* 5 字节 */
```

- [ ] **Step 3: 编译验证 + 结构体大小**

在 setup() 开头临时加 (验证后删):
```c
Serial.printf("sizeof req=%d reply=%d\n", (int)sizeof(decision_request_frame_t), (int)sizeof(decision_reply_frame_t));
```
Run: `python -m platformio run -e 52_codebuddy_ai_box`
Expected: 编译通过。串口应打印 `sizeof req=134 reply=5` (与 Dongle 一致)。验证后删除。

- [ ] **Step 4: Commit**

```bash
git add examples/52_codebuddy_ai_box/espnow_protocol.h
git commit -m "feat(52): 协议加 0x0A/0x0B 决策帧 (与 Dongle 一致)"
```

---

## Task 2: espnow_recv_cb 加 0x0A 分支 (存请求 + 置 flag)

**Files:**
- Modify: `examples/52_codebuddy_ai_box/51_mic_wifi.ino`

**Interfaces:**
- Consumes: `decision_request_frame_t` (Task 1)。
- Produces: 全局 `static decision_request_frame_t g_decision_req;`, `static volatile bool g_decision_pending = false;`, `static volatile bool g_decision_active = false;`。供界面/主循环 (Task 3/5) 使用。

- [ ] **Step 1: 在全局变量区 (token_data 附近) 加决策状态变量**

```c
static decision_request_frame_t g_decision_req;
static volatile bool g_decision_pending = false;  // 收到新请求待切界面
static volatile bool g_decision_active  = false;  // 决策界面正显示中
static uint32_t g_decision_deadline_ms = 0;       // 超时时刻
static int g_decision_sel = -1;                   // 当前高亮选项 (-1=未选)
```

- [ ] **Step 2: 在 espnow_recv_cb 内 (与 0x07/0x08/0x09 分支同级) 加 0x0A 处理**

参考既有 `else if (frame_type == FRAME_TYPE_AI_STATE ...)` 分支之后加:
```c
    else if (frame_type == FRAME_TYPE_DECISION_REQ && len == sizeof(decision_request_frame_t)) {
        const decision_request_frame_t *req = (const decision_request_frame_t *)data;
        if (espnow_crc8(data, sizeof(*req) - 1) == req->crc8) {
            memcpy(&g_decision_req, req, sizeof(g_decision_req));
            g_decision_pending = true;   // 主循环据此切到决策界面
            Serial.printf("Decision req id=%u kind=%u opts=%u\n",
                          req->decision_id, req->kind, req->opt_count);
        }
    }
```

- [ ] **Step 3: 编译**

Run: `python -m platformio run -e 52_codebuddy_ai_box`
Expected: 编译通过。

- [ ] **Step 4: Commit**

```bash
git add examples/52_codebuddy_ai_box/51_mic_wifi.ino
git commit -m "feat(52): espnow_recv_cb 收 0x0A 决策请求"
```

---
## Task 3: 决策界面 (screen 7) — LVGL 创建 + 刷新

**Files:**
- Modify: `examples/52_codebuddy_ai_box/51_mic_wifi.ino`

**Interfaces:**
- Consumes: `g_decision_req`, `g_decision_sel` (Task 2); `xGuiSemaphore`, `switch_screen` (既有)。
- Produces:
  - `static void create_screen_decision();` — 建界面对象。
  - `static void update_screen_decision();` — 按 g_decision_req 填标题/选项/高亮。
  - 全局: `static lv_obj_t *screen_decision = NULL;` + 选项按钮数组 + 确认按钮 + 各自的屏幕坐标 (供触摸命中用)。

- [ ] **Step 1: 全局对象 + 命中区域 (界面对象区, screen_touch_test 附近)**

```c
static lv_obj_t *screen_decision = NULL;
static lv_obj_t *dec_title_label = NULL;
static lv_obj_t *dec_opt_btn[DECISION_MAX_OPTS] = {NULL};
static lv_obj_t *dec_opt_label[DECISION_MAX_OPTS] = {NULL};
static lv_obj_t *dec_confirm_btn = NULL;
// 触摸命中矩形 (屏幕坐标 240x320): 选项纵向排列, 确认键在底部
#define DEC_OPT_X       10
#define DEC_OPT_W       220
#define DEC_OPT_H       40
#define DEC_OPT_Y0      70
#define DEC_OPT_GAP     48
#define DEC_CONFIRM_Y   280
#define DEC_CONFIRM_H   35
```

- [ ] **Step 2: create_screen_decision()**

```c
static void create_screen_decision() {
    screen_decision = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_decision, lv_color_hex(0x101828), 0);

    dec_title_label = lv_label_create(screen_decision);
    lv_label_set_long_mode(dec_title_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(dec_title_label, 220);
    lv_obj_set_style_text_color(dec_title_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(dec_title_label, &lv_font_montserrat_18, 0);
    lv_obj_align(dec_title_label, LV_ALIGN_TOP_MID, 0, 15);
    lv_label_set_text(dec_title_label, "");

    for (int i = 0; i < DECISION_MAX_OPTS; i++) {
        dec_opt_btn[i] = lv_obj_create(screen_decision);
        lv_obj_set_size(dec_opt_btn[i], DEC_OPT_W, DEC_OPT_H);
        lv_obj_set_pos(dec_opt_btn[i], DEC_OPT_X, DEC_OPT_Y0 + i * DEC_OPT_GAP);
        lv_obj_set_style_radius(dec_opt_btn[i], 8, 0);
        lv_obj_set_style_bg_color(dec_opt_btn[i], lv_color_hex(0x2A3441), 0);
        lv_obj_clear_flag(dec_opt_btn[i], LV_OBJ_FLAG_SCROLLABLE);
        dec_opt_label[i] = lv_label_create(dec_opt_btn[i]);
        lv_obj_set_style_text_color(dec_opt_label[i], lv_color_hex(0xE5E7EB), 0);
        lv_obj_center(dec_opt_label[i]);
        lv_label_set_text(dec_opt_label[i], "");
    }

    dec_confirm_btn = lv_obj_create(screen_decision);
    lv_obj_set_size(dec_confirm_btn, DEC_OPT_W, DEC_CONFIRM_H);
    lv_obj_set_pos(dec_confirm_btn, DEC_OPT_X, DEC_CONFIRM_Y);
    lv_obj_set_style_radius(dec_confirm_btn, 8, 0);
    lv_obj_set_style_bg_color(dec_confirm_btn, lv_color_hex(0x2563EB), 0);
    lv_obj_clear_flag(dec_confirm_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *cl = lv_label_create(dec_confirm_btn);
    lv_label_set_text(cl, "confirm");
    lv_obj_set_style_text_color(cl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(cl);
}
```

- [ ] **Step 3: update_screen_decision() — 填数据 + 高亮**

```c
static void update_screen_decision() {
    if (!dec_title_label) return;
    lv_label_set_text(dec_title_label, g_decision_req.title);
    for (int i = 0; i < DECISION_MAX_OPTS; i++) {
        if (!dec_opt_btn[i]) continue;
        if (i < g_decision_req.opt_count) {
            lv_obj_clear_flag(dec_opt_btn[i], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(dec_opt_label[i], g_decision_req.opts[i]);
            uint32_t bg = (i == g_decision_sel) ? 0x2563EB : 0x2A3441;  // 选中变蓝
            lv_obj_set_style_bg_color(dec_opt_btn[i], lv_color_hex(bg), 0);
        } else {
            lv_obj_add_flag(dec_opt_btn[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}
```

- [ ] **Step 4: 在 switch_screen 的 screen 分派里注册 screen 7**

在 switch_screen 内 (参考既有 screen 6 触摸测试的 case/if 分支) 加:
```c
    else if (screen_num == 7) {
        if (!screen_decision) create_screen_decision();
        update_screen_decision();
        lv_scr_load(screen_decision);
    }
```
(具体形态按既有 switch_screen 里的写法对齐——若用 if-else 链就加 else if。)

- [ ] **Step 5: 编译**

Run: `python -m platformio run -e 52_codebuddy_ai_box`
Expected: 编译通过。

- [ ] **Step 6: Commit**

```bash
git add examples/52_codebuddy_ai_box/51_mic_wifi.ino
git commit -m "feat(52): 决策界面 screen 7 (标题+选项+确认)"
```

---

## Task 4: send_decision_reply — 组 0x0B + ESP-NOW 回传

**Files:**
- Modify: `examples/52_codebuddy_ai_box/51_mic_wifi.ino`

**Interfaces:**
- Consumes: `decision_reply_frame_t` (Task 1); `dongle_mac` + `esp_now_send` (既有, 见 send_key_frame 附近)。
- Produces: `static void send_decision_reply(uint16_t id, uint8_t index);`

- [ ] **Step 1: 写 send_decision_reply (放 send_key_frame 附近)**

```c
static void send_decision_reply(uint16_t id, uint8_t index) {
    decision_reply_frame_t rep;
    memset(&rep, 0, sizeof(rep));
    rep.frame_type = FRAME_TYPE_DECISION_REPLY;
    rep.decision_id = id;
    rep.chosen_index = index;
    rep.crc8 = espnow_crc8((uint8_t*)&rep, sizeof(rep) - 1);
    esp_now_send(dongle_mac, (uint8_t*)&rep, sizeof(rep));
    Serial.printf("Decision reply id=%u index=%u sent\n", id, index);
}
```

- [ ] **Step 2: 编译**

Run: `python -m platformio run -e 52_codebuddy_ai_box`
Expected: 编译通过。

- [ ] **Step 3: Commit**

```bash
git add examples/52_codebuddy_ai_box/51_mic_wifi.ino
git commit -m "feat(52): send_decision_reply 组 0x0B 回传"
```

---
## Task 5: 主循环接线 (切界面 + 触摸命中 + 确认 + 超时)

**Files:**
- Modify: `examples/52_codebuddy_ai_box/51_mic_wifi.ino`

**Interfaces:**
- Consumes: `g_decision_pending` / `g_decision_active` / `g_decision_deadline_ms` / `g_decision_sel` / `g_decision_req` (Task 2); `switch_screen` (既有); `update_screen_decision` (Task 3); `send_decision_reply` (Task 4); `touch.scan()` (既有 AtkBoxTouch)。
- Produces: 主循环中的决策处理块。K10 常量: 屏 240x320, 15s 超时。

- [ ] **Step 1: loop() 顶部 — 检测新请求, 切到决策界面**

在 loop() 内、界面轮换逻辑之前加:
```c
    if (g_decision_pending) {
        g_decision_pending = false;
        g_decision_active = true;
        g_decision_sel = -1;
        g_decision_deadline_ms = millis() + 15000;   // 15s 超时
        switch_screen(7);
    }
```

- [ ] **Step 2: loop() 内 — 决策界面激活时的触摸命中 + 超时 (其余交互让路)**

紧接上一步加:
```c
    if (g_decision_active) {
        // 超时: 发 0xFF 回执, 退出决策界面
        if ((int32_t)(millis() - g_decision_deadline_ms) >= 0) {
            send_decision_reply(g_decision_req.decision_id, 0xFF);
            g_decision_active = false;
            switch_screen(SCREEN_ORDER[order_index]);   // 回到轮换界面
        } else {
            uint16_t tx, ty;
            static uint32_t last_touch_ms = 0;
            if (touch.scan(&tx, &ty) && (millis() - last_touch_ms > 250)) {
                last_touch_ms = millis();
                // 命中选项?
                bool hit = false;
                for (int i = 0; i < g_decision_req.opt_count; i++) {
                    int y0 = DEC_OPT_Y0 + i * DEC_OPT_GAP;
                    if (tx >= DEC_OPT_X && tx <= DEC_OPT_X + DEC_OPT_W &&
                        ty >= y0 && ty <= y0 + DEC_OPT_H) {
                        g_decision_sel = i;
                        hit = true;
                        if (xGuiSemaphore && xSemaphoreTake(xGuiSemaphore, portMAX_DELAY) == pdTRUE) {
                            update_screen_decision();
                            xSemaphoreGive(xGuiSemaphore);
                        }
                        break;
                    }
                }
                // 命中确认键且已选?
                if (!hit && g_decision_sel >= 0 &&
                    tx >= DEC_OPT_X && tx <= DEC_OPT_X + DEC_OPT_W &&
                    ty >= DEC_CONFIRM_Y && ty <= DEC_CONFIRM_Y + DEC_CONFIRM_H) {
                    send_decision_reply(g_decision_req.decision_id, (uint8_t)g_decision_sel);
                    g_decision_active = false;
                    switch_screen(SCREEN_ORDER[order_index]);
                }
            }
        }
        return;   // 决策界面激活时, 跳过本次 loop 的常规按键/轮换逻辑 (但 lv_task_handler 仍需跑)
    }
```

- [ ] **Step 3: 确认 lv_task_handler 不被 return 跳过**

检查 loop() 结构: 若 `lv_task_handler()` 在上面 `return` 之后, 需把决策块的 `return` 改为跳过"按键处理/界面轮换"但**保留** `lv_task_handler()` 调用。做法: 用一个 `bool skip_normal = g_decision_active;` 包住常规交互, 而非直接 return。按实际 loop() 结构调整——目标: LVGL 刷新照常, 只是不响应 A/B 键与轮换。

```c
    // 示例结构 (按实际 loop 调整):
    //   if (!skip_normal) { ...按键处理/轮换... }
    //   lv_task_handler();  // 始终执行
```

- [ ] **Step 4: 编译 + 烧录**

Run: `python -m platformio run -t upload -e 52_codebuddy_ai_box --upload-port <K10_COM>`
Expected: 编译烧录成功。

- [ ] **Step 5: 单元验证 (临时造请求)**

在 setup() 末尾临时加一段, 上电 5s 后本地伪造一个决策请求 (验证界面+触摸+回执, 无需 Dongle):
```c
    // 临时: 伪造决策请求
    strncpy(g_decision_req.title, "Allow Write test.txt?", DECISION_TITLE_LEN-1);
    g_decision_req.decision_id = 999; g_decision_req.kind = 0; g_decision_req.opt_count = 3;
    strncpy(g_decision_req.opts[0], "Yes", DECISION_OPT_LEN-1);
    strncpy(g_decision_req.opts[1], "No", DECISION_OPT_LEN-1);
    strncpy(g_decision_req.opts[2], "Always", DECISION_OPT_LEN-1);
    g_decision_pending = true;
```
Expected: 上电即弹决策界面, 触摸选项高亮, 点确认后串口打印 `Decision reply id=999 index=N sent`, 界面退回轮换。验证后删除这段临时代码, 重新烧录。

- [ ] **Step 6: Commit**

```bash
git add examples/52_codebuddy_ai_box/51_mic_wifi.ino
git commit -m "feat(52): 主循环决策界面接线 (触摸选+确认+超时)"
```

---

## Task 6: 与接收端会师联调 (跨计划)

**Files:** 无, 纯集成验证。

- [ ] **Step 1: 两端都烧录最新固件, K10 与 Dongle 完成配对**

Expected: Dongle 日志出现 "Paired with device"; K10 能正常收 0x07/0x08/0x09 (回归)。

- [ ] **Step 2: 从 PC 经 cc_bridge 发真实决策**

Run (接收端计划环境): `python bridge.py <UART0_COM> "在当前目录创建 test.txt"`
Expected: K10 弹"运行/Write: test.txt?" → 触摸"允许" → cc_bridge 收到 index=0 → Claude 真的创建文件。

- [ ] **Step 3: 测多选题 (AskUserQuestion)**

给一个有多方案的任务, 触发 Claude 调 AskUserQuestion → K10 显示问题+选项 → 触摸选 → Claude 采纳。

- [ ] **Step 4: 回归**

确认音频✅键盘✅仍正常 (按 A 录音、按键 HID)。

---

## Self-Review (发射端)

- **spec 覆盖**: 协议(T1/spec4, 与Dongle一致)✓ 收0x0A(T2/spec6.3)✓ 决策界面LVGL(T3/spec6.3)✓ 0x0B回传(T4/spec6.3)✓ 触摸选+确认+暂停轮换+15s超时(T5/spec6.3)✓ 端到端(T6/spec8阶段2-3)✓ 纯触摸(T3/T5, 无物理键依赖/spec交互)✓
- **占位符**: 均为可执行代码。T3 Step4 / T5 Step3 标注"按既有 switch_screen / loop 实际结构对齐"——这是因 .ino 既有结构需现场读取, 已给出明确目标与示例, 非空泛占位。
- **类型一致**: `g_decision_req`/`g_decision_sel`/`g_decision_active` 等 T2 定义、T3/T5 使用一致; `create_screen_decision`/`update_screen_decision` T3 定义、T3Step4/T5 使用一致; `send_decision_reply(uint16_t,uint8_t)` T4 定义、T5 调用签名一致; `DEC_OPT_*` 宏 T3 定义、T5 命中检测使用一致; frame_type 常量与 T1 一致。
- **xGuiSemaphore**: T5 触摸更新界面时持锁 (spec 5 约束); switch_screen 既有实现自带持锁。
- **跨计划一致性**: T1 结构体与接收端计划 Task 1 逐字节相同, 以 spec 第4节为唯一真源; 联调 T6 用 sizeof 日志 + 实际收发兜底验证。

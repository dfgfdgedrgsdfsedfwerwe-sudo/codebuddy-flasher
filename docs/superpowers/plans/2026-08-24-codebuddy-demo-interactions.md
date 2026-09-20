# CodeBuddy 演示模式交互 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 CodeBuddy K10 的界面 1（Token Usage）、界面 3（Product Inspo）、界面 4（User Profile）增加演示模式交互，统一采用「短按 A 循环/长按 A 触发」模式。

**Architecture:** 在单文件 Arduino sketch `51_mic_wifi.ino` 中，为每个界面新增状态变量、创建时初始化交互控件、在 `update_screen_X()` 中反映状态、在 `loop()` 的按键 A 处理块中根据 `current_screen` 分派交互逻辑。打字动画在 `loop()` 中独立轮询。所有交互仅 `demo_mode_active == true` 时生效。

**Tech Stack:** Arduino (PlatformIO / ESP32-S3), LVGL 8.x, Arduino SD 库, FreeRTOS 信号量。

## Global Constraints

- 目标文件仅一个：`examples/51_mic_wifi/51_mic_wifi.ino`
- 编译命令：`python -m platformio run -e 51_mic_wifi`（`pio`/`platformio` 不在 PATH）
- 烧录命令：`python -m platformio run -e 51_mic_wifi -t upload --upload-port COM4`
- 所有 LVGL 对象操作必须持 `xGuiSemaphore` 信号量（`xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)` … `xSemaphoreGive(xGuiSemaphore)`）
- 长按阈值复用现有常量 `LONG_PRESS_MS`（600ms）
- 按键 B 逻辑不得修改（始终用于界面导航）
- 所有交互逻辑用 `if (demo_mode_active && current_screen == N)` 包裹，非演示模式行为不变
- 高亮色 `0xFBBF24`（亮黄）、绿色 `0x10B981`、白色 `0xFFFFFF`、灰色 `0x888888`（与现有代码一致）
- 界面 ID：0=Status, 1=Token, 2=Project, 3=Inspo, 4=Profile, 5=AI
- 无单元测试框架：每个任务的验证 = 编译通过 + 烧录 + 串口日志确认 + 屏幕视觉确认

---

## File Structure

**唯一修改文件**：`examples/51_mic_wifi/51_mic_wifi.ino`

改动分布：
1. **全局变量区**（约 line 519-528 附近，界面 2 变量之后）— 新增界面 1/3/4 的状态变量
2. **`create_screen_token()`**（约 line 400+）— 无需改（高亮通过 update 实现）
3. **`update_screen_token()`**（约 line 457-509）— 增加高亮渲染
4. **`create_screen_inspo()`**（约 line 689-716）— 增加光标标签
5. **`update_screen_inspo()`**（约 line 718-720）— 无需大改
6. **`create_screen_profile()`**（约 line 727-757）— 保存头像对象引用供切换
7. **`inject_demo_data()`**（约 line 1030-1120）— 保存灵感全文指针、触发 SD 扫描
8. **`loop()` 按键 A 处理块**（约 line 1243-1295）— 新增界面 1/3/4 分派分支
9. **`loop()` 打字动画轮询**（loop 末尾，界面刷新附近）— 新增打字机推进逻辑
10. **新增辅助函数**：`update_token_highlight()`、`reset_token_item()`、`scan_profile_images()`、`start_inspo_typing()`、`update_inspo_typing()`

---

## Task 1: Token Usage 高亮切换 + 重置

**Files:**
- Modify: `examples/51_mic_wifi/51_mic_wifi.ino`（全局变量区、`update_screen_token()`、`loop()` 按键 A 块）

**Interfaces:**
- Consumes: 现有 `token_data`（`token_status_frame_t`，字段 `.count`、`.items[i].name`、`.items[i].used`、`.items[i].total`、`.items[i].percent_x10`）、`token_rows[i]`（结构含 `.name_label`、`.pct_label`、`.bar`、`.amount_label`）、`demo_mode_active`、`current_screen`、`LONG_PRESS_MS`、`xGuiSemaphore`、`format_number()`、`espnow_crc8()`
- Produces: `token_selected`（uint8_t 全局）、`update_token_highlight()`、`reset_token_item(uint8_t idx)`

- [ ] **Step 1: 新增状态变量**

在界面 2 变量块之后（`choice_confirmed` 声明行的下方）添加：

```cpp
// 界面 1 Token Usage 交互: 高亮选择 + 重置
static uint8_t token_selected = 0;   // 当前高亮的模型行索引
```

- [ ] **Step 2: 新增高亮渲染函数**

在 `update_screen_token()` 函数定义之前添加：

```cpp
// 刷新 Token 界面的高亮 (仅演示模式)
static void update_token_highlight() {
    if (!demo_mode_active) return;
    uint8_t count = token_data_valid ? token_data.count : 0;
    if (count > TOKEN_MAX_ITEMS) count = TOKEN_MAX_ITEMS;
    for (uint8_t i = 0; i < count; i++) {
        uint32_t color = (i == token_selected) ? 0xFBBF24 : 0xFFFFFF;
        lv_obj_set_style_text_color(token_rows[i].name_label, lv_color_hex(color), 0);
    }
}
```

- [ ] **Step 3: 在 update_screen_token 末尾调用高亮**

找到 `update_screen_token()` 函数结尾的 `}`（约 line 509，`count == 0` 提示块之后），在其之前插入：

```cpp
    // 演示模式下应用高亮
    update_token_highlight();
```

- [ ] **Step 4: 新增重置函数**

在 `update_token_highlight()` 之后添加：

```cpp
// 重置指定 Token 模型的用量 (演示: 模拟清空配额)
static void reset_token_item(uint8_t idx) {
    if (!token_data_valid || idx >= token_data.count) return;
    token_data.items[idx].used = 0;
    token_data.items[idx].percent_x10 = 0;
    Serial.printf("Token: reset %s\n", token_data.items[idx].name);
    update_screen_token();  // 重绘进度条/百分比/数量
}
```

- [ ] **Step 5: 在按键 A 处理块新增界面 1 分支**

在 `loop()` 的按键 A 释放处理中，找到 `if (demo_mode_active && current_screen == 2 && !choice_confirmed) {` 这个分支。在它的**前面**插入界面 1 的分支（保持 else-if 链）：

```cpp
            // 界面 1 Token Usage: 短按 A 高亮下一个, 长按 A 重置当前
            if (demo_mode_active && current_screen == 1) {
                uint8_t count = token_data_valid ? token_data.count : 0;
                if (count == 0) { prev_a = a; continue; }  // 无数据不响应
                if (duration < LONG_PRESS_MS) {
                    token_selected = (token_selected + 1) % count;
                    Serial.printf("Token: highlight %d %s\n", token_selected,
                                  token_data.items[token_selected].name);
                    if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) {
                        update_token_highlight();
                        xSemaphoreGive(xGuiSemaphore);
                    }
                } else {
                    if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) {
                        reset_token_item(token_selected);
                        xSemaphoreGive(xGuiSemaphore);
                    }
                }
            } else if (demo_mode_active && current_screen == 2 && !choice_confirmed) {
```

注意：原来的 `if (demo_mode_active && current_screen == 2 ...)` 改为 `} else if (...)`，即把界面 1 的 `if` 接在前面。若使用 `continue;` 需确认按键块处于 `if (now - lastBtn >= 10)` 内且没有后续必须执行的代码——当前结构中 `continue` 会跳到 `loop()` 顶部，安全。

- [ ] **Step 6: 编译**

Run: `python -m platformio run -e 51_mic_wifi`
Expected: `SUCCESS`，无编译错误

- [ ] **Step 7: 烧录并验证**

Run: `python -m platformio run -e 51_mic_wifi -t upload --upload-port COM4`
然后 `python -m platformio device monitor -p COM4 -b 115200`

验证步骤：
1. A+B 长按 2 秒进入演示模式
2. 按 B 切到界面 1（Token Usage）
3. 短按 A：串口打印 `Token: highlight N <名称>`，屏幕对应行名称变黄
4. 连续短按 A：高亮循环回到第一行
5. 长按 A：串口打印 `Token: reset <名称>`，该行进度条清空、百分比归零、颜色变绿

- [ ] **Step 8: 提交（若 git 仓库已初始化）**

```bash
git add examples/51_mic_wifi/51_mic_wifi.ino
git commit -m "feat: Token Usage 界面高亮切换+重置交互"
```
若非 git 仓库则跳过。

---

## Task 2: Product Inspo 打字机动画 + 闪烁光标

**Files:**
- Modify: `examples/51_mic_wifi/51_mic_wifi.ino`（全局变量区、`create_screen_inspo()`、`inject_demo_data()`、`loop()` 按键 A 块、`loop()` 动画轮询）

**Interfaces:**
- Consumes: 现有 `label_inspo_content`、`label_inspo_date`（LVGL label 对象）、`demo_mode_active`、`current_screen`、`LONG_PRESS_MS`、`xGuiSemaphore`、`millis()`
- Produces: `inspo_full_text`（const char*）、`inspo_char_count`（uint16_t）、`inspo_typing`（bool）、`start_inspo_typing()`、`update_inspo_typing()`

- [ ] **Step 1: 新增状态变量**

在 Task 1 的 `token_selected` 声明之后添加：

```cpp
// 界面 3 Product Inspo 打字动画
static const char *inspo_full_text = nullptr;   // 完整文本 (指向 inject_demo_data 里的字面量)
static uint16_t inspo_char_count = 0;            // 已显示字符数
static bool     inspo_typing = false;            // 是否正在打字
static uint32_t inspo_last_char = 0;             // 上次显示字符时间戳
static bool     inspo_cursor_on = false;         // 光标当前显示状态
static uint32_t inspo_cursor_time = 0;           // 光标闪烁计时
static const uint32_t INSPO_CHAR_MS = 40;        // 每字符间隔
static const uint32_t INSPO_CURSOR_MS = 500;     // 光标闪烁间隔
```

- [ ] **Step 2: 在 inject_demo_data 保存全文指针**

在 `inject_demo_data()` 中找到设置灵感内容的位置（`lv_label_set_text(label_inspo_content, "Lenovo CodeBuddy is more than...")`）。将长字符串提取为 static 常量并保存指针。在该 `lv_label_set_text` 调用**之前**添加：

```cpp
    static const char *INSPO_TEXT =
        "Lenovo CodeBuddy is more than a tool for coding - it is a new "
        "creative interface for the AI era. Designed to turn ideas into "
        "action, CodeBuddy enables users to vibe code through natural "
        "voice interaction, capture inspiration instantly, and stay "
        "continuously connected with AI assistants throughout the day.";
    inspo_full_text = INSPO_TEXT;
```

然后把原来的 `lv_label_set_text(label_inspo_content, "...")` 改为显示全文（初始态显示完整文本，等按 A 才重新打字）：

```cpp
    lv_label_set_text(label_inspo_content, INSPO_TEXT);
```

（若原代码已有等价长字符串，直接复用其内容赋给 `INSPO_TEXT`，删除重复字面量。）

- [ ] **Step 3: 新增打字启动函数**

在 `update_screen_inspo()` 之前添加：

```cpp
// 开始灵感打字动画
static void start_inspo_typing() {
    if (inspo_full_text == nullptr) return;
    inspo_char_count = 0;
    inspo_typing = true;
    inspo_last_char = millis();
    inspo_cursor_on = true;
    inspo_cursor_time = millis();
    lv_label_set_text(label_inspo_content, "_");
    lv_label_set_text(label_inspo_date, "Just now");
    Serial.println("Inspo: start typing");
}
```

- [ ] **Step 4: 新增打字推进函数**

在 `start_inspo_typing()` 之后添加。缓冲区 512 字节，容纳全文 + 光标 + 结束符：

```cpp
// 打字动画每帧推进 (在 loop 中调用, 已持有信号量)
static void update_inspo_typing() {
    if (inspo_full_text == nullptr) return;
    uint32_t now = millis();
    uint16_t full_len = strlen(inspo_full_text);
    static char buf[512];

    // 推进字符
    if (inspo_typing && (now - inspo_last_char >= INSPO_CHAR_MS)) {
        inspo_last_char = now;
        if (inspo_char_count < full_len) {
            inspo_char_count++;
        } else {
            inspo_typing = false;  // 打完
        }
    }

    // 光标闪烁
    if (now - inspo_cursor_time >= INSPO_CURSOR_MS) {
        inspo_cursor_time = now;
        inspo_cursor_on = !inspo_cursor_on;
    }

    // 组装显示文本 = 已打字符 + 光标
    uint16_t n = inspo_char_count;
    if (n > full_len) n = full_len;
    if (n > sizeof(buf) - 2) n = sizeof(buf) - 2;
    memcpy(buf, inspo_full_text, n);
    buf[n] = inspo_cursor_on ? '_' : ' ';
    buf[n + 1] = '\0';
    lv_label_set_text(label_inspo_content, buf);
}
```

- [ ] **Step 5: 在按键 A 处理块新增界面 3 分支**

在 Task 1 新增的界面 1 分支之后（else-if 链中），加入界面 3 分支：

```cpp
            } else if (demo_mode_active && current_screen == 3) {
                if (duration < LONG_PRESS_MS) {
                    // 短按: 开始打字
                    if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) {
                        start_inspo_typing();
                        xSemaphoreGive(xGuiSemaphore);
                    }
                } else {
                    // 长按: 跳过, 显示全文
                    inspo_typing = false;
                    inspo_char_count = strlen(inspo_full_text);
                    inspo_cursor_on = false;
                    if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) {
                        lv_label_set_text(label_inspo_content, inspo_full_text);
                        xSemaphoreGive(xGuiSemaphore);
                    }
                    Serial.println("Inspo: skip animation");
                }
```

- [ ] **Step 6: 在 loop 动画轮询区新增打字推进**

在 `loop()` 末尾找到界面刷新/动画区域（Task 参考现有 AI 界面动画轮询，形如 `if (current_screen == 5) { ... }`）。在其附近添加界面 3 的轮询：

```cpp
    // 界面 3 打字动画推进
    if (demo_mode_active && current_screen == 3 &&
        (inspo_typing || inspo_cursor_on || inspo_char_count > 0)) {
        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) {
            update_inspo_typing();
            xSemaphoreGive(xGuiSemaphore);
        }
    }
```

- [ ] **Step 7: 编译**

Run: `python -m platformio run -e 51_mic_wifi`
Expected: `SUCCESS`

- [ ] **Step 8: 烧录并验证**

Run: `python -m platformio run -e 51_mic_wifi -t upload --upload-port COM4`

验证步骤：
1. A+B 进演示模式，按 B 切到界面 3（Product Inspo）
2. 短按 A：串口 `Inspo: start typing`，日期变 "Just now"，文本从头逐字出现，末尾有闪烁 `_`
3. 打字过程中末尾光标每 0.5 秒闪一次
4. 打完后光标继续闪烁
5. 再次短按 A 重新打字；打字中长按 A：串口 `Inspo: skip animation`，立即显示全文，光标消失

- [ ] **Step 9: 提交（若 git 仓库已初始化）**

```bash
git add examples/51_mic_wifi/51_mic_wifi.ino
git commit -m "feat: Product Inspo 打字机动画+闪烁光标"
```

---

## Task 3: User Profile SD 卡扫描 + 切换用户

**Files:**
- Modify: `examples/51_mic_wifi/51_mic_wifi.ino`（全局变量区、`create_screen_profile()`、`inject_demo_data()`、`loop()` 按键 A 块）

**Interfaces:**
- Consumes: 现有 `sd_card_ready`（bool）、`label_profile_name`（LVGL label）、`profile_image`（LVGL img 对象）、`SD`（Arduino SD 库全局）、`demo_mode_active`、`current_screen`、`LONG_PRESS_MS`、`xGuiSemaphore`
- Produces: `profile_paths[][16]`、`profile_count`（uint8_t）、`profile_index`（uint8_t）、`scan_profile_images()`

- [ ] **Step 1: 确认头像对象引用可用**

检查 `create_screen_profile()`：确认 SD 就绪分支创建的图片对象保存在文件级变量 `profile_image`（现有代码约 line 723 `static lv_obj_t *profile_image;`）。若该变量是函数内局部，改为文件级 static。切换用户需要通过它调用 `lv_img_set_src()`。

- [ ] **Step 2: 新增状态变量**

在 Task 2 的 inspo 变量之后添加：

```cpp
// 界面 4 User Profile: SD 卡多头像切换
#define MAX_PROFILE_IMAGES 10
static char    profile_paths[MAX_PROFILE_IMAGES][16];  // LVGL 路径 "D:/userN.png"
static uint8_t profile_count = 0;                      // 扫描到的图片数
static uint8_t profile_index = 0;                      // 当前显示索引
```

- [ ] **Step 3: 新增 SD 扫描函数**

在 `create_screen_profile()` 之前添加：

```cpp
// 扫描 SD 卡根目录 /user1.png ~ /user10.png
static void scan_profile_images() {
    profile_count = 0;
    if (!sd_card_ready) {
        Serial.println("Profile: SD not ready, no scan");
        return;
    }
    for (int i = 1; i <= 10 && profile_count < MAX_PROFILE_IMAGES; i++) {
        char sd_path[16];
        snprintf(sd_path, sizeof(sd_path), "/user%d.png", i);
        if (SD.exists(sd_path)) {
            snprintf(profile_paths[profile_count], 16, "D:/user%d.png", i);
            profile_count++;
        }
    }
    Serial.printf("Profile: found %d images\n", profile_count);
}
```

- [ ] **Step 4: 在 inject_demo_data 触发扫描并显示首张**

在 `inject_demo_data()` 中找到设置 profile 名字的位置（`lv_label_set_text(label_profile_name, ...)`）。替换为：

```cpp
    scan_profile_images();
    profile_index = 0;
    if (profile_count > 0) {
        lv_img_set_src(profile_image, profile_paths[0]);
        lv_label_set_text_fmt(label_profile_name, "User %d", 1);
    }
```

（若 `profile_image` 在 SD 未就绪时为 NULL，`scan_profile_images()` 会返回 count=0，此块不执行，安全。）

- [ ] **Step 5: 在按键 A 处理块新增界面 4 分支**

在 Task 2 的界面 3 分支之后（else-if 链），加入界面 4 分支：

```cpp
            } else if (demo_mode_active && current_screen == 4) {
                if (profile_count == 0) { prev_a = a; continue; }  // 无图不响应
                if (duration < LONG_PRESS_MS) {
                    // 短按: 切换下一个用户
                    profile_index = (profile_index + 1) % profile_count;
                } else {
                    // 长按: 返回第一个用户
                    profile_index = 0;
                    Serial.println("Profile: reset to User 1");
                }
                if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) {
                    lv_img_set_src(profile_image, profile_paths[profile_index]);
                    lv_label_set_text_fmt(label_profile_name, "User %d", profile_index + 1);
                    xSemaphoreGive(xGuiSemaphore);
                }
                Serial.printf("Profile: switch to User %d\n", profile_index + 1);
```

- [ ] **Step 6: 编译**

Run: `python -m platformio run -e 51_mic_wifi`
Expected: `SUCCESS`

- [ ] **Step 7: 烧录并验证**

Run: `python -m platformio run -e 51_mic_wifi -t upload --upload-port COM4`

验证步骤：
1. 确认 SD 卡根目录有 user1.png ~ user4.png
2. A+B 进演示模式，串口应打印 `Profile: found 4 images`
3. 按 B 切到界面 4（User Profile），显示 user1.png + "User 1"
4. 短按 A：头像切到 user2.png，名字变 "User 2"，串口 `Profile: switch to User 2`
5. 连续短按 A 循环到 User 4 后回到 User 1
6. 长按 A：回到 User 1，串口 `Profile: reset to User 1`

- [ ] **Step 8: 提交（若 git 仓库已初始化）**

```bash
git add examples/51_mic_wifi/51_mic_wifi.ino
git commit -m "feat: User Profile SD卡扫描+多用户切换"
```

---

## Task 4: 集成回归测试

**Files:**
- 无代码改动（纯验证）

**Interfaces:**
- Consumes: Task 1/2/3 的全部产物

- [ ] **Step 1: 全界面循环验证**

烧录最新固件后，A+B 进演示模式，按 B 依次循环所有界面，确认：
- 界面 0 (Status)：正常显示，按 A 仍触发 F2+音频流（非演示交互）→ 实际演示模式下界面 0 行为按现有逻辑
- 界面 1 (Token)：短按高亮、长按重置正常
- 界面 2 (Project)：选择题短按切换、长按确认正常（回归，未被破坏）
- 界面 3 (Inspo)：短按打字、长按跳过正常
- 界面 4 (Profile)：短按切换、长按复位正常
- 界面 5 (AI)：表情动画正常（回归，未被破坏）

- [ ] **Step 2: 按键 B 导航回归**

在每个界面短按 B，确认都能正常切到下一个界面（按键 B 逻辑未受影响）。

- [ ] **Step 3: 非演示模式回归**

重启设备（不进演示模式），确认：
- 按 A 短按触发 F2 + 音频流开关（串口 `Streaming STARTED/STOPPED`）
- 按 A 长按触发 Enter（串口 `Key sent: Enter`）
- 按 B 切换界面
- 三个新交互在非演示模式下**不生效**

- [ ] **Step 4: SD 卡异常回归**

拔掉 SD 卡重启，进演示模式，切到界面 4，确认：
- 串口 `Profile: SD not ready, no scan` 或 `found 0 images`
- 按 A 无反应，不崩溃
- 显示原有 fallback 渐变头像

- [ ] **Step 5: 更新项目文档**

在 `examples/51_mic_wifi/状态显示功能.md` 中追加三个新界面的交互说明（短按/长按行为表格）。

- [ ] **Step 6: 提交（若 git 仓库已初始化）**

```bash
git add examples/51_mic_wifi/状态显示功能.md
git commit -m "docs: 补充界面1/3/4演示交互说明"
```

---

## Self-Review

**1. Spec coverage（对照设计文档逐条核对）:**
- ✅ 界面 1 短按高亮 → Task 1 Step 5
- ✅ 界面 1 长按重置 → Task 1 Step 4-5
- ✅ 界面 3 短按打字 → Task 2 Step 3,5
- ✅ 界面 3 长按跳过 → Task 2 Step 5
- ✅ 界面 3 闪烁光标 → Task 2 Step 4
- ✅ 界面 4 SD 扫描 → Task 3 Step 3
- ✅ 界面 4 短按切换 → Task 3 Step 5
- ✅ 界面 4 长按复位 → Task 3 Step 5
- ✅ 自动用户名 "User N" → Task 3 Step 4,5
- ✅ Fallback 处理 → Task 3 Step 3,5 + Task 4 Step 4
- ✅ 按键 B 不受影响 → Global Constraints + Task 4 Step 2
- ✅ 非演示模式不受影响 → Global Constraints + Task 4 Step 3

**2. Placeholder scan:** 无 TODO/TBD，所有代码步骤含完整代码块。

**3. Type consistency:**
- `token_selected` (uint8_t) — Task 1 定义，Task 1 使用 ✅
- `update_token_highlight()` — Task 1 Step 2 定义，Step 3/5 调用 ✅
- `reset_token_item(uint8_t)` — Task 1 Step 4 定义，Step 5 调用 ✅
- `inspo_full_text` (const char*) — Task 2 Step 1 定义，Step 2/3/4/5 使用 ✅
- `start_inspo_typing()` / `update_inspo_typing()` — Task 2 Step 3/4 定义，Step 5/6 调用 ✅
- `scan_profile_images()` — Task 3 Step 3 定义，Step 4 调用 ✅
- `profile_paths`/`profile_count`/`profile_index` — Task 3 Step 2 定义，Step 4/5 使用 ✅
- `profile_image` — Task 3 Step 1 确认为文件级变量后 Step 4/5 使用 ✅

**潜在风险点**：`continue;` 在按键块中的行为——已在 Task 1 Step 5 注明需确认 `continue` 跳转到 loop 顶部是安全的（当前 loop 结构中按键处理后无必须执行的收尾逻辑，但打字动画轮询在 loop 末尾，`continue` 会跳过它。**修正**：界面 1/4 无数据时用 `continue` 会跳过界面 3 打字轮询，但那时 `current_screen != 3`，轮询本就不执行，安全）。

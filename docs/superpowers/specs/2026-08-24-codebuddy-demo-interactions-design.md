# CodeBuddy K10 演示模式交互设计

**日期**: 2026-08-24  
**状态**: 已批准  
**作者**: Claude Code

## 背景

CodeBuddy K10 设备有 6 个演示界面，目前界面 0/2/5 已有交互效果，需要为界面 1（Token Usage）、界面 3（Product Inspo）、界面 4（User Profile）增加演示交互，展示设备的动态能力和使用场景。

## 设计目标

1. **统一交互语言**：采用「短按 A 循环/选择 + 长按 A 触发/重置」模式，与界面 2 的选择题交互保持一致
2. **符合内容特点**：每个界面的交互要贴合其展示目的（监控/记录/身份）
3. **仅演示模式生效**：所有新增交互仅在 `demo_mode == true` 时启用，不影响实际工作模式
4. **简洁实现**：不破坏现有布局，最小化状态变量和代码复杂度

## 通用规则

- **按键 B**：始终保持界面切换导航功能（0→1→2→3→4→5→0）
- **按键 A**：在演示模式下根据 `current_screen` 有不同行为
- **长按检测**：持续 `LONG_PRESS_MS`（600ms）触发长按事件（复用现有常量，与界面 2 保持一致）
- **串口日志**：每个交互动作输出调试信息，格式 `[界面名]: [动作] [参数]`

## 界面 1: Token Usage

### 功能描述
展示多个 AI 模型的 Token 消耗监控，用户可高亮不同模型查看详情，并模拟重置用量。

### 交互设计

**短按 A - 高亮切换**
- 当前高亮索引 +1，循环到末尾后归零（0 → 1 → 2 → 3 → 0）
- 视觉效果：高亮行的服务名变为亮黄色 `0xFBBF24`，其他行保持白色 `0xFFFFFF`
- 串口输出：`Token: highlight [索引] <服务名>`

**长按 A - 重置当前模型**
- 将当前高亮模型的用量归零：`used = 0`、`percent_x10 = 0`
- 进度条清空，颜色变为绿色（<80% 状态）
- 串口输出：`Token: reset <服务名>`

### 状态变量
```c
static uint8_t token_selected = 0;  // 当前高亮索引 (0 ~ count-1)
```

### 实现要点
- 高亮效果通过修改 `lv_label_set_style_text_color()` 实现
- 重置后需重绘进度条和百分比文本
- 若当前无模型数据（`token_count == 0`），按键 A 无反应

---

## 界面 3: Product Inspo

### 功能描述
展示灵感笔记的实时记录过程，模拟用户用 CodeBuddy 快速捕捉想法的场景。

### 交互设计

**短按 A - 开始打字动画**
- 重置已显示字符数为 0，开启打字状态
- 更新卡片日期为当前时间
- 文本内容逐字显示，末尾带闪烁光标 `_`
- 串口输出：`Inspo: start typing`

**长按 A - 跳过动画**
- 立即显示完整文本，停止打字状态
- 隐藏光标
- 串口输出：`Inspo: skip animation`

### 动画参数
- **打字速度**：每 40ms 显示一个字符（约 25 字符/秒）
- **光标闪烁**：每 500ms 切换显示/隐藏状态
- **完整文本**：使用演示模式注入的 `inspo_content` 字符串（约 280 字符，打完约 11 秒）

### 状态变量
```c
static const char *inspo_full_text = nullptr;  // 指向完整文本
static uint16_t inspo_char_count = 0;          // 已显示字符数
static bool     inspo_typing = false;          // 是否正在打字
static uint32_t inspo_last_char = 0;           // 上次显示字符的时间戳
static bool     inspo_cursor_blink = false;    // 光标闪烁状态
static uint32_t inspo_cursor_time = 0;         // 光标闪烁计时
```

### 实现要点
- 在 `loop()` 中轮询：若 `inspo_typing == true` 且时间间隔足够，则 `char_count++`
- 使用 `lv_label_set_text()` 显示子串：`strncpy(buf, inspo_full_text, inspo_char_count)`
- 光标实现：在文本末尾拼接 `_` 字符，根据 `cursor_blink` 状态切换显示
- 打完后 `typing = false`，光标继续闪烁表示"等待输入"

---

## 界面 4: User Profile

### 功能描述
展示 CodeBuddy 的"电子吧唧"功能——用户可切换不同身份头像和名字，体现个性化身份卡特性。

### 交互设计

**短按 A - 切换用户**
- 用户索引 +1，循环到末尾后归零
- 同时更新头像图片和用户名文字
- 串口输出：`Profile: switch to User X`

**长按 A - 返回第一个用户**
- 索引重置为 0，显示第一个用户
- 串口输出：`Profile: reset to User 1`

### 资源扫描
**启动时自动扫描 SD 卡**（演示模式激活时触发）：
```c
void scan_profile_images() {
  profile_count = 0;
  for (int i = 1; i <= 10 && profile_count < MAX_PROFILE_IMAGES; i++) {
    char path[16];
    sprintf(path, "/user%d.png", i);
    if (SD.exists(path)) {
      sprintf(profile_paths[profile_count], "D:/user%d.png", i);  // LVGL 格式
      profile_count++;
    }
  }
  Serial.printf("Profile: found %d images\n", profile_count);
}
```

- 扫描路径：`/user1.png` ~ `/user10.png`（SD 库格式）
- 存储路径：`D:/user1.png` ~ `D:/user10.png`（LVGL 格式）
- 用户名：自动生成 "User 1"、"User 2"...（索引 +1）

### 状态变量
```c
#define MAX_PROFILE_IMAGES 10
static char    profile_paths[MAX_PROFILE_IMAGES][16];  // LVGL 路径数组
static uint8_t profile_count = 0;                      // 找到的图片数量
static uint8_t profile_index = 0;                      // 当前显示索引
```

### 实现要点
- 头像更新：`lv_img_set_src(ui_profile_avatar, profile_paths[profile_index])`
- 用户名更新：`lv_label_set_text_fmt(ui_profile_name_label, "User %d", profile_index + 1)`
- Fallback 处理：若 SD 未就绪或 `profile_count == 0`，按键 A 无反应，显示原有渐变头像
- 路径格式转换：SD 库用 `/userX.png`，LVGL 用 `D:/userX.png`

---

## 实现路线图

### 阶段 1: 基础交互（优先）
1. **Token Usage**: 短按高亮切换 + 长按重置
2. **Product Inspo**: 短按开始打字 + 基础逐字显示
3. **User Profile**: SD 卡扫描 + 短按切换

### 阶段 2: 视觉优化
1. **Product Inspo**: 闪烁光标实现
2. **Token Usage**: 高亮行淡入淡出动画（可选）
3. **User Profile**: 切换时头像缩放动画（可选）

### 阶段 3: 测试验证
1. 按键响应速度测试（短按/长按/连按）
2. SD 卡边界测试（无卡/空卡/user1~user10 全部存在）
3. 长时间打字动画内存测试

---

## 技术约束

1. **SD 卡依赖**：User Profile 功能依赖 SD 卡正常挂载，需在启动时检测并提供 fallback
2. **LVGL 刷新**：所有动画更新需在 `lv_task_handler()` 前完成，避免撕裂
3. **内存限制**：打字动画的临时缓冲区最大 512 字节（适配当前最长灵感文本）
4. **串口日志**：仅在演示模式下输出，避免干扰正常工作模式的调试信息

---

## 成功标准

1. **功能完整**：三个界面的短按/长按交互全部按设计工作
2. **视觉流畅**：打字动画无卡顿，光标闪烁节奏自然
3. **资源鲁棒**：SD 卡异常时不崩溃，有清晰的 fallback 提示
4. **交互一致**：与界面 2 的长按检测时间一致，用户学习成本低

---

## 后续扩展（可选）

1. **Token Usage**: 增加"模拟消耗增长"动画（长按时进度条缓慢上涨）
2. **Product Inspo**: 支持多条灵感笔记切换（短按循环不同笔记）
3. **User Profile**: 支持 SD 卡根目录的 `usernames.txt` 文件自定义用户名
4. **统一配置**: 将所有演示数据（Token 列表、灵感文本、用户名）提取到单独的 `demo_config.h` 文件

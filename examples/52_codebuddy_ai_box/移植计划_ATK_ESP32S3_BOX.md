# CodeBuddy Wireless 移植计划：K10 → 正点原子 ESP32-S3 BOX (DNESP32S3B)

**文档版本**: v1.0
**编写日期**: 2026-08-27
**源工程**: `examples/52_code_buddy_AI BOX/51_mic_wifi.ino`（K10 / Arduino-PlatformIO）
**目标硬件**: 正点原子 ESP32-S3 BOX 开发板（DNESP32S3B，资料盘 `C:\【正点原子】ESP32 AI BOX1`）
**目标**: 保留全部现有功能（ESP-NOW 音频流、HID 按键转发、六界面 LVGL UI、SD 卡图片、演示模式），**新增电容触摸交互**。

---

## 1. 硬件差异总览（调研结论）

| 模块 | K10（源） | S3 BOX（目标） | 移植动作 |
|------|-----------|---------------|---------|
| MCU | ESP32-S3, 16MB Flash, PSRAM | ESP32-S3, 16MB Flash, **八线 PSRAM 80MHz** | 无需改 |
| LCD | ILI9341 **SPI**，240×320 | ST7789VW **8080 并口**（D0-D7 + WR/RD/DC/CS），240×320 | **显示驱动重写** |
| 触摸 | 无 | **CHSC5432 电容触摸**（I2C 地址 0x2E，复位/中断经 XL9555） | **新增**（用户核心需求） |
| 麦克风 | ES7243E I2S ADC（仅录） | **ES8311 codec**（录+放，I2C 地址 0x18） | 音频驱动替换 |
| 喇叭 | 无 | NS4150B 功放 + 喇叭（ES8311 DAC 输出，经 XL9555 SPK_CTRL 使能） | 可选新增（TTS 回放） |
| 按键 | A/B 两键（XL95x5 扩展） | KEY0/KEY1（XL9555 扩展）+ BOOT（GPIO0） | 引脚映射改 |
| IO 扩展 | XL95x5，I2C 地址 0x20 类似 | XL9555，地址 0x20 | 兼容，寄存器布局不同 |
| SD 卡 | SPI（CS 40 / MOSI 42 / MISO 41 / SCLK 44） | SPI2（**SCLK 7 / MOSI 16 / MISO 15 / CS 17**） | 挂载点与引脚改 |
| I2C | SDA 47 / SCL 48 | **SDA 48 / SCL 45**，400kHz | 引脚改 |
| LED | 背光控制 | LEDR（XL9555）、LCD 背光（XL9555 LCD_BL_IO） | 适配 |

**关键兼容性结论**：
- ✅ 两边都是 **LVGL 8**，六界面 UI 代码可整体复用
- ✅ 分辨率同为 240×320，UI 布局不用动
- ✅ LovyanGFX 原生支持 S3 的 **8080 并口**（`Bus_Parallel8`）
- ✅ ESP-NOW / 协议层（`espnow_protocol.h`）与硬件无关，零修改
- ✅ PSRAM + 16MB Flash 配置一致，`LV_IMG_CACHE_DEF_SIZE 4` 等配置可沿用

---

## 2. 框架选型（关键决策）

### 方案 A：保留 Arduino + PlatformIO + LovyanGFX（推荐）

- 应用层 67KB 的 `51_mic_wifi.ino` 逻辑**几乎原样保留**（ESP-NOW、按键状态机、六界面、演示模式）
- 新写一个 `gfx/atk_box_gfx.cpp` 显示配置：LovyanGFX `Bus_Parallel8` + `Panel_ST7789`，替换原 `main.h` 里的 ILI9341 SPI 配置
- 触摸驱动：用 Arduino `Wire` 直驱 CHSC5432（读 0x2E 的坐标寄存器），注册为 LVGL indev —— 正点原子的 `chsc5xxx.c` 是 IDF API 写的，不能直接用，但**寄存器协议部分照抄**（约 100 行）
- ES8311：正点原子 `es8311.c` 的 45 个寄存器初始化序列同样可移植为 Arduino `Wire` 版本（官方例程 24_recoding 有完整录音配置参考）
- **工作量**: 约 3-4 天

### 方案 B：整体迁移到 ESP-IDF

- 正点原子例程 `30_lvgl_demo(ES8311)` 提供全套现成 IDF BSP（LCD/触摸/ES8311/SD/XL9555）
- 但需把整个应用层从 Arduino 重写成 `app_main` + FreeRTOS 任务结构：`.ino` 里的 `setup()/loop()`、按钮去抖状态机、LVGL 创建/更新函数、ESP-NOW 回调全部要按 IDF 风格重组
- **工作量**: 约 6-8 天，且后续 K10 分支的功能同步维护成本翻倍

### 决策依据

方案 A 把改动集中在**一个硬件抽象层文件**，UI 和业务逻辑零改动，风险最小；正点原子的 IDF 驱动作为寄存器配置的权威参考。**推荐方案 A**。

> 待用户确认：是否接受 Arduino 框架？若希望顺带切换 IDF（例如为了后续接小智 AI 官方工程 `xiaozhi-esp32s3_box.zip`），则选方案 B。

---

## 3. 移植后目录结构（方案 A）

```
examples/52_code_buddy_AI BOX/
├── 51_mic_wifi.ino          # 主逻辑（不变，仅引脚宏和 initBoard 调整）
├── espnow_protocol.h         # 协议（零修改，仍与 Dongle 同步）
├── main.h                    # 硬件抽象：改为条件编译，按板型选驱动
│   ├── #define BOARD_K10     → 原 ILI9341 SPI + ES7243E 路径（保留）
│   └── #define BOARD_ATK_BOX → 新 ST7789 并口 + ES8311 + CHSC5432 路径
├── atk_box_gfx.h/.cpp        # 新增：LovyanGFX 并口屏 + 触摸 indev + ES8311 初始化
└── (K10 原有 initBoard.h 路径在 ATK_BOX 下不参与编译)
```

同时需要在仓库根 `platformio.ini` 新增 `[env:52_code_buddy_AI_BOX]` 环境（注意目录名含空格，需把目录重命名为 `52_code_buddy_ai_box` 或在 `src_dir` 里加引号处理）。

---

## 4. 分阶段实施计划

### 阶段 0：环境搭建与基线（0.5 天）

1. 复制 `51_mic_wifi` 环境为 `52_code_buddy_ai_box`（`platformio.ini`）
2. 验证新板 USB 串口、下载（典型 COM 口待插入后确认，CH343 USB 转串口）
3. 里程碑：空工程能编译烧录，串口通

### 阶段 1：显示驱动移植（1 天）★ 最高风险

1. `atk_box_gfx.cpp` 配置 LovyanGFX：
   - `Bus_Parallel8`：D0=40, D1=39, D2=38, D3=12, D4=11, D5=10, D6=9, D7=46；WR=42, RD=41, DC=2, CS=1
   - `Panel_ST7789`：240×320，背光经 XL9555 `LCD_BL_IO` (0x0080)
   - 先跑 LovyanGFX 自测（纯色填充 + 渐变条），排除并口时序问题
2. LVGL 移植层对接：`lv_disp_drv` flush 回调改调 LovyanGFX `pushPixels`（照抄 K10 现有结构，仅换句柄）
3. XL9555 Arduino 驱动（`Wire` 版，参考正点 `xl9555.c` 寄存器操作），先点亮背光
4. 里程碑：LVGL demo 界面显示正常

### 阶段 2：触摸驱动 + LVGL indev（0.5-1 天）★ 用户核心需求

1. CHSC5432 Arduino 驱动：`Wire.beginTransmission(0x2E)` 读触摸点（寄存器协议照抄 `chsc5xxx.c`）；复位经 XL9555 `CTP_RST_IO` (0x0040)
2. 注册 LVGL `lv_indev_drv_t`（type = `LV_INDEV_TYPE_POINTER`），read 回调填 `data->point`（参考正点 `lvgl_demo.c` 的 `touchpad_read`）
3. **触摸交互设计**（保持与按键并存）：
   - 水平滑动 = 切换界面（替代/等同于按键 B 短按）
   - 界面 1 Token：点按某行 = 高亮该行（等同 A 短按循环高亮）
   - 界面 3 Inspo：点按正文区域 = 重启打字机动画
   - 界面 4 Profile：点按照片区域 = 切换用户照片
   - 界面 0/5：点按 REC 按钮区域 = 切换音频流（等同 A 短按）
   - 顶部状态栏常驻：模式图标 + 电量（后续）
4. 手势识别：用 LVGL 8 自带 `lv_indev` gesture 事件或简单的自实现 swipe 检测（按下坐标→抬起坐标，水平位移 >60px 且垂直 <40px）
5. 里程碑：可触摸滑动切屏，六个界面全部可触控操作

### 阶段 3：音频驱动替换（1 天）

1. ES8311 Arduino 驱动：I2C 寄存器初始化（参考正点 `es8311.c` 45 个寄存器序列；正点例程 `24_recoding(ES8311)` 是纯录音场景，配置最接近我们的需求）
2. I2S 引脚改：BCLK=21, WS=13, DIN(ES8311 DOUT→ESP)=47, DOUT(ESP→ES8311 DIN)=14, MCLK=NC（注意：与 K10 的 MCLK 3/BCLK 0/WS 38/DIN 39 完全不同）
3. 采样率 16kHz 单声道，保持与 Dongle 协议一致；确认 ES8311 在 16k 下 ADC 分频配置（正点例程默认 44.1k，需改采样率分频寄存器）
4. 音频通路联调：本机录音 → ESP-NOW 发送 → Dongle → PC 出声
5. 里程碑：对着新板说话，PC 端收到清晰音频（延迟 <50ms）

### 阶段 4：按键与 SD 卡（0.5 天）

1. 按键映射：KEY1（XL9555 `KEY1_IO` 0x0008）= 按键 A；KEY0（0x0010）= 按键 B；BOOT（GPIO0，按住进入下载模式的风险需注意——**BOOT 键不适合做功能键**，仅 KEY0/KEY1 映射 A/B）
2. XL9555 INT（GPIO3）可用中断方式读键，也可轮询（沿用 K10 轮询逻辑更省事）
3. SD 卡：SPI2 引脚改 SCLK=7/MOSI=16/MISO=15/CS=17；Arduino SD 库 `SD.begin(17, SPI, 25000000)`，挂载点维持 `D:` 供 LVGL 用
4. 里程碑：A/B 按键全部功能等同 K10；SD 卡图片（user.png/ai.png）在界面 4/5 正常显示

### 阶段 5：整机联调 + 演示模式验证（0.5-1 天）

1. 全功能回归：按 CLAUDE.md「Testing the System」清单逐项验证（音频路径、键盘路径、状态显示、演示模式 A+B 2 秒）
2. 与 Dongle + PC 端到端联调（注意：K10 MAC 地址是编译进代码的，新板 MAC 不同——**Dongle 的 `config.h` 需要把 K10 MAC 换成新板 MAC，或改用广播/配对帧**）
3. 演示模式 + 触摸交互演示
4. 里程碑：全部功能在新板复现，触摸可用

**总工作量（方案 A）**: 4-5 天。最乐观 3 天，悲观 7 天（若并口屏时序或 ES8311 16kHz 配置踩坑）。

---

## 5. 主要风险与应对

| # | 风险 | 等级 | 应对 |
|---|------|------|------|
| 1 | LovyanGFX 并口 + ST7789 时序不匹配（白屏/花屏） | 高 | 先跑纯色自测隔离问题；必要时降并口写速率；参考正点 IDF `lcd.c` 的初始化命令序列补 `init_sequence` |
| 2 | ES8311 16kHz 录音配置（正点例程只有 44.1k/48k） | 中 | 查 ES8311 数据手册 ADC 分频寄存器；或先用 16k 直接试（内部 PLL 通常支持任意常用采样率） |
| 3 | 新板 MAC 与 Dongle 过滤不匹配 → ESP-NOW 收不到 | 中 | 改 Dongle `config.h`；或升级为配对帧（协议里已有 0x04 Pair） |
| 4 | XL9555 与 K10 的 XL95x5 寄存器布局差异（背光/按键位置不同） | 低 | 直接按正点原子 `xl9555.h` 的 IO 位定义写，不沿用 K10 位定义 |
| 5 | 目录名含空格导致 PlatformIO `src_dir` 解析问题 | 低 | 重命名目录或用引号路径；建议顺便规范化 |
| 6 | PSRAM 八线模式与 K10 四线差异导致 LovyanGFX 帧缓冲异常 | 低 | PlatformIO board 配置用标准 `esp32s3box` 类定义，PSRAM flag 已验证（OCT 80M） |

---

## 6. 触摸交互详细设计（新功能规格）

**设计原则**: 触摸是按键的**补充**而非替代——保留 A/B 物理键全部行为（演示、F2/Enter/Backspace 转发），触摸负责"看和点"的自然交互。

### 6.1 全局手势

| 手势 | 动作 | 等同按键 |
|------|------|---------|
| 水平滑动（>60px） | 切换到上/下一界面 | B 短按 |
| 长按屏幕 (>1.5s) | 无（保留给未来"进入/退出演示模式"） | — |

### 6.2 各界面触控区

| 界面 | 触控点 | 动作 |
|------|--------|------|
| 5 AI Status | 云朵脸区域 | 手动切换情绪（Thinking→Coding→Done 循环） |
| 2 Coding Status | 项目行 | 无（显示类） |
| 1 Token Usage | 服务行 | 高亮该行（A 短按的直达版） |
| 3 Product Inspo | 正文卡片 | 重启打字机动画 |
| 4 User Profile | 照片区域 | 切换下一张用户照片 |
| 0 ESP-NOW Status | REC/IDLE 指示区 | 切换音频流（等同 A 短按） |

### 6.3 实现要点

- 每个界面在 `create_screen_X()` 时给可点对象挂 `lv_event_cb`（`LV_EVENT_CLICKED`），无需自建 hit-test
- 滑动手势：在 LVGL screen 对象上挂 `LV_EVENT_GESTURE`（LVGL 8 原生支持，`lv_indev_get_gesture_dir`），比自实现稳
- 触摸与按键操作同一 `xGuiSemaphore` 线程安全规则（见 CLAUDE.md 惯例）
- 所有触摸事件同样走"标志位 + 主循环处理"模式（与现有 `screen_dirty` 架构一致），不在回调里直接操作 LVGL

---

## 7. 验收标准

**功能保留（全部通过才算移植成功）**:
- [ ] ESP-NOW 音频流：A 短按开始/停止，PC 录音设备有电平，延迟 <50ms
- [ ] HID 按键：A 短按 F2 / A 长按 Enter / B 长按 Backspace 连发
- [ ] 六界面循环 + 各界面数据显示正确
- [ ] SD 卡 user.png / ai.png 在界面 4/5 正常显示
- [ ] 演示模式（A+B 2 秒）+ 三个界面的演示专属交互
- [ ] AI Status 情绪自动循环 + 眨眼动画

**新增功能**:
- [ ] 触摸滑动切换界面，灵敏度可接受（误触率 <5%）
- [ ] 六界面各自的触控点全部生效
- [ ] 触摸与物理按键可混合使用，无冲突/无死机

**稳定性**:
- [ ] 连续运行 1 小时无崩溃、无内存泄漏（触摸轮询 + LVGL 动画并发）
- [ ] 屏幕切换流畅度与 K10 相当（注意 `LV_IMG_CACHE_DEF_SIZE 4` 沿用）

---

## 8. 决策记录（用户已确认，2026-08-27）

1. **框架**: ✅ **方案 A（Arduino + PlatformIO + LovyanGFX）**
2. **喇叭**: ✅ **本次一并启用** —— ES8311 DAC + NS4150B 功放，移植时录音与播放通路都打通（提示音/TTS 预留接口）
3. **Dongle MAC**: ✅ **升级配对协议** —— 不硬编码新板 MAC，改用 Pair 帧（0x04）动态配对：发射端广播配对请求 → Dongle 学习并存 NVS → ACK 确认绑定（需同步改 `dongle_firmware`）
4. **目录**: ✅ **重命名为 `52_codebuddy_ai_box`**（去空格），同步更新 CLAUDE.md 与 platformio.ini 引用
5. **屏幕方向**: 联调时实测触摸坐标方向，必要时在驱动里做镜像/交换 XY 校准（一次宏定义即可）

> 决策 3 的连带影响：`espnow_protocol.h` 两端（发射端 + `dongle_firmware/main/`）必须同步更新并保持字节一致；Dongle 需新增 NVS 存储已配对 MAC + 配对状态 LED 指示（可复用现有 WS2812）。
> 决策 2 的连带影响：新增阶段 3 的播放通路测试（I2S TX → ES8311 DAC → 功放）；SPK_CTRL 使能经 XL9555（0x0020）。

---

## 9. 实施进度

✅ **已完成** (2026-08-27):

| 阶段 | 内容 | 产出文件 |
|------|------|---------|
| 0 | 目录重命名 + PlatformIO 环境 | `platformio.ini` [env:52_codebuddy_ai_box] |
| 1 | 显示驱动 (Bus_Parallel8 + ST7789) | `AtkBoxGfx.hpp` |
| 2 | 触摸驱动 (CHSC5432) + LVGL indev + 滑动手势 | `AtkBoxTouch.h` + ino |
| 3 | 音频驱动 (ES8311 16kHz + 喇叭) | `AtkBoxAudio.h` |
| 4 | 按键(XL9555) + SD卡(SPI2) 适配 | `AtkBoxXL9555.h` + ino |
| 5 | 配对协议升级 (动态 MAC + NVS) | `pairing.c/h` + 两端 ino/c 修改 |
| 整合 | 板级初始化统一入口 | `AtkBoxBoard.h` |

**编译验证**: ATK BOX 设备端 **✅ 编译成功** (Flash 24.3% / 1.14MB, RAM 14.9%)，与 K10 原工程占用一致。

⏳ **待上电验证** (需实机):

1. **Dongle 端编译** — 在 ESP-IDF 环境 `cd dongle_firmware && idf.py build`（本地未装 IDF）
2. **首次配对** — ATK BOX 上电广播 PAIR → Dongle 学 MAC → 回 ACK → 双方 NVS 持久化
3. **触摸校准** — 测试滑动方向，必要时改 `AtkBoxTouch.h` 的 `TOUCH_SWAP_XY`/`TOUCH_FLIP_X/Y` 宏
4. **显示校准** — 若花屏/白屏，调 `AtkBoxGfx.hpp` 的 `cfg.invert` / `freq_write`；若明暗反转调 `invert`
5. **六界面** — 按键 B + 触摸左右滑切屏 + A+B 2秒演示模式
6. **音频录音** — 按键 A → Dongle LED 闪 + PC 录音电平
7. **喇叭放音** — `es8311.setMute(false)` + 播放测试（提示音接口预留）
8. **SD 卡** — FAT32 格式化 + `user.png`/`ai.png` → 界面 4/5 显示

⚠️ **上电前须确认的硬件项**:
- **PSRAM 配置**: 当前 `platformio.ini` 的 board 是 `esp32-s3-devkitc-1`（默认 N8 无 PSRAM）。ATK BOX 是 **16MB Flash + 8线 PSRAM**，LVGL 双缓冲依赖 PSRAM。若上电报 PSRAM 分配失败，需确认 `board_build.arduino.memory_type = qio_opi` 生效（已在 [env] 中）。
- **LCD invert**: ST7789 出厂默认反色，`AtkBoxGfx.hpp` 已设 `cfg.invert = true`，若实际颜色反了则改回 false。

---

## 10. 实施变更记录

### 变更 1: 移除触摸滑动翻页 (2026-08-31)

**原计划**（第 6.1 节）:
- 全局手势: 水平滑动（>60px）切换到上/下一界面，等同按键 B 短按
- 阶段 2 里程碑: "可触摸滑动切屏，六个界面全部可触控操作"

**实际实施**:
- 触摸滑动翻页功能在实施后经用户实测，发现 CHSC5432 电容触摸横向滑动灵敏度不足，翻页不可靠
- **已于 2026-08-31 移除**触摸滑动翻页代码（删除 `51_mic_wifi.ino` loop() 内的滑动检测逻辑）
- 翻页现在**仅由按键 B (K1) 短按**完成
- 触摸仅保留界面内点击交互（详情卡片、AI 情绪点击、TouchTest 画点），由 LVGL 输入驱动独立处理

**影响**:
- 验收标准第 8 条"触摸滑动切换界面，灵敏度可接受"已不适用
- 按键 B 短按从原计划的"Enter 确认"改为"翻页"（Enter 确认移到 A 长按）
- 触摸交互降级为"辅助输入"，主交互回归物理按键

**原因**:
- 用户需求变更：物理按键翻页更可靠
- 硬件限制：CHSC5432 横向滑动识别率 <50%，用户体验不佳

详细按键映射见 `按键交互设计.md` 中的"ATK BOX 实际映射"表格。

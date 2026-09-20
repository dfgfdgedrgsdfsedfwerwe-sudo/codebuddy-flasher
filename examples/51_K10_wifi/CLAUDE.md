# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**CodeBuddy Wireless** (51_mic_wifi) — 一个基于 ESP32-S3 的无线键盘 + 音频流系统，带有丰富的状态显示界面。

这是 DFRobot UNIHIKER K10 开发板（ESP32-S3, 16MB flash, PSRAM, 240x320 ILI9341 SPI LCD, 板载 SD 卡）的主要演示项目，位于更大的多示例仓库 `dfk10_arduino_demo-master` 中的 `examples/51_mic_wifi/` 目录。

**完整系统**: K10 发送端 (本项目) + ESP32-S3 Dongle 接收端 (`../../dongle_firmware/`) = 无线音频输入设备 + HID 键盘

## 构建和开发命令

本项目使用 **PlatformIO**（Arduino 框架）。**重要**: 使用 `python -m platformio`（`pio`/`platformio` 命令不在 PATH 中）。

```bash
# 从仓库根目录执行（C:\Users\4090\Desktop\dfk10_arduino_demo-master）
cd C:\Users\4090\Desktop\dfk10_arduino_demo-master

# 编译本项目
python -m platformio run -e 51_mic_wifi

# 烧录到 K10（COM4 是常用端口，根据实际调整）
python -m platformio run -t upload -e 51_mic_wifi --upload-port COM4

# 串口监视器
python -m platformio device monitor -p COM4 -b 115200

# 完整工作流（编译 + 烧录 + 监视）
python -m platformio run -t upload -e 51_mic_wifi --upload-port COM4 && python -m platformio device monitor -p COM4 -b 115200
```

**配套 Dongle 固件**（必须配合使用）:
```bash
cd ../../dongle_firmware
idf.py build                    # 编译（ESP-IDF 项目，不是 PlatformIO）
idf.py -p COM6 flash monitor    # 烧录到 Dongle（COM6 是常用端口）
```

**Git 子模块**（首次编译前必须初始化）:
```bash
cd C:\Users\4090\Desktop\dfk10_arduino_demo-master
git submodule update --init --recursive   # 初始化 LVGL 和 LovyanGFX
```

## 系统架构

### 数据流

```
K10 (本项目) ←→ [ESP-NOW 2.4GHz] ←→ Dongle ←→ [USB] ←→ PC
```

**K10 发送端** (`examples/51_mic_wifi/`):
- I2S 麦克风 (ES7243E) → ESP-NOW 音频流 (16kHz mono)
- 2 个按键 (A/B) → HID 键盘事件 (F2/Enter/Esc/Backspace)
- 6 个 LVGL UI 屏幕（ESP-NOW 状态、Token 使用、编码状态、灵感、用户资料、AI 状态动画云脸）
- SD 卡图片显示 (240×320 PNG)
- 接收来自 PC 的状态帧（通过 Dongle 转发）

**Dongle 接收端** (`../../dongle_firmware/`):
- USB 复合设备: HID 键盘 + USB Audio (UAC 1.0, 16kHz mono)
- 转发 K10 的按键事件和音频到 PC
- （未来）转发 PC 状态数据到 K10

**协议**: `espnow_protocol.h`（K10 和 Dongle 共享，必须保持字节级一致）
- 帧类型: 0x01 按键, 0x02 音频, 0x04 配对, 0x06 心跳, 0x07 Token 状态, 0x08 项目状态, 0x09 AI 情绪
- CRC8 校验（多项式 0x07）
- ESP-NOW 配置: 信道 1, K10 MAC `3c:dc:75:6d:7f:b4`, Dongle MAC `e0:72:a1:d4:8f:e0`

### 按键交互

**基础交互**（所有模式）:
| 按键 | 短按 (<600ms) | 长按 (≥600ms) |
|------|--------------|--------------|
| A | F2 + 切换音频流 | Enter（确认）|
| B | 循环 UI 屏幕 | Backspace（每 100ms 重复）|
| A+B | 按住 2 秒 = 演示模式（向所有 6 个屏幕注入模拟数据）| |

**演示模式专属交互**（进入 Demo Mode 后生效）:
| 屏幕 | 按键 A 短按 | 按键 A 长按 |
|------|------------|------------|
| 1 - Token 使用 | 循环高亮下一行（高亮行背景变浅灰）| 重置所有进度条到 10% |
| 3 - 产品灵感 | 重启打字机动画（清空 → 逐字显示）| （保留为 Enter）|
| 4 - 用户资料 | 切换用户照片（SD 卡或 fallback 渐变）| （保留为 Enter）|

详见: `按键交互设计.md`

### UI 屏幕

**屏幕顺序**（按 B 键循环，可通过 `SCREEN_ORDER[]` 数组自定义）:
1. **屏幕 5 - AI 状态**（启动默认屏幕）: 带表情的动画云
   - 三种情绪: Thinking（眉毛上扬，嘴闭合）, Coding（眉毛皱起，嘴动）, Done（眉毛放松，微笑）
   - 自动循环每 4 秒；或由 PC 通过 AI Emotion 帧（0x09）驱动
   - 眼睛每 3 秒眨眼，Coding 状态嘴部动画
   - 显示状态文本（默认或来自 PC 的自定义文本）
2. **屏幕 2 - 编码状态**: 项目列表，带彩色状态点
3. **屏幕 1 - Token 使用**: AI 服务令牌消耗（进度条）
   - **演示模式交互**: A 短按循环高亮 → A 长按重置进度
4. **屏幕 3 - 产品灵感**: 灵感卡片，带打字机动画
   - **演示模式交互**: 进入时自动打字机动画（40ms/字符）+ 闪烁光标（500ms）→ A 短按重启
5. **屏幕 4 - 用户资料**: 用户照片（来自 SD `D:/user.png`）+ 名字
   - **演示模式交互**: A 短按切换用户照片（SD 卡图片或 fallback 渐变色块）
   - **SD 卡检测**: 启动时扫描 `D:/user.png`, `D:/user1.png` ... (最多 10 张)
6. **屏幕 0 - ESP-NOW 状态**: 连接统计，数据包计数

**演示模式**: 同时按 A+B 2 秒，用模拟数据填充所有屏幕。

## 关键实现细节

### SD 卡图片显示（屏幕 4 & 5）

**设置要求**:
1. SD 卡必须是 **FAT32**（64GB 卡默认 exFAT，会导致 `f_mount` 失败）
2. 图片文件: `D:/user.png` (240×320), `D:/ai.png` (240×320 无脸云)
3. LVGL 自定义文件系统驱动使用盘符 **'D'**（不是 'S'，会与 LV_FS_FATFS_LETTER 冲突）
4. SD 初始化 **必须在** `create_screen_*` 调用之前（屏幕检查 `sd_card_ready` 标志）

**关键配置** (`lv_conf.h`):
- `LV_IMG_CACHE_DEF_SIZE 4` — **关键**: 没有缓存，PNG 每帧重新解码（100+ms），导致循环延迟，使按键 B 短按被误检测为长按，冻结屏幕切换。
- `LV_USE_PNG 1` — 启用内置 PNG 解码器

**代码结构** (`main.h`):
- `fs_sd_open/close/read/seek/tell` — 桥接 LVGL 到 SD 卡的回调
- `lv_fs_sd_init()` — 向 LVGL 注册 'D:' 驱动

### AI 情绪状态机（屏幕 5）

**状态** (`ai_state_t` 枚举):
- `AI_STATE_THINKING` (0): 眉毛上扬 (`FACE_BROW_Y - 4`), 小闭口, "Thinking..."
- `AI_STATE_CODING` (1): 眉毛下降 (`FACE_BROW_Y + 5`), 动画嘴（每 250ms 开/关）, "Coding..."
- `AI_STATE_DONE` (2): 中性眉毛, 宽笑容（26×14px）, "Done!"

**行为**:
- **本地模式**（默认）: 每 4 秒自动循环状态（`AI_STATE_DURATION_MS`）
- **外部模式**: 从 PC 接收 AI Emotion 帧（0x09）；覆盖本地循环；15 秒超时后恢复本地模式

**实现** (`apply_ai_state()`):
- 眉毛: 通过 `lv_obj_align()` 与 Y 偏移定位
- 嘴: 通过 `lv_obj_set_size()` 调整大小以实现不同形状
- 文本: 来自 PC 的自定义文本或每个状态的默认文本
- 眼睛: 眨眼动画（每 3 秒）独立于状态

**关键**: `ai_state_t` 枚举必须在文件顶部定义（Arduino 在函数定义前自动生成 `apply_ai_state()` 的前向声明）。

### LVGL 线程安全

**所有 LVGL 操作必须持有 `xGuiSemaphore`** 以防止竞态条件:
- `switch_screen()` — 在 `lv_scr_load()` 和 `update_*` 调用前获取信号量
- 动画刷新循环（屏幕 5 的 100ms 定时器）— 用信号量包裹 `update_screen_ai_status()`
- 主更新块（`screen_dirty` 检查）— 受保护
- `lv_task_handler()` — 受保护

**缺少锁的症状**: 屏幕切换冻结（尤其是从屏幕 5），短按误检测为长按。

### 屏幕切换架构

**集中式切换**:
```c
static const uint8_t SCREEN_ORDER[6] = {5, 2, 1, 3, 4, 0};  // 可自定义顺序
static uint8_t order_index = 0;

// 按键 B 处理
order_index = (order_index + 1) % 6;
switch_screen(SCREEN_ORDER[order_index]);
```

**优势**: 单一真相来源；易于重新排序或更改启动屏幕。

### PC-到-K10 状态协议（未来上位机）

详见 `上位机开发文档.md` 获取完整规范。

**帧类型**（全部小端序，紧凑结构体）:
- **0x07 Token 状态** (154 字节): 最多 5 个 AI 服务，带名称（20 字符）、已用/总计令牌、百分比×10
- **0x08 项目状态** (154 字节): 最多 6 个项目，带名称（24 字符）、状态码（0=规划, 1=编码, 2=审查, 3=完成, 4=错误, 5=空闲）
- **0x09 AI 情绪** (24 字节): 情绪码（0/1/2）、自定义文本（20 字符），覆盖本地动画

**数据路径**: `PC 应用 → USB 串口（115200）→ Dongle（串口收→ESP-NOW 转发，需要实现）→ K10 (espnow_recv_cb)`

**K10 接收器**: `51_mic_wifi.ino` 中的 `espnow_recv_cb()` 验证 CRC8，复制到全局变量（`token_data`, `project_data`, `ai_state`），设置 `screen_dirty` 或 `ai_state_changed` 标志。

## 关键文件及其作用

### 核心源文件

- **51_mic_wifi.ino** — 主逻辑: ESP-NOW、按键、屏幕管理、动画状态机
- **espnow_protocol.h** — 共享协议定义（必须与 Dongle 的 `../../dongle_firmware/main/espnow_protocol.h` 同步）
- **main.h** — 硬件初始化（I2S、LCD、SD）、LVGL 显示/输入驱动、SD 文件系统桥接
- **UNIHIKER_K10_PIN.h** — GPIO 引脚映射
- **上位机开发文档.md** — PC 应用开发者协议规范（帧格式、CRC8、Python 示例）
- **按键交互设计.md** — 按键交互设计文档

**屏幕创建模式**:
```c
static void create_screen_X();   // 为屏幕 X 创建 LVGL 对象
static void update_screen_X();   // 更新屏幕 X 上的数据
```

**更新触发**: 设置 `screen_dirty = true` 或使用 100ms 定时器（屏幕 5 动画）。

## 硬件特定约束

**K10 开发板**:
- Flash: 51_mic_wifi 使用约 24%（1.1MB / 4.7MB）
- PSRAM: LVGL 帧缓冲区和图像缓存所需
- SD 卡: SPI 模式，与 LCD 共享总线（不同引脚的 CS）
- LCD: ILI9341, 240×320, 16 位色，DMA 传输
- 麦克风: ES7243E (I2S), 16kHz 立体声（下混为单声道传输）

**ESP-NOW 限制**:
- 最大负载: 250 字节
- 音频帧: 120 字节（60 样本 × 2 字节）+ 10 字节头
- 可靠传送: K10 跟踪 `sendFailCount`；Dongle 无重试

**典型端口**:
- K10: COM4（Windows，根据需要调整 `--upload-port`）
- Dongle: COM6

## 常见陷阱和解决方案

1. **屏幕 5 无法切换**: 检查 `LV_IMG_CACHE_DEF_SIZE` 是否为 4（不是 0）。没有缓存，循环减慢到 300+ms，导致按键时序问题。

2. **SD 卡挂载失败**: 格式化为 FAT32。Windows 快速格式化 64GB+ 默认为 exFAT。

3. **`update_screen_ai_status()` 崩溃**: 始终检查 `ai_thinking_arc != NULL` 后再访问（SD 图像模式激活时为 NULL）。

4. **K10 和 Dongle 之间协议不匹配**: 编辑一方的 `espnow_protocol.h` 后，**立即复制到另一方**（它们必须字节级相同）。

5. **LVGL UI 故障**: 确保所有 LVGL 调用（`lv_port_*` 驱动中除外）持有 `xGuiSemaphore`。

6. **找不到自定义字体**: 在 `lv_conf.h` 中启用: `#define LV_FONT_MONTSERRAT_18 1`（以及 20、28 根据需要）。

## 测试系统

**音频路径**:
1. 烧录 Dongle，连接到 PC
2. 烧录 K10
3. K10 上短按按键 A → Dongle 上 LED 闪烁 → PC 录音设备显示电平
4. 验证延迟 <50ms（对着 K10 麦克风说话，通过 PC 应用监听）

**键盘路径**:
1. 打开 Windows 资源管理器，选择文件
2. 短按 A → 文件进入重命名模式（收到 F2 键）
3. 长按 B → 字符连续删除（Backspace 重复）
4. 短按 B → 重命名确认（Enter 键）

**状态显示**:
1. 按 A+B 2 秒 → 所有 6 个屏幕填充演示数据
2. 重复按 B → 按顺序循环屏幕: 5→2→1→3→4→0
3. 屏幕 5: 观察云脸每 4 秒自动循环 Thinking→Coding→Done，眼睛眨眼

**调试**:
- K10 串口: `python -m platformio device monitor -p COM4`
- Dongle 串口: `idf.py -p COM6 monitor`
- 查找: "ESP-NOW init OK", "SD mounted", "Switched to screen N", "AI state received"

## 相关文档

- `上位机开发文档.md` — PC 应用集成指南（帧格式、CRC8、Python 示例）
- `按键交互设计.md` — 按键交互设计理由
- `../../dongle_firmware/README.md` — Dongle 特定构建和架构说明（LED 状态、USB UAC、故障排除）
- `../../dongle_firmware/CLAUDE.md` — Dongle 项目的 Claude 指导文档
- `.claude/projects/.../memory/` 中的内存文件 — 详细的验证日志和故障排除历史

## 仓库上下文

本项目是 `dfk10_arduino_demo-master` 仓库中 40+ 示例之一:
- **父仓库根目录**: `../../` (C:\Users\4090\Desktop\dfk10_arduino_demo-master)
- **本项目位置**: `examples/51_mic_wifi/`
- **配套 Dongle**: `../../dongle_firmware/` (独立的 ESP-IDF 项目)
- **PlatformIO 配置**: `../../platformio.ini` (设置 `default_envs = 51_mic_wifi` 选择本项目)

**切换到其他示例**: 编辑 `../../platformio.ini` 并更改 `default_envs`，然后从仓库根目录运行 `python -m platformio run -e <env_name>`。

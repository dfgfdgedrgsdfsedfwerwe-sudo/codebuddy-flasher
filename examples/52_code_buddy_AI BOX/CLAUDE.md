# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**CodeBuddy Wireless + AI Cloud Integration** — This directory (`52_code_buddy_AI BOX`) is an evolution of the `51_mic_wifi` project with planning documents for integrating Xiaozhi AI cloud capabilities. The source code (`51_mic_wifi.ino`) is currently identical to `../51_mic_wifi/` but housed here for future AI cloud feature development.

**Current system**: K10 transmitter (this project) + ESP32-S3 Dongle receiver (`../../dongle_firmware/`) = wireless audio input device + HID keyboard.

**Planned enhancement**: Add WiFi → Xiaozhi AI cloud voice dialogue mode alongside the existing ESP-NOW → Dongle mode, with user-switchable modes.

This is a PlatformIO Arduino project for the DFRobot UNIHIKER K10 board (ESP32-S3, 16MB flash, PSRAM, 240x320 ILI9341 SPI LCD, onboard SD card).

## Repository Structure Context

This is **one example in a 40+ example repository**:
- **Parent repository root**: `../../` (C:\Users\4090\Desktop\dfk10_arduino_demo-master)
- **This project location**: `examples/52_code_buddy_AI BOX/`
- **Source twin**: `examples/51_mic_wifi/` (currently identical .ino file)
- **Companion Dongle**: `../../dongle_firmware/` (separate ESP-IDF project, not PlatformIO)
- **PlatformIO config**: `../../platformio.ini` (currently points to `51_mic_wifi` env, not `52_code_buddy_AI BOX`)

⚠️ **Build system caveat**: There is no `52_code_buddy_AI BOX` environment in `platformio.ini` yet. This directory is a development workspace for AI cloud integration planning. To build, you must use the `51_mic_wifi` environment from the repository root.

## Build and Development Commands

**Important**: Use `python -m platformio` (the `pio`/`platformio` commands are not in PATH).

```bash
# Work from repository root for PlatformIO commands
cd C:\Users\4090\Desktop\dfk10_arduino_demo-master

# Compile this project (uses 51_mic_wifi env since no 52 env exists yet)
python -m platformio run -e 51_mic_wifi

# Flash to K10 (COM4 is typical, adjust as needed)
python -m platformio run -t upload -e 51_mic_wifi --upload-port COM4

# Serial monitor
python -m platformio device monitor -p COM4 -b 115200

# Helper scripts (run from this directory)
cd examples/"52_code_buddy_AI BOX"
python check_sd_boot.py        # Capture boot logs, check SD card init
python monitor_continuous.py   # Continuous serial monitoring
python test_k10_button.py      # Button interaction testing

# Dongle (ESP-IDF project, NOT PlatformIO)
cd ../../dongle_firmware
idf.py build
idf.py -p COM6 flash monitor   # COM6 is typical for Dongle

# Initialize git submodules (required before first build)
cd C:\Users\4090\Desktop\dfk10_arduino_demo-master
git submodule update --init --recursive
```

## Architecture Overview

### Current System (K10 + Dongle)

**Data flow**: `K10 (this project) ←→ [ESP-NOW 2.4GHz] ←→ Dongle ←→ [USB] ←→ PC`

**K10 Transmitter**:
- I2S microphone (ES7243E) → ESP-NOW audio streaming (16kHz mono)
- 2 buttons (A/B) → USB HID keyboard (F2/Enter/Esc/Backspace)
- 6 LVGL UI screens (ESP-NOW status, Token usage, Coding status, Inspo, User profile, AI Status with animated cloud face)
- SD card image display (240×320 PNG from `D:/user.png`, `D:/ai.png`)
- Receives status frames from PC via Dongle (future)

**Dongle Receiver**:
- USB composite device: HID keyboard + USB Audio (UAC 1.0, 16kHz mono)
- Forwards K10 key events and audio to PC
- (Future) Forwards PC status data to K10

**Protocol**: `espnow_protocol.h` (shared between K10 and Dongle, must stay in sync)
- Frame types: 0x01 Key, 0x02 Audio, 0x04 Pair, 0x06 Heartbeat, 0x07 Token Status, 0x08 Project Status, 0x09 AI Emotion
- CRC8 validation (polynomial 0x07)
- ESP-NOW config: Channel 1, K10 MAC `3c:dc:75:6d:7f:b4`, Dongle MAC `e0:72:a1:d4:8f:e0`

### Planned Enhancement (Xiaozhi AI Cloud)

**See**: `小智AI集成技术方案.md` for complete 6-day implementation plan.

**Vision**: Add WiFi → Xiaozhi AI cloud mode alongside ESP-NOW mode, with user-switchable operation:
- **ESP-NOW mode**: Current functionality (wireless mic/keyboard to Dongle)
- **Xiaozhi mode**: WiFi → WebSocket → xiaozhi.me cloud → real-time voice dialogue

**Key challenges**:
1. **WiFi channel conflict**: ESP-NOW fixed to channel 1 vs router's dynamic channel
2. **Protocol integration**: WebSocket client + PCM audio upload + JSON messaging
3. **Provisioning**: SoftAP hotspot for WiFi credential input (no app required)

**Recommended approach**: Mutually exclusive mode switching (simple, no Dongle firmware changes) rather than concurrent operation.

## Button Interaction

**基础交互** (UX 重定义 2026-08-28，所有模式):
| Button | Short Press (<600ms) | Long Press (≥600ms) |
|--------|---------------------|---------------------|
| KEY1 (A) | 切换录音 (F2 + 音频流) | Esc (取消) |
| KEY0 (B) | Enter (确认) | 连续 Backspace (每 100ms) |
| BOOT (C) | 返回主界面 (AI Status) | 进入 TouchTest 界面 |
| A+B | 同时按 2 秒 = Demo mode (注入所有界面模拟数据) | |

**界面切换**: 触摸左右滑动 (dx>60 && dy<40)，按 SCREEN_ORDER 顺序循环。B 短按不再切屏。

**触摸交互** (Phase 2 已实现，单点触摸单击/长按):
| Screen | 单击交互 |
|--------|----------|
| 1 - Token Usage | 点击服务名 → 弹用量详情卡片 (点卡外关闭) |
| 2 - Coding Status | 点击项目名 → 详情卡片；点击圆点 → 循环切状态 |
| 3 - Product Inspo | 点击标题 → 重启打字机；点击正文 → 暂停/继续 |
| 4 - User Profile | 点击头像 → 切换下一张照片 |
| 5 - AI Status | 点击屏幕 → 切换情绪 (过滤滑动，15s 后恢复自动) |

**演示模式专属按键交互** (进入 Demo Mode 后 Button A 仍保留):
| Screen | Button A 短按 | Button A 长按 |
|--------|---------------|---------------|
| 1 - Token Usage | 循环高亮下一行 | 重置进度 |
| 2 - Coding Status | 切换选择题选项 | 确认选择 |
| 3 - Product Inspo | 逐字显示 | 跳过显示全文 |
| 4 - User Profile | — | 切换用户照片 |

**未实现** (Phase 3/4): 长按类触摸交互 (关注/编辑菜单/暂停动画)、虚拟键盘、照片管理界面、设置界面。

详见: `../../docs/superpowers/specs/2026-08-28-atk-box-touch-interaction-design.md` (设计), `../../docs/superpowers/plans/2026-08-28-atk-box-touch-interaction.md` (实现计划)

## UI Screens

**Screen order** (Press B to cycle, customizable via `SCREEN_ORDER[]` array):
1. **Screen 5 - AI Status** (boot default): Animated cloud with emotional expressions
   - Three emotions: Thinking, Coding, Done (auto-cycles every 4s or driven by PC)
   - Eyes blink every 3s, mouth animates during Coding state
2. **Screen 2 - Coding Status**: Project list with colored status dots
3. **Screen 1 - Token Usage**: AI service token consumption (progress bars)
   - **演示模式交互**: A 短按循环高亮 → A 长按重置进度
4. **Screen 3 - Product Inspo**: Inspiration cards with typewriter animation
   - **演示模式交互**: 进入时自动打字机动画（40ms/字符）+ 闪烁光标（500ms）→ A 短按重启
5. **Screen 4 - User Profile**: User photo (from SD `D:/user.png`) + name
   - **演示模式交互**: A 短按切换用户照片（SD 卡图片或 fallback 渐变色块）
   - **SD 卡检测**: 启动时扫描 `D:/user.png`, `D:/user1.png` ... (最多 10 张)
6. **Screen 0 - ESP-NOW Status**: Connection stats, packet counts

**Demo Mode**: Press A+B together for 2 seconds to populate all screens with mock data.

## Critical Implementation Details

### SD Card Image Display (Screens 4 & 5)

**Setup requirements**:
1. SD card must be **FAT32** (64GB cards default to exFAT which fails `f_mount`)
2. Image files: `D:/user.png` (240×320), `D:/ai.png` (240×320 faceless cloud)
3. LVGL custom filesystem driver uses drive letter **'D'** (not 'S', which conflicts with LV_FS_FATFS_LETTER)
4. SD initialization **must precede** `create_screen_*` calls (screens check `sd_card_ready` flag)

**Key config** (`lv_conf.h`):
- `LV_IMG_CACHE_DEF_SIZE 4` — **Critical**: Without caching, PNG re-decodes every frame (100+ms), causing loop delays that make button B short-press misdetected as long-press, freezing screen transitions.
- `LV_USE_PNG 1` — Enable built-in PNG decoder

**Code structure** (`main.h`):
- `fs_sd_open/close/read/seek/tell` — Callbacks bridging LVGL to SD card
- `lv_fs_sd_init()` — Registers 'D:' drive with LVGL

### AI Emotion State Machine (Screen 5)

**States** (`ai_state_t` enum):
- `AI_STATE_THINKING` (0): Raised brows, small closed mouth, "Thinking..."
- `AI_STATE_CODING` (1): Lowered brows, animated mouth (opens/closes every 250ms), "Coding..."
- `AI_STATE_DONE` (2): Neutral brows, wide smile, "Done!"

**Behavior**:
- **Local mode** (default): Auto-cycles states every 4s (`AI_STATE_DURATION_MS`)
- **External mode**: Receives AI Emotion frames (0x09) from PC; overrides local cycling; reverts to local after 15s timeout

**Critical**: `ai_state_t` enum must be defined at file top (Arduino auto-generates forward declarations for `apply_ai_state()` before function definitions).

### LVGL Threading Safety

**All LVGL operations must hold `xGuiSemaphore`** to prevent race conditions:
- `switch_screen()` — Takes semaphore before `lv_scr_load()` and `update_*` calls
- Animation refresh loop (100ms timer for Screen 5) — Wraps `update_screen_ai_status()` in semaphore
- Main update block (`screen_dirty` checks) — Protected
- `lv_task_handler()` — Protected

**Symptom of missing lock**: Screen transitions freeze (especially from Screen 5), short button presses misdetected as long presses.

### Screen Transition Architecture

**Centralized switching**:
```c
static const uint8_t SCREEN_ORDER[6] = {5, 2, 1, 3, 4, 0};  // Customizable order
static uint8_t order_index = 0;

// Button B handler
order_index = (order_index + 1) % 6;
switch_screen(SCREEN_ORDER[order_index]);
```

**Benefits**: Single source of truth for screen order; easy to reorder or change boot screen.

## Key Files and Their Roles

### Core Source Files

- **51_mic_wifi.ino** — Main logic: ESP-NOW, buttons, screen management, animation state machine
- **espnow_protocol.h** — Shared protocol definitions (must sync with Dongle's copy in `../../dongle_firmware/main/`)
- **main.h** — Hardware init (I2S, LCD, SD), LVGL display/input drivers, SD filesystem bridge
- **UNIHIKER_K10_PIN.h** — GPIO pin mapping

### Planning Documents (Chinese)

- **小智AI集成技术方案.md** — Complete 6-day plan for Xiaozhi AI cloud integration: architecture, challenges (WiFi channel conflict, protocol, provisioning), phased implementation, risks, libraries
- **上位机开发文档.md** — PC app integration guide (frame formats, CRC8, Python examples for status pushes)
- **按键交互设计.md** — Button interaction design rationale
- **状态显示功能.md** — Six-screen UI specification, demo mode interactions, real data protocol
- **技术文档.md** — Complete technical reference: hardware, code structure, protocol details

### Helper Scripts

- **check_sd_boot.py** — Capture K10 boot logs via serial, verify SD card initialization
- **monitor_continuous.py** — Continuous serial monitoring (simpler than platformio monitor)
- **test_k10_button.py** — Button interaction testing utility

## Hardware-Specific Constraints

**K10 Board**:
- Flash: ~24% used by 51_mic_wifi (1.1MB / 4.7MB)
- PSRAM: Required for LVGL frame buffers and image cache
- SD card: SPI mode, shared bus with LCD (CS on different pins)
- LCD: ILI9341, 240×320, 16-bit color, DMA transfers
- Microphone: ES7243E (I2S), 16kHz stereo (downmixed to mono for transmission)

**ESP-NOW limits**:
- Max payload: 250 bytes
- Audio frames: 120 bytes (60 samples × 2 bytes) + 10 byte header
- Reliable delivery: K10 tracks `sendFailCount`; Dongle has no retry

**Typical ports**:
- K10: COM4 (Windows, adjust `--upload-port` as needed)
- Dongle: COM6 (adjust in `idf.py -p` as needed)

## Common Pitfalls and Solutions

1. **Screen 5 won't transition**: Check `LV_IMG_CACHE_DEF_SIZE` is 4 (not 0). Without caching, loop slows to 300+ms causing button timing issues.

2. **SD card mount fails**: Format as FAT32. Windows Quick Format on 64GB+ defaults to exFAT.

3. **Crash in `update_screen_ai_status()`**: Always check `ai_thinking_arc != NULL` before accessing (it's NULL when SD image mode is active).

4. **Protocol mismatch between K10 and Dongle**: After editing `espnow_protocol.h` on one side, **immediately copy to the other** (they must be byte-identical).

5. **LVGL UI glitches**: Ensure all LVGL calls (except in `lv_port_*` drivers) hold `xGuiSemaphore`.

6. **Build fails with missing libraries**: Extract `lib/Arduino_DriveBus` and `lib/ESP32_JPEG` from `../../lib.zip` if folders are empty. Run `git submodule update --init --recursive` for LVGL and LovyanGFX.

## Testing the System

**Audio path**:
1. Flash Dongle, connect to PC
2. Flash K10
3. Short-press button A on K10 → LED on Dongle blinks → PC recording device shows level
4. Verify latency <50ms (speak into K10 mic, listen via PC app)

**Keyboard path**:
1. Open Windows Explorer, select a file
2. Short-press A → File enters rename mode (F2 key received)
3. Long-press B → Characters delete continuously (Backspace repeat)
4. Short-press B → Rename confirmed (Enter key)

**Status display**:
1. Press A+B for 2s → All 6 screens populate with demo data
2. Press B repeatedly → Cycle through screens in order: 5→2→1→3→4→0
3. Screen 5: Observe cloud face auto-cycling Thinking→Coding→Done every 4s with blinking eyes

**Debugging**:
- K10 serial: `python monitor_continuous.py` or `python -m platformio device monitor -p COM4`
- Dongle serial: `idf.py -p COM6 monitor`
- Look for: "ESP-NOW init OK", "SD mounted", "Switched to screen N", "AI state received"

## Related Documentation

- `小智AI集成技术方案.md` — 6-day implementation plan for WiFi + Xiaozhi AI cloud integration
- `上位机开发文档.md` — PC app integration guide (frame formats, CRC8, Python examples)
- `状态显示功能.md` — Six-screen UI specification, demo mode interactions
- `技术文档.md` — Complete technical reference
- `按键交互设计.md` — Button interaction design rationale
- `../../dongle_firmware/README.md` — Complete Dongle documentation (LED states, USB UAC, troubleshooting)
- `../../dongle_firmware/CLAUDE.md` — Dongle-specific build and architecture notes
- Memory files in `.claude/projects/.../memory/` — Detailed verification logs and troubleshooting history

## Prerequisites and Setup

**Before first build**, verify these — a fresh clone will not compile without them:
- **Missing libraries**: Extract `lib/Arduino_DriveBus` and `lib/ESP32_JPEG` from `../../lib.zip` if the `lib/` folders are empty
- **Git submodules**: LVGL and LovyanGFX are submodules. Run from repository root:
  ```bash
  git submodule update --init --recursive
  ```

## Documentation Languages

Most repository docs are in Chinese (中文); this CLAUDE.md is English for Claude Code. Reply, and write design/requirements docs, in Chinese to match the user's language unless asked otherwise.

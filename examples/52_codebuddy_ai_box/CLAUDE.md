# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**CodeBuddy AI BOX (ATK ESP32-S3 BOX 移植版)** — This is a hardware migration of the `51_mic_wifi` project from DFRobot K10 to the 正点原子 (Alientek) ATK ESP32-S3 BOX development board. The project maintains full feature compatibility while adding touch screen support.

**Current system**: ATK BOX transmitter (this project) + ESP32-S3 Dongle receiver (`../../dongle_firmware/`) = wireless audio input device + HID keyboard with touch UI.

**Hardware platform**: 正点原子 ESP32-S3 BOX (ESP32-S3, 16MB flash, 8MB PSRAM, 240×320 ST7789 8-bit parallel LCD, CHSC5432 capacitive touch, ES8311 audio codec, SD card)

This is a PlatformIO Arduino project in a 40+ example repository.

## Repository Structure Context

This is **one example in a 40+ example repository**:
- **Parent repository root**: `../../` (C:\Users\4090\Desktop\dfk10_arduino_demo-master)
- **This project location**: `examples/52_codebuddy_ai_box/` (note: directory name changed from `52_code_buddy_AI BOX`)
- **Source origin**: Migrated from `examples/51_mic_wifi/` (K10 version)
- **Companion Dongle**: `../../dongle_firmware/` (separate ESP-IDF project, not PlatformIO)
- **PlatformIO config**: `../../platformio.ini` contains `[env:52_codebuddy_ai_box]`

## Build and Development Commands

**Important**: Use `python -m platformio` (the `pio`/`platformio` commands are not in PATH).

```bash
# Work from repository root for PlatformIO commands
cd C:\Users\4090\Desktop\dfk10_arduino_demo-master

# Compile this project
python -m platformio run -e 52_codebuddy_ai_box

# Flash to ATK BOX (COM11 is typical, adjust as needed)
python -m platformio run -t upload -e 52_codebuddy_ai_box --upload-port COM11

# Serial monitor (device outputs on COM12 when COM11 is used for upload)
python -m platformio device monitor -p COM12 -b 115200

# Clean rebuild (when switching between major changes)
python -m platformio run -t clean -e 52_codebuddy_ai_box
python -m platformio run -e 52_codebuddy_ai_box

# Compiled firmware location
.pio\build\52_codebuddy_ai_box\firmware.bin

# Dongle (ESP-IDF project, NOT PlatformIO)
cd ../../dongle_firmware
idf.py build
idf.py -p COM6 flash monitor   # COM6 is typical for Dongle

# Initialize git submodules (required before first build)
cd C:\Users\4090\Desktop\dfk10_arduino_demo-master
git submodule update --init --recursive
```

**Typical COM ports**:
- ATK BOX upload: COM11
- ATK BOX serial output: COM12
- Dongle: COM6

## Architecture Overview

### Current System (ATK BOX + Dongle)

**Data flow**: `ATK BOX (this project) ←→ [ESP-NOW 2.4GHz] ←→ Dongle ←→ [USB] ←→ PC`

**ATK BOX Transmitter**:
- I2S microphone (ES8311) → ESP-NOW audio streaming (16kHz mono)
- 4 buttons (A/B/BOOT + RST) → USB HID keyboard
- 7 LVGL UI screens (page navigation via Button B only — see note below)
- CHSC5432 capacitive touch for in-screen taps (screen switching by swipe was REMOVED)
- SD card image display (240×320 PNG from `D:/user.png`, `D:/ai.png`)
- Receives status frames from PC via Dongle (future)

**Dongle Receiver**:
- USB composite device: HID keyboard + USB Audio (UAC 1.0, 16kHz mono)
- Forwards ATK BOX key events and audio to PC
- (Future) Forwards PC status data to ATK BOX

**Protocol**: `espnow_protocol.h` (shared between ATK BOX and Dongle, must stay in sync)
- Frame types: 0x01 Key, 0x02 Audio, 0x04 Pair, 0x06 Heartbeat, 0x07 Token Status, 0x08 Project Status, 0x09 AI Emotion, 0x0A Decision Request, 0x0B Decision Reply
- CRC8 validation (polynomial 0x07)
- ESP-NOW config: Channel 1, ATK BOX MAC (varies), Dongle MAC `e0:72:a1:d4:8f:e0`
- **Decision interaction** (0x0A/0x0B): PC sends a decision request (title + up to 4 options, `decision_request_frame_t`, 134 bytes) → ATK BOX shows Screen 7 decision UI → user taps an option → ATK BOX replies with chosen index (`decision_reply_frame_t`, 5 bytes). PC-side tools in `pc_tools/`.

## Hardware-Specific: ATK BOX vs K10

### Key Differences (Migration Guide)

| Component | K10 (Original) | ATK BOX (This Project) | Adaptation |
|-----------|---------------|------------------------|------------|
| **LCD** | ILI9341 SPI | ST7789 8-bit parallel | `AtkBoxBoard.h` LovyanGFX driver |
| **Touch** | None | CHSC5432 I2C capacitive | `AtkBoxTouch.h` new driver |
| **Audio** | ES7243E (mic only) | ES8311 (mic + speaker) | Both 16kHz I2S |
| **Buttons** | A/B direct GPIO | KEY0/KEY1 via XL9555 + BOOT(GPIO0) | Compatibility macros |
| **IO Expander** | XL95x5 | XL9555 16-bit | `AtkBoxXL9555.h` |
| **Beeper** | None | Via XL9555 P0.2 | Silenced in init (set as input) |
| **Backlight** | Direct PWM | Via XL9555 P0.7 | Controlled by IO expander |
| **SD Card** | SPI shared with LCD | SPI on separate FSPI bus | No bus conflict |

### Button Mapping (4 Usable Buttons)

**ATK BOX has 4 physical buttons, but only 3 are software-readable**:

| Button | Hardware Connection | Software Readable | Function |
|--------|-------------------|-------------------|----------|
| **KEY1 (A)** | XL9555 P0.3 | ✅ Yes | Short=F2+audio, Long=Enter |
| **KEY0 (B)** | XL9555 P0.4 | ✅ Yes | Short=next screen, Long=Backspace |
| **BOOT (C)** | GPIO0 (ESP32) | ✅ Yes | Short=return to main (screen 5), Long=jump to TouchTest |
| **RST** | EN pin (ESP32) | ❌ No (hardware reset only) | Resets device |

> **用户口径按键编号对照**: K2 = 按键 A (KEY1, P0.3)，K1 = 按键 B (KEY0, P0.4)，K0 = BOOT (GPIO0)。
>
> **2026-08-31 交互定稿**: 翻页只由 **K1(B) 短按** 承担（触摸滑动翻页已移除）；**Enter 确认**在 **K2(A) 长按**（原为 Esc，已取消 Esc）。此前代码里 B 短按是 Enter、翻页仅靠触摸滑动，是 bug，现已对齐本表。A 短按仍为 F2+录音；B 长按连续 Backspace；K0 短按回主界面、长按跳 TouchTest。三键均经串口日志验证工作正常。
>
> **⚠️ dongle_mac 陷阱**: `51_mic_wifi.ino` 顶部 `dongle_mac` 必须是 Dongle 实际 MAC `e0:72:a1:d4:8f:e0`。曾被误改成 `AC:A7:04:EF:E7:48`，导致 ESP-NOW 发送大面积失败（串口 `FAIL≫TX`），电脑收不到任何按键。换新 Dongle 时须同步更新此值。

**Key implementation macros** (`51_mic_wifi.ino`):
```c
#define digital_read_key_a()    (xl9555.key1Pressed() ? 0 : 1)
#define digital_read_key_b()    (xl9555.key0Pressed() ? 0 : 1)
#define digital_read_key_boot()  digitalRead(0)  // GPIO0, press=0
```

**Important**: BOOT button has dual purpose:
- **During boot/reset**: If pressed, enters download mode (blocks app)
- **During runtime**: Works as normal function key
- Avoid holding BOOT when powering on or resetting

### Demo Mode Button Interactions (演示模式专属)

Enter demo mode: **hold A+B together for 2 seconds** (injects mock data into all screens).

| Screen | Button A 短按 | Button A 长按 |
|--------|---------------|---------------|
| 1 - Token Usage | 循环高亮下一行 | 重置所有进度条到 10% |
| 3 - Product Inspo | 重启打字机动画 | （保留为 Enter） |
| 4 - User Profile | 切换用户照片 | （保留为 Enter） |

详见: `按键交互设计.md`, `状态显示功能.md`

## Touch Interaction (ATK BOX 新增)

The CHSC5432 capacitive touch enables gesture navigation alongside physical buttons.

### Touch Calibration (关键)

Touch coordinates are calibrated via macros in `AtkBoxTouch.h`:
```c
#define TOUCH_SWAP_XY   0   // ATK BOX verified: 0 (do NOT swap)
#define TOUCH_FLIP_X    0   // X mirror
#define TOUCH_FLIP_Y    0   // Y mirror
```

**Calibration procedure** (if touch direction is wrong):
1. Flash firmware, navigate to TouchTest screen (screen 6)
2. Draw on screen, observe if green dot follows finger
3. If X/Y swapped → set `TOUCH_SWAP_XY = 1`
4. If left/right mirrored → set `TOUCH_FLIP_X = 1`
5. If up/down mirrored → set `TOUCH_FLIP_Y = 1`

### Screen Navigation (界面切换) — 按键翻页, 无触摸滑动

**Page navigation is by physical button only.** Press **Button B (K1)** short-press to advance
to the next screen; there is no touch-based page switching.

History (2026-08-31): swipe-to-switch-screen was implemented and then **removed** on user
request because the capacitive touch was too insensitive for reliable horizontal swipes.
An earlier double-tap approach was also tried (<10% success). Touch is now used ONLY for
in-screen interactions (detail cards, AI-emotion tap, TouchTest drawing), handled by the
LVGL input driver — not for paging. Do NOT re-add swipe navigation without asking.

### TouchTest Screen (调试工具, Screen 6)

A dedicated diagnostic screen showing:
- Real-time touch coordinates (X, Y)
- Green dot following finger position
- "Released" status when finger lifts

Use this to verify touch hardware and calibrate coordinate mapping. Accessible via:
- Button B (K1) short-press cycling (part of 7-screen cycle)
- BOOT button long-press (jumps directly to it)

## UI Screens

**Screen order** (7 screens, cycle via Button B short-press, defined in `SCREEN_ORDER[7]` array):
`{5, 2, 1, 3, 4, 6, 0}` = AI → Project → Token → Inspo → Profile → TouchTest → Status

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
6. **Screen 6 - TouchTest**: Touch diagnostic (real-time coords + green dot follows finger). ATK BOX addition. Reachable via Button B cycling or BOOT long-press.
7. **Screen 0 - ESP-NOW Status**: Connection stats, packet counts

**Screen 7 - Decision UI** (not in the cycle): Temporary overlay shown when a Decision Request (0x0A) arrives from PC. Displays title + up to 4 tappable options; touching one sends a Decision Reply (0x0B) and returns to the previous screen.

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

**Centralized switching** (7 screens including TouchTest):
```c
static const uint8_t SCREEN_ORDER[7] = {5, 2, 1, 3, 4, 6, 0};  // AI→Project→Token→Inspo→Profile→TouchTest→Status
static uint8_t order_index = 0;

// Button B short-press handler (touch swipe paging removed)
order_index = (order_index + 1) % 7;
switch_screen(SCREEN_ORDER[order_index]);
```

`switch_screen()` maps `current_screen` values 0-6 to `lv_scr_load()` calls. Screen 6 (TouchTest) is the ATK BOX addition.

## Key Files and Their Roles

### Core Source Files

- **51_mic_wifi.ino** — Main logic: ESP-NOW, buttons (A/B/BOOT), screen management (7 screens, Button B paging), touch tap handling, animation state machine
- **AtkBoxBoard.h / .cpp** — ATK BOX board support package (replaces K10's `initBoard`): init sequence, backlight, beeper silencing
- **AtkBoxTouch.h** — CHSC5432 capacitive touch driver: I2C 32-bit register protocol, reset control (via XL9555 P0.6), coordinate calibration macros
- **AtkBoxXL9555.h** — XL9555 16-bit IO expander: buttons KEY0/KEY1, backlight P0.7, beeper P0.2, touch reset P0.6, speaker P0.5
- **espnow_protocol.h** — Shared protocol definitions (must sync with Dongle's copy in `../../dongle_firmware/main/`)
- **UNIHIKER_K10_PIN.h** — K10 compatibility pin mapping layer

### Planning / Progress Documents (Chinese)

- **ATK_BOX_移植进度.md** — Migration progress report: completed stages, verification checklist, known issues, calibration procedures
- **移植计划_ATK_ESP32S3_BOX.md** — Original 6-stage migration plan with hardware difference tables
- **小智AI集成技术方案.md** — Xiaozhi AI cloud integration plan (future work)
- **上位机开发文档.md** — PC app integration guide (frame formats, CRC8, Python examples)
- **按键交互设计.md** — Button interaction design rationale
- **状态显示功能.md** — UI specification, demo mode interactions, real data protocol

### Helper Scripts

- **check_sd_boot.py** — Capture boot logs via serial, verify SD card init
- **monitor_continuous.py** — Continuous serial monitoring
- **test_k10_button.py** — Button interaction testing utility

## Hardware-Specific Constraints

**ATK ESP32-S3 BOX**:
- Flash: ~24% used (1.1MB / 4.7MB)
- PSRAM: 8MB, required for LVGL frame buffers and image cache
- LCD: ST7789, 240×320, 8-bit parallel, DMA transfers (LovyanGFX `Bus_Parallel8`)
- Touch: CHSC5432, I2C addr 0x2E, chip ID 0x05030100
- Audio: ES8311 (mic + speaker), I2C addr 0x18, 16kHz mono I2S
- IO Expander: XL9555, I2C addr 0x20
- SD card: SPI mode on FSPI bus (separate from LCD parallel bus)
- I2C bus (SDA/SCL): shared by XL9555 (0x20), ES8311 (0x18), CHSC5432 (0x2E), and others (0x1E, 0x50, 0x6A, 0x7E)

**ESP-NOW limits**:
- Max payload: 250 bytes
- Reliable delivery: ATK BOX tracks `sendFailCount`; Dongle has no retry

**Typical ports**: ATK BOX upload COM11, serial output COM12, Dongle COM6

## Common Pitfalls and Solutions

1. **⚠️ Editing wrong directory**: The compile directory is `examples/52_codebuddy_ai_box/` (underscores). An older `examples/52_code_buddy_AI BOX/` (spaces) may still exist — edits there are NOT compiled. Always verify `src_dir = examples/${platformio.default_envs}` resolves to the underscore directory.

2. **Touch direction wrong**: Adjust `TOUCH_SWAP_XY`/`FLIP_X`/`FLIP_Y` in `AtkBoxTouch.h`. Use TouchTest screen (screen 6) to verify. ATK BOX verified value: all 0.

3. **Beeper won't stop on boot**: ATK beeper is on XL9555 P0.2, active on power-up. Set P0.2 as **input mode** (high-Z) in `AtkBoxXL9555` init to silence.

4. **Touch not responding after boot**: CHSC5432 reset is via XL9555 P0.6. Ensure reset sequence (low→delay→high) runs after XL9555 init. Reads returning all `0xFF` indicate I2C failure or chip held in reset.

5. **BOOT button enters download mode**: GPIO0 (BOOT) is dual-purpose. Never hold it during power-on/reset, only press during runtime.

6. **Paging is button-only (no touch swipe)**: Screen switching uses Button B (K1) short-press. Touch swipe navigation was removed (2026-08-31) because the capacitive touch was too insensitive; double-tap was also rejected (<10% success). Touch now serves only in-screen taps. Do NOT re-add swipe paging without asking.

7. **LVGL API version mismatch**: This project uses **LVGL v8**. Do NOT use v9 APIs like `lv_draw_buf_t`, `LV_COLOR_FORMAT_RGB565`, `lv_point_precise_t`, or 4-arg `lv_canvas_set_px`.

8. **Screen won't transition**: Check `LV_IMG_CACHE_DEF_SIZE` is 4 (not 0). Without caching, PNG re-decodes each frame (100+ms), causing button timing issues.

9. **SD card mount fails**: Format as FAT32. Windows Quick Format on 64GB+ defaults to exFAT.

10. **Protocol mismatch between ATK BOX and Dongle**: After editing `espnow_protocol.h` on one side, immediately copy to the other (must be byte-identical).

11. **LVGL threading**: All LVGL calls (except in `lv_port_*` drivers) must hold `xGuiSemaphore`. Missing lock → screen freezes, button timing errors.

## Testing the System

**Audio path**:
1. Flash Dongle, connect to PC
2. Flash ATK BOX (COM11), serial monitor on COM12
3. Short-press button A → LED on Dongle blinks → PC recording device shows level
4. Verify latency <50ms (speak into ATK BOX mic, listen via PC app)

**Keyboard path**:
1. Open Windows Explorer, select a file
2. Short-press A → File enters rename mode (F2 key received)
3. Long-press B → Characters delete continuously (Backspace repeat)
4. Short-press B → Next screen (short press doesn't send keys unless in demo mode)

**Screen navigation** (button-only, no touch swipe):
1. Boot ATK BOX → Default screen 5 (AI Status)
2. Short-press B → next screen (5→2→1→3→4→6→0→…)
3. Long-press BOOT → Jump to screen 6 (TouchTest)

**Touch interaction** (in-screen taps only):
1. On TouchTest screen, draw and verify green dot follows finger
2. Tap AI-emotion / detail-card elements to trigger their in-screen actions
   (touch does NOT switch pages)

**Status display**:
1. Press A+B for 2s → All screens populate with demo data
2. Press B repeatedly → Cycle through 7 screens: 5→2→1→3→4→6→0
3. Screen 5: Observe cloud face auto-cycling Thinking→Coding→Done every 4s with blinking eyes
4. Screen 1 (in demo mode): Press A repeatedly to highlight next model row
5. Screen 4 (in demo mode): Press A repeatedly to cycle user portraits

**Touch calibration verification**:
1. Navigate to screen 6 (TouchTest)
2. Touch top-left corner → verify coordinates near (0, 0)
3. Touch bottom-right → verify near (239, 319)
4. If wrong: adjust `TOUCH_SWAP_XY`/`FLIP_X`/`FLIP_Y` in `AtkBoxTouch.h`

**Debugging**:
- ATK BOX serial (COM12): `python monitor_continuous.py` or `python -m platformio device monitor -p COM12`
- Dongle serial: `idf.py -p COM6 monitor`
- Look for: "ESP-NOW init OK", "SD mounted", "Touch scan ready", "Switched to screen N", "AI state received"

## Related Documentation

**ATK BOX Migration**:
- `ATK_BOX_移植进度.md` — Migration progress: completed stages, verification results, calibration values
- `移植计划_ATK_ESP32S3_BOX.md` — Original 6-stage plan with hardware comparison tables

**Wireless System**:
- `按键交互设计.md` — Button interaction rationale (4 gestures covering full voice input workflow)
- `状态显示功能.md` — 7-screen UI specification, demo mode, real data protocol
- `上位机开发文档.md` — PC app integration guide (frame formats, CRC8, Python examples)

**Future Work**:
- `小智AI集成技术方案.md` — 6-day implementation plan for WiFi + Xiaozhi AI cloud integration

**Repository-Level**:
- `../../dongle_firmware/README.md` — Complete Dongle documentation (LED states, USB UAC, troubleshooting)
- `../../dongle_firmware/CLAUDE.md` — Dongle-specific build and architecture notes
- `../../CLAUDE.md` — Repository root guidance (multi-example structure, build system, prerequisites)
- Memory files in `../../.claude/projects/.../memory/` — Verification logs, troubleshooting history

## Migration Notes (ATK BOX Specific)

This project was migrated from `51_mic_wifi` (K10 version) to ATK ESP32-S3 BOX in 2026-08-27. Key adaptations:

1. **Display driver**: Replaced `Bus_SPI` (K10 ILI9341) with `Bus_Parallel8` (ATK ST7789) in LovyanGFX config
2. **Touch support**: New `AtkBoxTouch.h` driver for CHSC5432 capacitive touch, replacing K10's no-touch design
3. **Button layer**: K10 used direct GPIO (`digitalRead`), ATK uses XL9555 IO expander (`xl9555.key0Pressed()`) with compatibility macros
4. **Audio codec**: Both use I2S 16kHz, but ATK has ES8311 (mic+speaker) vs K10's ES7243E (mic only)
5. **Beeper handling**: ATK BOX has active beeper on XL9555 P0.2, silenced by setting as input
6. **7th screen**: Added TouchTest screen (screen 6) for touch hardware verification
7. **BOOT button**: Added as 3rd function key (short=return to main / long=jump to TouchTest)

**Verified working**:
- ✅ Display (ST7789 parallel, 240×320)
- ✅ Touch (CHSC5432, in-screen taps; swipe paging removed 2026-08-31)
- ✅ Buttons (KEY0/KEY1 via XL9555, BOOT via GPIO0)
- ✅ Audio (ES8311 mic, I2S recording)
- ✅ SD card (user portraits, AI cloud image)
- ✅ ESP-NOW (audio streaming, keyboard HID)
- ✅ 7-screen UI with animations

**Known issues**: None — all features working as of 2026-08-27.

## Prerequisites and Setup

**Before first build**, verify these — a fresh clone will not compile without them:
- **Missing libraries**: Extract `lib/Arduino_DriveBus` and `lib/ESP32_JPEG` from `../../lib.zip` if the `lib/` folders are empty
- **Git submodules**: LVGL and LovyanGFX are submodules. Run from repository root:
  ```bash
  git submodule update --init --recursive
  ```

## Documentation Languages

Most repository docs are in Chinese (中文); this CLAUDE.md is English for Claude Code. Reply, and write design/requirements docs, in Chinese to match the user's language unless asked otherwise.

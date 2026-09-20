# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a PlatformIO Arduino project for the DFRobot UNIHIKER K10 board (ESP32-S3, 16MB flash, PSRAM, 240x320 ILI9341 SPI LCD, onboard SD card). The repository contains multiple example demonstrations, with the primary project being **CodeBuddy Wireless** — a wireless keyboard + audio streaming system with rich status displays.

## Repository Structure

This is a **multi-example repository** with 40+ example projects for the K10 board, not a single-purpose project:
- **Primary project**: `examples/51_mic_wifi` (CodeBuddy Wireless) — documented in detail below
- **Other examples**: Display drivers (01–12), sensors (20–22), music/audio (23), games (40–41), LVGL UI demos (30–32, 42–44)
- **Companion firmware**: `dongle_firmware/` — separate ESP-IDF project (not PlatformIO) for the USB dongle
- **Web flasher**: `web_flasher/` — browser-based firmware flasher (Web Serial + esptool-js), multi-product support
- **Build system**: PlatformIO with environment switching via `platformio.ini`

The active example is selected by `default_envs` in `platformio.ini`. `src_dir` is derived from it (`src_dir = examples/${platformio.default_envs}`), so changing `default_envs` changes which example compiles. All envs inherit the base `[env]` config (PSRAM, USB CDC on boot, 16MB partitions).

## Prerequisites and Setup

**Before first build**, verify these — a fresh clone will not compile without them:
- **Missing libraries**: The root `README.md` notes that `lib/Arduino_DriveBus` and `lib/ESP32_JPEG` may be empty after clone. Extract them from `lib.zip` in the repository root if present.
- **Git submodules**: LVGL and LovyanGFX are submodules. Run:
  ```bash
  git submodule update --init --recursive
  ```

## Documentation Languages

Most repository docs are in Chinese (中文); this CLAUDE.md is English for Claude Code. Reply, and write design/requirements docs, in Chinese to match the user's language unless asked otherwise.
- `README.md` (root): Library setup notes
- `examples/51_mic_wifi/上位机开发文档.md`: PC app protocol spec (frame formats, CRC8, Python examples)
- `examples/51_mic_wifi/按键交互设计.md`: Button interaction design rationale
- `dongle_firmware/README.md`: Complete dongle documentation (LED states, USB UAC, troubleshooting)
- `web_flasher/README.md`: Web flasher usage (browser requirements, updating firmware)
- `web_flasher/EXTEND.md`: Guide for adding new products to the flasher platform

## Build System and Development Commands

The project uses PlatformIO with **40+ example environments**. **Important**: Use `python -m platformio` (the `pio`/`platformio` commands are not in PATH).

```bash
# K10 (examples/51_mic_wifi) - Main CodeBuddy project
python -m platformio run -e 51_mic_wifi                    # compile
python -m platformio run -t upload -e 51_mic_wifi --upload-port COM4  # flash
python -m platformio device monitor -p COM4 -b 115200      # serial monitor

# Build a different example without editing platformio.ini
python -m platformio run -e 30_lvgl_Gif                    # LVGL GIF demo
python -m platformio run -e 40_Snake_Game                  # Snake game
# NOTE: -e sets the env, but src_dir follows default_envs in platformio.ini.
# To actually compile a different example's source, also set default_envs (below).

# Dongle (dongle_firmware) - ESP-IDF project (NOT PlatformIO)
cd dongle_firmware
idf.py build
idf.py -p COM7 flash monitor
idf.py -p COM7 flash  # flash only

# Web-based flasher (no toolchain required, Chrome/Edge desktop only)
cd web_flasher
python serve.py  # Opens http://localhost:8000
# Select target (K10 or Dongle), connect device, click "开始写入"
# See web_flasher/README.md for details

# Initialize git submodules (lvgl and LovyanGFX)
git submodule update --init --recursive

# Change active example in K10:
# Edit platformio.ini and set default_envs = 51_mic_wifi (or other env)
```

**Special dependency note**: `32_DEMO_MJPEG` requires the `ESP32_JPEG` library (extract from `lib.zip` if the `lib/` folder is empty); its env adds custom `build_flags` linking `libesp_codec.a`.

## Architecture Overview

### CodeBuddy Wireless System (K10 + Dongle)

**Data flow**: `K10 (51_mic_wifi) ←→ [ESP-NOW 2.4GHz] ←→ Dongle (dongle_firmware) ←→ [USB] ←→ PC`

**Two endpoints**:
1. **K10 Transmitter** (`examples/51_mic_wifi/`)
   - Arduino (PlatformIO)
   - I2S microphone (ES7243E) → ESP-NOW audio streaming
   - 2 buttons (A/B) → USB HID keyboard (F2/Enter/Esc/Backspace)
   - 6 LVGL UI screens (ESP-NOW status, Token usage, Coding status, Inspo, User profile, AI Status with animated cloud face)
   - SD card image display (240×320 PNG)
   - Receives status frames from PC via Dongle
   
2. **Dongle Receiver** (`dongle_firmware/`)
   - ESP-IDF
   - USB composite device: HID keyboard + USB Audio (UAC 1.0, 16kHz mono)
   - Forwards K10 key events and audio to PC
   - (Future) Forwards PC status data to K10

**Protocol**: `espnow_protocol.h` (shared between K10 and Dongle, must stay in sync)
- Frame types: 0x01 Key, 0x02 Audio, 0x04 Pair, 0x06 Heartbeat, 0x07 Token Status, 0x08 Project Status, 0x09 AI Emotion
- CRC8 validation (polynomial 0x07)
- ESP-NOW config: Channel 1, K10 MAC `3c:dc:75:6d:7f:b4`, Dongle MAC `e0:72:a1:d4:8f:e0`

### K10 Button Interaction (51_mic_wifi)

**基础交互** (所有模式)：
| Button | Short Press (<600ms) | Long Press (≥600ms) |
|--------|---------------------|---------------------|
| A | F2 + toggle audio streaming | Enter (confirm) |
| B | Cycle UI screens | Backspace (repeats every 100ms) |
| A+B | Hold 2s = Demo mode (injects mock data into all 6 screens) | |

**演示模式专属交互** (进入 Demo Mode 后生效)：
| Screen | Button A 短按 | Button A 长按 |
|--------|---------------|---------------|
| 1 - Token Usage | 循环高亮下一行（高亮行背景变浅灰） | 重置所有进度条到 10% |
| 3 - Product Inspo | 重启打字机动画（清空 → 逐字显示） | （保留为 Enter） |
| 4 - User Profile | 切换用户照片（SD 卡或 fallback 渐变） | （保留为 Enter） |

### K10 UI Screens (Press B to cycle)

**Screen order** (customizable via `SCREEN_ORDER[]` array in 51_mic_wifi.ino):
1. **Screen 5 - AI Status** (boot default): Animated cloud with emotional expressions
   - Three emotions: Thinking (raised brows, closed mouth), Coding (furrowed brows, talking), Done (relaxed brows, smile)
   - Auto-cycles every 4s; or driven by PC via AI Emotion frame (0x09)
   - Eyes blink every 3s, mouth animates during Coding state
   - Displays status text (default or custom from PC)
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
- `AI_STATE_THINKING` (0): Raised brows (`FACE_BROW_Y - 4`), small closed mouth, "Thinking..."
- `AI_STATE_CODING` (1): Lowered brows (`FACE_BROW_Y + 5`), animated mouth (opens/closes every 250ms), "Coding..."
- `AI_STATE_DONE` (2): Neutral brows, wide smile (26×14px), "Done!"

**Behavior**:
- **Local mode** (default): Auto-cycles states every 4s (`AI_STATE_DURATION_MS`)
- **External mode**: Receives AI Emotion frames (0x09) from PC; overrides local cycling; reverts to local after 15s timeout

**Implementation** (`apply_ai_state()`):
- Eyebrows: Positioned via `lv_obj_align()` with Y offsets
- Mouth: Resized via `lv_obj_set_size()` for different shapes
- Text: Custom from PC or default per state
- Eyes: Blink animation (every 3s) independent of state

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

### PC-to-K10 Status Protocol (Future Upper Computer)

See `examples/51_mic_wifi/上位机开发文档.md` for full specification.

**Frame types** (all little-endian, packed structs):
- **0x07 Token Status** (154 bytes): Up to 5 AI services with name (20 chars), used/total tokens, percentage×10
- **0x08 Project Status** (154 bytes): Up to 6 projects with name (24 chars), status code (0=Planning, 1=Coding, 2=Review, 3=Done, 4=Error, 5=Idle)
- **0x09 AI Emotion** (24 bytes): Emotion code (0/1/2), custom text (20 chars), overrides local animation

**Data path**: `PC app → USB serial (115200) → Dongle (串口收→ESP-NOW转发, needs implementation) → K10 (espnow_recv_cb)`

**K10 receiver**: `espnow_recv_cb()` in 51_mic_wifi.ino validates CRC8, copies to globals (`token_data`, `project_data`, `ai_state`), sets `screen_dirty` or `ai_state_changed` flags.

## Key Files and Their Roles

### K10 (examples/51_mic_wifi/)

Full path from repository root: `examples/51_mic_wifi/`

- **51_mic_wifi.ino** — Main logic: ESP-NOW, buttons, screen management, animation state machine
- **espnow_protocol.h** — Shared protocol definitions (must sync with Dongle's copy in `dongle_firmware/main/`)
- **main.h** — Hardware init (I2S, LCD, SD), LVGL display/input drivers, SD filesystem bridge
- **UNIHIKER_K10_PIN.h** — GPIO pin mapping
- **上位机开发文档.md** — Protocol spec for PC app developers (Chinese, frame formats, CRC8, Python examples)
- **按键交互设计.md** — Button interaction design doc (Chinese)

**Screen creation pattern**:
```c
static void create_screen_X();   // Creates LVGL objects for screen X
static void update_screen_X();   // Updates data on screen X
```

**Update trigger**: Set `screen_dirty = true` or use 100ms timer (Screen 5 animation).

### Dongle (dongle_firmware/)

Full path from repository root: `dongle_firmware/` — **ESP-IDF project** (not PlatformIO; uses `idf.py`, not `python -m platformio`). See `dongle_firmware/README.md` for complete documentation (Chinese, LED states, USB UAC, troubleshooting).

- **main/main.c** — USB composite device (HID + Audio), ESP-NOW receiver
- **main/espnow_protocol.h** — Protocol (must stay byte-identical with K10's copy in `examples/51_mic_wifi/`)
- **main/espnow_receiver.c/h** — ESP-NOW frame handling
- **main/usb_descriptors.c** — USB device descriptors (HID report + UAC)
- **main/config.h** — MAC addresses, channel, buffer sizes, LED GPIO
- **main/audio_ringbuf.c/h** — Lock-free ring buffer for audio
- **main/led_indicator.c/h** — WS2812 LED status indicator (heartbeat, data flash, error states)
- **LED功能说明.md**, **LED测试验证清单.md** — LED documentation (Chinese)

**USB audio format**: UAC 1.0, 16kHz mono, 16-bit, 32-sample frames (2ms latency target).

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
- Dongle: COM7

## Common Pitfalls and Solutions

1. **Screen 5 won't transition**: Check `LV_IMG_CACHE_DEF_SIZE` is 4 (not 0). Without caching, loop slows to 300+ms causing button timing issues.

2. **SD card mount fails**: Format as FAT32. Windows Quick Format on 64GB+ defaults to exFAT.

3. **Crash in `update_screen_ai_status()`**: Always check `ai_thinking_arc != NULL` before accessing (it's NULL when SD image mode is active).

4. **Protocol mismatch between K10 and Dongle**: After editing `espnow_protocol.h` on one side, **immediately copy to the other** (they must be byte-identical).

5. **LVGL UI glitches**: Ensure all LVGL calls (except in `lv_port_*` drivers) hold `xGuiSemaphore`.

6. **Custom fonts not found**: Enable in `lv_conf.h`: `#define LV_FONT_MONTSERRAT_18 1` (and 20, 28 as needed).

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
- K10 serial: `python -m platformio device monitor -p COM4`
- Dongle serial: `idf.py -p COM7 monitor`
- Look for: "ESP-NOW init OK", "SD mounted", "Switched to screen N", "AI state received"

## Related Documentation

- `examples/51_mic_wifi/上位机开发文档.md` — PC app integration guide (frame formats, CRC8, Python examples)
- `examples/51_mic_wifi/按键交互设计.md` — Button interaction design rationale
- `dongle_firmware/CLAUDE.md` — Dongle-specific build and architecture notes
- Memory files in `.claude/projects/.../memory/` — Detailed verification logs and troubleshooting history

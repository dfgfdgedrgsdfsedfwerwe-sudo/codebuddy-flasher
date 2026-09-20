# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is the **CodeBuddy Dongle** firmware — an ESP-IDF project for ESP32-S3-WROOM-1-N16R8 that acts as a wireless-to-USB bridge. It receives audio and keyboard events from a K10 device over ESP-NOW and presents them to a PC as a USB composite device (HID Keyboard + UAC Microphone).

This is **not** a PlatformIO/Arduino project — it uses ESP-IDF's native CMake build system.

## Build System Commands

```bash
# Set target chip (first time only)
idf.py set-target esp32s3

# Build firmware
idf.py build

# Flash and monitor (replace COM7 with actual port)
idf.py -p COM7 flash monitor

# Flash only
idf.py -p COM7 flash

# Monitor only
idf.py -p COM7 monitor

# Clean build
idf.py fullclean

# Menu config (usually not needed — sdkconfig.defaults covers essentials)
idf.py menuconfig
```

**Exit monitor**: `Ctrl + ]`

## Key Architecture

**Wireless Reception**:
- Uses **ESP-NOW** (not WiFi AP/STA) to receive from K10 device on channel 1 (`ESPNOW_CHANNEL` in `config.h`)
- Frame types (`frame_type_t` in `espnow_protocol.h`): `0x01 KEY` (HID events), `0x02 AUDIO` (PCM16 128-byte chunks), `0x03 FEC` (forward error correction), `0x04 PAIR` / `0x05 PAIR_ACK` (dynamic pairing), `0x06 HEARTBEAT`, `0x07 TOKEN_STATUS`, `0x08 PROJECT_STATUS`, `0x09 AI_STATE` — the last three flow **PC → Dongle → K10** to drive the K10 status screens
- Protocol defined in `main/espnow_protocol.h` — **this file must stay identical between K10 Arduino code and Dongle ESP-IDF code**
- CRC8 validation on every frame; 丢包 detection via sequence numbers
- LED indicator (GPIO 48): visual feedback for system status and data activity

**Dynamic Pairing** (`main/pairing.c/h`) — replaces the old hardcoded `PEER_MAC_ADDR` workflow:
- K10 sends a `PAIR` frame (0x04) on boot; Dongle learns the sender's MAC from `src_mac`, persists it to NVS (namespace `pairing`), and replies with a `PAIR_ACK` (0x05) carrying the Dongle's own MAC
- On subsequent boots `pairing_init()` reloads the paired MAC from NVS — no reflash needed to change devices
- `pairing_clear()` wipes the NVS entry (reserved for a future button/command trigger)
- `PEER_MAC_ADDR` in `config.h` is legacy; the pairing module is the source of truth for the peer MAC at runtime

**Audio Pipeline**:
- ESP-NOW writes 128 bytes every ~4ms (burst arrival) → ring buffer (`audio_ringbuf.c`, `AUDIO_RINGBUF_SIZE` = 64KB / ~4s in `config.h`) → USB reads 32 bytes every 1ms (USB SOF interrupt)
- Prebuffering: accumulates 512 bytes before USB starts reading to prevent initial underrun
- Overrun strategy: drop oldest data (覆盖式) to prioritize low latency over perfect buffering

**USB Composite Device**:
- Built with **TinyUSB** (managed component `espressif/tinyusb`), not `esp_tinyusb` wrapper
- VID:PID = `0x303A:0x8000` (Espressif VID + custom PID)
- Interface 0: HID Keyboard (6-key rollover, sends F2/Enter from K10 buttons)
- Interfaces 1-2: UAC 1.0 Microphone (16kHz mono 16-bit PCM, 32 bytes/ms)
- USB strings: manufacturer `CodeBuddy`, product `CodeBuddy Wireless Dongle` (`config.h`)
- Descriptors in `usb_descriptors.c`; TinyUSB callbacks in `main.c` (`tud_audio_tx_done_pre_load_cb`, `tud_hid_*`)
- **Linker note**: `main/CMakeLists.txt` uses `-u tud_descriptor_*_cb` / `-u tud_hid_descriptor_report_cb` to force retention of the TinyUSB weak-symbol callbacks — without this, `--gc-sections` strips our overrides and the device enumerates with default descriptors

**USB PHY Setup**:
- Uses internal USB OTG PHY (GPIO 19/20), **not** USB-Serial/JTAG
- `usb_new_phy()` must be called before `tusb_init()` to switch from JTAG mode
- Console logs still work via USB-Serial/JTAG (separate peripheral); TinyUSB and logging don't conflict

**FreeRTOS Tasks**:
- `usb_device_task`: polls `tud_task()` every 1ms for USB events
- `stats_task`: prints ESP-NOW/buffer stats every 1s if `DEBUG_STATS=1`
- ESP-NOW reception happens in WiFi ISR → writes directly to ring buffer (no dedicated task)

## File Structure

```
main/
├── main.c                # Entry point: init USB/ESP-NOW, create tasks
├── config.h              # User-editable: channel, MAC address, buffer size
├── espnow_protocol.h     # ⚠️ SHARED with K10 — frame structs, CRC8
├── espnow_receiver.c/h   # ESP-NOW init, frame dispatch, FEC history, stats
├── pairing.c/h           # Dynamic MAC pairing via NVS (PAIR/PAIR_ACK frames)
├── led_indicator.c/h     # WS2812 status LED (GPIO 48)
├── audio_ringbuf.c/h     # Thread-safe ring buffer with prebuffering
├── usb_descriptors.c     # TinyUSB device/config/string/HID descriptors
├── tusb_config.h         # TinyUSB compile-time config (classes, buffer sizes)
├── CMakeLists.txt        # Component registration
└── idf_component.yml     # Dependency: espressif/tinyusb ^0.17.0

CMakeLists.txt            # Project definition, adds main/ to include path
sdkconfig.defaults        # Key settings: USB PHY, WiFi, PSRAM, logging
dependencies.lock         # IDF component manager lock file
```

## Important Configuration

**Editable runtime config** (`main/config.h`):
- `ESPNOW_CHANNEL` (currently 1) — must match K10 device
- `PEER_MAC_ADDR` — legacy hardcoded K10 MAC; superseded at runtime by the `pairing.c` NVS entry (all-zeros = accept any sender in dev mode)
- `AUDIO_RINGBUF_SIZE` (currently 64KB = ~4s audio) — increase if seeing underruns
- `AUDIO_PREBUFFER_BYTES` (default 512 = 16ms) — delay before USB starts reading
- `USB_VID` / `USB_PID` / `USB_MANUFACTURER` / `USB_PRODUCT` / `USB_SERIAL`

**Build-time config** (`sdkconfig.defaults`):
- `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y` — logging via USB-JTAG, not TinyUSB
- `CONFIG_SPIRAM=y` — PSRAM enabled for ring buffer (not currently used; buffer is in DRAM)
- `CONFIG_ESP_WIFI_ENABLED=y` — ESP-NOW requires WiFi driver (but no AP connection)
- `CONFIG_FREERTOS_HZ=1000` — 1ms tick for USB timing

## Protocol Synchronization

**Critical**: `main/espnow_protocol.h` defines frame layouts using `__attribute__((packed))`. Any change to this file requires:
1. Copy the updated file to K10 Arduino project (`examples/5X_*/espnow_protocol.h`)
2. Recompile and reflash **both** K10 and Dongle
3. Verify frame sizes match in serial logs

CRC8 polynomial: `0x07`, init `0x00` — inline implementation in `espnow_protocol.h`

## Flashing and Verification

**Get MAC address**: After first flash, serial monitor prints `Dongle MAC: XX:XX:XX:XX:XX:XX`. With dynamic pairing this is informational — the Dongle learns the K10 MAC automatically from the first `PAIR` frame and persists it to NVS (`pairing` namespace). To force re-pairing, erase NVS (`idf.py -p COM7 erase-flash`) or call `pairing_clear()`.

**PC recognition checklist** (Windows):
1. Device Manager → "Human Interface Devices" → should see HID keyboard (VID_303A&PID_8000)
2. Device Manager → "Audio inputs and outputs" → should see "CodeBuddy Mic"
3. Settings → Sound → Input → select "CodeBuddy Mic" → speak into K10 → level meter should react

If device shows as `VID_303A&PID_1001` (USB-Serial/JTAG), USB PHY init failed — check serial log for errors.

**Useful PowerShell check**:
```powershell
Get-PnpDevice | Where-Object { $_.FriendlyName -like "*CodeBuddy*" }
```

## Debugging

**Serial logs**:
- Tag `espnow_rx`: frame reception, CRC errors, 丢包 detection
- Tag `pairing`: pair frame received, MAC learned, `PAIR_ACK` sent, NVS load on boot
- Tag `main`: init sequence, USB mount/unmount, stats (every 1s if `DEBUG_STATS=1`)
- Enable verbose: `idf.py menuconfig` → Component config → Log output → Default log level → Verbose

**Common issues**:
- **No audio on PC**: K10 isn't sending ESP-NOW frames. Check K10 serial log for "ESP-NOW send" messages. Verify MAC addresses match.
- **USB enumeration fails**: USB PHY not initialized. Verify `usb_new_phy()` succeeds before `tusb_init()`. Check `sdkconfig` has `CONFIG_USB_OTG_SUPPORTED=y`.
- **Audio glitches**: Ring buffer underrun. Increase `AUDIO_RINGBUF_SIZE` or `AUDIO_PREBUFFER_BYTES` in `config.h`.
- **HID keys not working**: K10 not sending key frames, or `on_key_frame()` callback not wired. Check Dongle serial for "Key sent: ..." logs.

**FEC recovery**: Currently incomplete (frames counted but not recovered). XOR recovery logic is stubbed in `handle_fec_frame()` — marked TODO.

## Related Documentation

- `README.md` — overview, hardware specs, protocol summary (Chinese)
- `BUILD.md` — step-by-step build guide
- `FLASH_AND_TEST.md` — flashing, Windows device checks, troubleshooting
- `USB_DEVICE_CHECKLIST.md` — USB enumeration verification steps
- `LED功能说明.md` / `LED测试验证清单.md` — WS2812 LED status indicator (states, test checklist)
- Parent repo `examples/52_codebuddy_ai_box/` — ATK BOX transmitter side (shared `espnow_protocol.h`, full CodeBuddy architecture)

## ESP-IDF Version

Tested with **ESP-IDF 5.4+**. `idf_component.yml` specifies `idf: ">=5.0"`.

Run `. ~/esp/esp-idf/export.sh` (Linux/Mac) or `C:\esp\esp-idf\export.ps1` (Windows) before `idf.py` commands.

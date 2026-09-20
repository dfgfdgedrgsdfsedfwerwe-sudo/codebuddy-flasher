# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

> Scope: this file covers the **`31_png_browser`** example only. See the repository-root `CLAUDE.md` for board hardware, the shared LVGL/BSP patterns, and how examples are selected.

## What this example does

An LVGL-based image browser: recursively scans an SD card for `.png` files, loads them one at a time into a full-screen `lv_img`, and lets you page through them. Two on-screen buttons (Prev/Next) and the two physical buttons (A=prev, B=next) change images.

## Build

This example is **not** the active build. To compile it, edit the repo-root `platformio.ini`: comment out the current `default_envs` line and uncomment `default_envs = 31_png_browser`. Then `pio run` / `pio run -t upload` / `pio device monitor` as usual.

`[env:31_png_browser]` inherits the base `[env]` unchanged — **no ESP32_JPEG needed**. PNG decoding goes through LVGL's built-in PNG decoder, not the ESP32_JPEG hardware codec (that library is only wired into `[env:32_DEMO_MJPEG]`). The root CLAUDE.md's note about "31/32 needing ESP32_JPEG" applies to 32, not this example.

## Two separate storage/SPI buses — do not confuse them

- **LCD** runs on SPI2_HOST (SCLK=12, MOSI=21, DC=13, CS=14) — see root CLAUDE.md's LCD warning.
- **SD card** runs on a **separate HSPI bus**: CS=40, MOSI=42, MISO=41, SCLK=44 (`SD_Card.h`). Uses the Arduino `SD` library at 1 MHz.

`SdCard::init()` **blocks in a `while(1)` retry loop until an SD card mounts** — with no card inserted the sketch hangs in `setup()` before the UI appears. This is intentional but easy to mistake for a crash.

## The "S:" drive — how paths map to the SD card

- `lv_fs_fatfs_init()` registers LVGL's FatFS driver. All image paths use the **`S:` drive letter** (e.g. `S:/folder/pic.png`).
- `find_png_files_recursive("S:")` walks the tree via `lv_fs_*` (LVGL FS), separate from the Arduino `SD` API used in `SdCard`. Both point at the same physical card.
- Directory-vs-file detection is a heuristic: it tries `lv_fs_open` as a file; if that fails it recurses into the path as a directory. Extension match is exact-lowercase `"png"` (uppercase `.PNG` won't match — `my_strcasecmp` exists but is unused here).
- Caps: `MAX_PNG_FILES = 100`, `MAX_PATH_LENGTH = 256`. Files beyond 100 are silently ignored.

## Non-obvious code facts

- **File header comments say `camera.ino` / "display the camera feed"** in both `main.cpp` and `main.md` — a copy-paste artifact. This example has nothing to do with a camera.
- **`main.md`** is a Markdown-wrapped snapshot of an older `main.cpp` (it still contains commented-out ES7243E mic-init code and `pinMode(45)` audio-amp enable). It is documentation only and is **not compiled** — only `.cpp`/`.ino`/`.h` in the folder are built. Edit `main.cpp`, not `main.md`.
- The **auto-advance slideshow timer** (`xAutoSwitchTimer`, `vTimerCallback`) is created but its `xTimerStart` call is commented out, so it never runs. Uncomment the `xTimerStart` block in `setup()` to enable timed cycling.
- `SD_Card.cpp` includes a large generic file-I/O helper class (`listDir`, `readFile`, `Folder_retrieval`, etc.) copied from other examples; only `init()` is actually used by the browser.
- `#include "es7243e.h"` and the I2C mic-init helpers (`i2c_write_reg`) appear in `main.md` but are stripped from the live `main.cpp`.

## Preparing images (png_converter.ipynb)

`png_converter.ipynb` batch-converts source `.jpg` images to `.png` **resized to 240x320** (the panel resolution) for placing on the SD card. Set `input_folder`/`output_folder` before running — the checked-in notebook has placeholder paths that error out as-is.

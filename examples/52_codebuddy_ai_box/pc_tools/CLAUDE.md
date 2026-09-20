# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

**PC Tools for CodeBuddy AI BOX** — Standalone Python utilities for wireless status display and decision interaction with the ATK ESP32-S3 BOX hardware via USB-to-ESP-NOW bridge (Dongle).

**System architecture**: `PC (these tools) → [USB Serial] → Dongle → [ESP-NOW 2.4GHz] → ATK BOX`

This is an **independent tool package** extracted from the larger `examples/52_codebuddy_ai_box/` Arduino project. The tools communicate with already-flashed firmware on the ATK BOX and Dongle.

## Quick Start

```bash
# Real-time status monitoring (Token/Git/AI)
python realtime_monitor.py COM6 C:\path\to\repo 5

# Decision interaction (PC + BOX dual display)
python ask_user_via_box.py COM6 "Choose database" "MySQL" "PostgreSQL" "MongoDB"
```

**Prerequisites**: Python 3.7+, `pyserial` (`pip install pyserial`), ATK BOX + Dongle hardware with firmware flashed.

## Communication Protocol

**Serial framing** (PC ↔ Dongle):
```
A5 5A | len(1B) | payload | outer_crc8
```

**ESP-NOW payload structure** (inside serial payload):
```
[frame_type, count/emotion, seq_num, ...data..., inner_crc8]
```

**Critical protocol details**:
- **Dual CRC layers**: Outer CRC8 validates serial transport, inner CRC8 validates ESP-NOW payload. Both use polynomial 0x07.
- **seq_num field**: All status frames (0x07/0x08/0x09) require 3-byte header `[type, count/emotion, seq_num=0]`. Missing this causes "Frame length mismatch" errors on ATK BOX.
- **Inter-frame delay**: Wait 0.5s between consecutive sends. Dongle's serial→ESP-NOW forwarding has no buffering; rapid sends cause loss.
- **Frame size limits**: Token=154B, Project=154B, AI=24B, Decision request=variable (4+N*32+1), Decision reply=4B.

**Frame types**:
- `0x07` Token Status — Up to 5 AI services with token usage (name, used, total, percent×10)
- `0x08` Project Status — Up to 6 projects (name, status code: 0=Planning 1=Coding 2=Review 3=Done 4=Error 5=Idle)
- `0x09` AI Emotion — Emotion code (0=Thinking 1=Coding 2=Done) + custom text (20 bytes)
- `0x0A` Decision Request — Title + up to 4 options (each 32 bytes), triggers touch interaction on ATK BOX
- `0x0B` Decision Reply — User's choice (0-3) or 0xFF (cancel/timeout), sent from ATK BOX back to PC

## Tool Architecture

### realtime_monitor.py

**Purpose**: Push live Claude Code session data to ATK BOX every N seconds.

**Data sources**:
1. **Token usage** (`get_token_usage()`) — Estimates from `~/.claude/history.jsonl` file size (1KB ≈ 250 tokens)
2. **Git projects** (`get_git_projects(repo_path)`) — Parses `git status --porcelain`, maps file changes to status codes
3. **AI emotion** (`get_ai_emotion()`) — Reads `~/.claude/sessions/{latest}.json` status field, maps `busy→1(Coding)`, `thinking→0(Thinking)`, `idle→2(Done)`

**Key class**: `RealtimeMonitor(port, repo_path, interval=5)`
- Auto-reconnect on serial failure
- Error handling per data source (partial success tolerated)
- Respects 0.5s inter-frame delay

**Configuration**: Pass update interval as 3rd CLI arg (default 5s).

### ask_user_via_box.py

**Purpose**: Display decision options on both PC screen and ATK BOX, wait for touch selection.

**Core function**: `ask_decision(port, title, options, timeout=30)`
- Sends 0x0A frame with title + options to ATK BOX
- ATK BOX shows Screen 7 (decision UI) with touch buttons
- Returns: `0-3` (chosen index), `None` (timeout/cancel)

**Dual display behavior** (PC + BOX):
- PC terminal shows formatted option menu with borders and indices
- ATK BOX touch screen shows same options as interactive buttons
- Both display result after selection (PC prints chosen option, BOX returns to previous screen)

**Encoding note**: Uses ASCII-safe result symbols (`[OK]` / `[TIMEOUT]`) to avoid Windows GBK console errors.

**Usage patterns**:
```python
# Command-line invocation
python ask_user_via_box.py COM6 "Title" "Opt1" "Opt2" "Opt3"

# Python integration
from ask_user_via_box import ask_decision
choice = ask_decision("COM6", "Pick action", ["Continue", "Skip", "Abort"], timeout=20)
```

## Common Issues

**Protocol failures**:
1. **"Stuck in S_CRC state"** (Dongle logs) → Outer CRC missing. Verify `wrap_frame()` appends `crc8(payload)`.
2. **"Frame length mismatch"** (ATK BOX logs) → Missing `seq_num` in frame header. All 0x07/0x08/0x09 frames need 3-byte header `[type, count, 0]`.
3. **Only first frame displays** → No inter-frame delay. Add `time.sleep(0.5)` between sends.

**Data extraction failures**:
- Token: Returns dummy data if `~/.claude/history.jsonl` not found
- Git: Returns "Git error" if not in repo or `git` command unavailable
- AI emotion: Returns "No session" if `~/.claude/sessions/` empty

**Serial port selection**:
- Windows: COM6 typical for Dongle (verify in Device Manager)
- Monitor script targets Dongle's USB serial port (not ATK BOX's COM11/COM12)

## Integration with ATK BOX Firmware

**Firmware location**: `../51_mic_wifi.ino` (Arduino project in parent directory)

**ATK BOX receiver**: `espnow_recv_cb()` validates inner CRC8, copies to globals (`token_data`, `project_data`, `ai_state`), sets `screen_dirty` flags.

**Decision flow**:
1. PC sends 0x0A → Dongle forwards via ESP-NOW → ATK BOX
2. ATK BOX calls `handle_decision_request()` → switches to Screen 7 (decision UI)
3. User taps option → ATK BOX sends 0x0B reply via ESP-NOW
4. Dongle forwards 0x0B to PC serial → `ask_decision()` returns choice index

**Screen states**: Status data updates Screens 1/2/5 (Token/Project/AI), decision interaction uses Screen 7 (temporary overlay, returns to previous screen after selection).

## File Responsibilities

- **realtime_monitor.py** — Status push loop, data extraction, auto-reconnect
- **ask_user_via_box.py** — Decision interaction, dual display, serial protocol
- **README.md** — End-user documentation (quick start, troubleshooting, examples)

## Testing Commands

```bash
# Test monitoring with 10-second updates
python realtime_monitor.py COM6 C:\repo 10

# Test decision with 3 options
python ask_user_via_box.py COM6 "Test" "A" "B" "C"

# Verify protocol with short timeout (5s)
python ask_user_via_box.py COM6 "Quick test" "Yes" "No" --timeout 5
```

## Protocol Reference

**CRC8 implementation** (polynomial 0x07, used by both tools):
```python
def crc8(data):
    crc = 0x00
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc << 1) ^ 0x07 if crc & 0x80 else crc << 1
            crc &= 0xFF
    return crc
```

**Frame construction pattern**:
```python
# Status frames (0x07/0x08/0x09)
payload = bytearray([frame_type, count_or_emotion, 0])  # seq_num=0
payload.extend(...data...)
payload.append(crc8(payload))  # Inner CRC
serial_frame = wrap_frame(payload)  # Adds magic + outer CRC

# wrap_frame() implementation
def wrap_frame(payload):
    frame = bytearray([0xA5, 0x5A, len(payload)])
    frame.extend(payload)
    frame.append(crc8(payload))  # Outer CRC
    return bytes(frame)
```

**Decision request frame structure** (0x0A):
```
[0x0A, option_count, seq_num=0, title(32B), opt0(32B), opt1(32B), ..., inner_crc8]
```

**Decision reply frame structure** (0x0B):
```
[0x0B, chosen_index(0-3 or 0xFF), seq_num=0, inner_crc8]
```

## Related Documentation

- `README.md` — User guide (setup, usage scenarios, troubleshooting)
- `../PC上位机联调进度.md` — Protocol development history, all fixes documented
- `../espnow_protocol.h` — C struct definitions for firmware (must match Python frame layouts)
- `../../../dongle_firmware/README.md` — Dongle firmware documentation (LED states, USB forwarding)

# 项目配置报告

> 由 `/init` 技能自动生成，描述项目的初始化状态与环境配置。
> 重新运行 `/init` 会覆盖本文件。

**生成时间**：2026-09-17 14:30:00  
**生成工具**：init-skill  
**配置版本**：1.0

---

## 1. 项目基本信息

| 项 | 值 |
|----|-----|
| 项目名称 | `51_K10_wifi` (CodeBuddy Wireless) |
| 项目路径 | `examples/51_K10_wifi` |
| 设备型号 | **K10-UNIHIKER** |
| 功能模块 | **ESP-NOW-Audio-HID** |
| 芯片方案商 | **ESP32** |
| 文档版本 | V2.0 |

---

## 2. 工具链

| 项 | 值 |
|----|-----|
| 类型 | PlatformIO + Arduino Framework |
| 平台 | espressif32 |
| 芯片 | ESP32-S3 |
| Flash | 16MB |
| PSRAM | 已启用 (QSPI) |
| USB CDC | 已启用 (on boot) |
| 检测状态 | ✅ 已检测到 |

**PlatformIO 环境配置**:
```ini
[env:51_mic_wifi]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
board_build.flash_size = 16MB
board_build.partitions = default_16MB.csv
build_flags = 
    -DBOARD_HAS_PSRAM
    -DARDUINO_USB_CDC_ON_BOOT=1
```

---

## 3. 构建系统

| 项 | 值 |
|----|-----|
| 类型 | PlatformIO |
| 构建入口 | `platformio.ini` (env: 51_mic_wifi) |
| 编译命令 | `python -m platformio run -e 51_mic_wifi` |
| 烧录命令 | `python -m platformio run -t upload -e 51_mic_wifi --upload-port COM4` |
| 芯片系列 | ESP32-S3 |
| CPU 架构 | Xtensa LX7 (双核) |

---

## 4. 模块架构

### 4.1 核心功能模块

| 模块 | 说明 |
|------|------|
| ESP-NOW 无线通信 | K10 ↔ Dongle 数据传输 (按键/音频/心跳) |
| I2S 音频采集 | ES7243E 麦克风 (16kHz stereo → mono) |
| 按键检测 | 2 按键 (A/B) 短按/长按/组合检测 |
| LVGL UI | 6 个屏幕 (AI 状态/Token/项目/灵感/用户资料/ESP-NOW 统计) |
| SD 卡存储 | FAT32 文件系统 (用户照片/AI 表情图片) |
| LCD 显示 | ILI9341 240×320 SPI (DMA 传输) |

### 4.2 依赖库

| 库 | 版本/来源 | 用途 |
|----|----------|------|
| lvgl | Git 子模块 | UI 框架 |
| LovyanGFX | Git 子模块 | 图形驱动 (LCD/SD) |
| ESP32_JPEG | lib/ | JPEG 解码 |
| Arduino_DriveBus | lib/ | 总线驱动抽象 |
| ArduinoJson | lib/ | JSON 解析 (未来上位机) |
| ESP32-audioI2S | lib/ | I2S 音频流 |

**关键子模块状态**:
- ✅ lvgl 已初始化 (`lib/lvgl/src/` 存在)
- ✅ LovyanGFX 已初始化 (`lib/LovyanGFX/src/` 存在)
- ✅ ESP32_JPEG 已提取 (`lib/ESP32_JPEG/src/` 存在)
- ✅ Arduino_DriveBus 已提取 (`lib/Arduino_DriveBus/src/` 存在)

---

## 5. 依赖检查

### 5.1 已安装依赖

| 依赖 | 类型 | 说明 |
|------|------|------|
| `python` | CLI | Python 3.11+ (PlatformIO 运行环境) |
| `platformio` | Python 包 | 嵌入式开发平台 |
| `esptool` | Python 包 | ESP32 烧录工具 |
| `pyserial` | Python 包 | 串口通信 |
| `git` | CLI | 版本控制 + 子模块管理 |

### 5.2 可选依赖

| 依赖 | 类型 | 用途 | 安装方式 |
|------|------|------|----------|
| `idf.py` | ESP-IDF | Dongle 固件编译 (ESP-IDF 项目) | https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/ |

**说明**: K10 固件 (本项目) 使用 PlatformIO，无需 ESP-IDF。Dongle 固件 (`dongle_firmware/`) 是独立的 ESP-IDF 项目，需单独安装。

---

## 6. 生成的产物

| 产物 | 路径 | 说明 |
|------|------|------|
| 环境初始化脚本 (PowerShell) | `scripts/setup_env.ps1` | Windows 环境安装脚本 |
| 环境初始化脚本 (Bash) | `scripts/setup_env.sh` | Git Bash 环境安装脚本 |
| 开发规范文档 | `docs/spec/K10-UNIHIKER-ESP-NOW-Audio-HID-ESP32-V2.0.md` | 协议规范 (ESP-NOW 帧格式/命令字典) |
| 项目配置报告 | `docs/init/project-config.md` | 本文件 |

---

## 7. 硬件配置

### 7.1 K10 开发板

| 项 | 值 |
|----|-----|
| 芯片 | ESP32-S3-WROOM-1-N16R8 |
| Flash | 16MB |
| PSRAM | 8MB QSPI |
| LCD | 240×320 ILI9341 (SPI) |
| 麦克风 | ES7243E (I2S) |
| SD 卡 | SPI 模式 (共享 LCD 总线) |
| 按键 | 2 个 (A: GPIO 21, B: GPIO 47) |
| USB | Type-C (CDC + UART) |

### 7.2 Dongle 接收器

| 项 | 值 |
|----|-----|
| 芯片 | ESP32-S3 |
| USB | 复合设备 (HID Keyboard + UAC 1.0 Audio) |
| LED | WS2812 × 1 (状态指示) |
| 固件路径 | `dongle_firmware/` (ESP-IDF 项目) |
| 编译命令 | `idf.py build` |
| 烧录命令 | `idf.py -p COM6 flash` |

---

## 8. 协议配置

### 8.1 ESP-NOW 参数

| 参数 | 值 |
|------|-----|
| 信道 | 1 (固定) |
| 加密 | 无 |
| K10 MAC | `3c:dc:75:6d:7f:b4` |
| Dongle MAC | `e0:72:a1:d4:8f:e0` |
| 最大负载 | 250 字节 |

### 8.2 帧类型

| 类型码 | 名称 | 方向 | 说明 |
|--------|------|------|------|
| 0x01 | 按键帧 | K10 → Dongle | HID 键码 (F2/Enter/Backspace) |
| 0x02 | 音频帧 | K10 → Dongle | 60 样本/帧 (16kHz mono) |
| 0x04 | 配对帧 | K10 → Dongle | 启动时单次发送 |
| 0x06 | 心跳帧 | K10 → Dongle | 1 Hz 连接检测 |
| 0x07 | Token 状态 | PC → K10 | 更新屏幕 1 进度条 |
| 0x08 | 项目状态 | PC → K10 | 更新屏幕 2 项目列表 |
| 0x09 | AI 情绪 | PC → K10 | 控制屏幕 5 云脸表情 |

详见: `docs/spec/K10-UNIHIKER-ESP-NOW-Audio-HID-ESP32-V2.0.md`

---

## 9. 参数来源

**自动检测**:
- `toolchain.type` = PlatformIO (从 `platformio.ini` 检测)
- `toolchain.platform` = espressif32 (从 env 配置读取)
- `buildSystem.chipSeries` = ESP32-S3 (从 board 配置读取)
- `libs.status` = 已就绪 (检查 lib/ 目录和子模块)

**用户提供**:
- `project.deviceModel` = K10-UNIHIKER (用户确认)
- `project.module` = ESP-NOW-Audio-HID (用户确认)
- `project.docVersion` = V2.0 (用户确认)

**项目文档推断**:
- `project.name` = 51_K10_wifi / CodeBuddy Wireless (从 CLAUDE.md 提取)
- `hardware.mic` = ES7243E (从源码和文档分析)
- `hardware.lcd` = ILI9341 (从源码和文档分析)

---

## 10. 后续建议

### 10.1 立即执行

1. **运行环境初始化脚本** (选择一个):
   ```powershell
   # Windows PowerShell
   .\scripts\setup_env.ps1
   ```
   ```bash
   # Git Bash
   ./scripts/setup_env.sh
   ```

2. **验证编译**:
   ```bash
   cd /c/Users/4090/Desktop/dfk10_arduino_demo-master
   python -m platformio run -e 51_mic_wifi
   ```

3. **烧录到设备**:
   ```bash
   # 连接 K10 到 USB 端口 (通常是 COM4)
   python -m platformio run -t upload -e 51_mic_wifi --upload-port COM4
   ```

4. **查看串口输出**:
   ```bash
   python -m platformio device monitor -p COM4 -b 115200
   ```

### 10.2 配套 Dongle 固件

**必须同时烧录 Dongle** 才能完整测试系统:
```bash
cd dongle_firmware
idf.py build
idf.py -p COM6 flash monitor
```

### 10.3 SD 卡准备

1. 格式化 SD 卡为 **FAT32** (64GB 卡默认 exFAT 会失败)
2. 准备图片文件 (240×320 PNG):
   - `D:/user.png` — 用户照片 (屏幕 4)
   - `D:/user1.png`, `D:/user2.png` ... — 备用照片 (最多 10 张)
   - `D:/ai.png` — AI 表情云图 (屏幕 5)

### 10.4 下一步技能

- **`/prd`**: 基于本配置生成产品需求文档
- **`/test`**: 建立单元测试基线 (按键/音频/协议)
- **`/fw:build`**: 自动化编译流程 (支持多环境)

---

## 11. 故障排除快速参考

| 问题 | 可能原因 | 解决方案 |
|------|----------|----------|
| 编译失败: "lvgl/lvgl.h not found" | 子模块未初始化 | `git submodule update --init --recursive` |
| 编译失败: "ESP32_JPEG.h not found" | 依赖库缺失 | 解压 `lib.zip` 到 `lib/` 目录 |
| SD 卡挂载失败 | exFAT 格式 | 重新格式化为 FAT32 |
| 屏幕 5 切换卡顿 | 图像缓存未启用 | 已修复: `lv_conf.h` 中 `LV_IMG_CACHE_DEF_SIZE 4` |
| 烧录失败: "no serial port" | 端口号错误 | 检查设备管理器，调整 `--upload-port` 参数 |
| Dongle 无响应 | 固件未烧录 | 烧录 `dongle_firmware/` 并检查 COM 端口 |
| 音频无声 | ESP-NOW 未连接 | 检查 MAC 地址配置和信道 |

完整故障排除文档:
- K10: `examples/51_mic_wifi/CLAUDE.md`
- Dongle: `dongle_firmware/README.md` (LED 状态码表)

---

**初始化状态**: ✅ 完成  
**环境就绪**: ✅ 所有依赖已检测  
**可立即编译**: ✅ (运行 `python -m platformio run -e 51_mic_wifi`)

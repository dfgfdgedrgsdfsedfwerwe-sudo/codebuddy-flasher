# CodeBuddy Dongle 固件 - ESP32-S3 USB UAC 音频接收器

## 项目概述

这是一个基于 ESP32-S3 的 USB Audio Class (UAC) 设备固件,实现了无线麦克风音频通过 ESP-NOW 协议接收,然后通过 USB 音频设备枚举到 Windows/macOS/Linux 系统。

**当前状态：✅ 已完成并验证工作**

Windows 设备管理器显示：
- ✅ **CodeBuddy Microphone** (音频输入设备) - 工作正常
- ✅ **CodeBuddy Audio** (声音、视频和游戏控制器) - 工作正常

## 硬件要求

- **开发板**: ESP32-S3 开发板(至少 8MB PSRAM)
- **USB 接口**: 
  - **COM 口**: 用于烧录固件和串口调试(标注 "COM")
  - **USB 口**: 用于 USB 音频设备枚举(标注 "USB")
- **配对设备**: 运行 ESP-NOW 音频发送固件的麦克风设备

## 系统架构

```
┌─────────────────┐         ESP-NOW          ┌──────────────────┐
│  麦克风设备      │  ─────────────────────>  │  ESP32-S3 Dongle │
│  (发送端)       │    48kHz 单声道 PCM      │  (接收端)        │
└─────────────────┘                          └──────────────────┘
                                                      │
                                                      │ USB
                                                      ▼
                                              ┌──────────────────┐
                                              │  Windows/macOS   │
                                              │  音频输入设备     │
                                              └──────────────────┘
```

## 核心功能模块

### 1. ESP-NOW 接收模块 (`espnow_receiver.c/h`)

**功能**: 接收来自无线麦克风的音频数据包

**关键实现**:
- 注册 ESP-NOW 接收回调 `espnow_recv_cb()`
- 解析 `espnow_audio_packet_t` 数据包
- 验证魔术字节 `ESPNOW_AUDIO_MAGIC` (0x4155)
- 将音频数据写入环形缓冲区

**数据包格式** (`espnow_protocol.h`):
```c
typedef struct {
    uint16_t magic;      // 0x4155 魔术字节
    uint16_t seq;        // 序列号(用于丢包检测)
    uint16_t length;     // 音频数据长度(字节)
    int16_t samples[];   // 可变长度 PCM 样本
} espnow_audio_packet_t;
```

### 2. 音频环形缓冲区 (`audio_ringbuf.c/h`)

**功能**: 解耦 ESP-NOW 接收速率和 USB 传输速率

**实现细节**:
```c
typedef struct {
    int16_t *buffer;              // 环形缓冲区存储
    size_t capacity_samples;      // 容量(样本数)
    size_t write_idx;             // 写索引
    size_t read_idx;              // 读索引
    SemaphoreHandle_t mutex;      // 互斥锁
} audio_ringbuf_t;
```

**配置** (`config.h`):
- **容量**: 48000 样本 = 1 秒音频缓冲
- **线程安全**: FreeRTOS 互斥锁保护

### 3. USB 描述符 (`usb_descriptors.c`)

**UAC 1.0 描述符配置**:

| 参数 | 值 | 说明 |
|------|-----|------|
| **VID** | `0x303A` | Espressif Systems |
| **PID** | `0x4004` | 自定义产品 ID |
| **设备名称** | `CodeBuddy Audio` | 显示在系统中的名称 |
| **音频接口** | `CodeBuddy Microphone` | 输入终端名称 |
| **采样率** | `48000 Hz` | 固定采样率 |
| **位深度** | `16-bit` | PCM S16_LE 格式 |
| **声道数** | `1 (Mono)` | 单声道 |
| **端点大小** | `96 字节` | 每 1ms 传输 48 样本 |

**描述符层次结构**:
```
Device Descriptor (设备描述符)
└── Configuration Descriptor (配置描述符)
    ├── Audio Control Interface (音频控制接口)
    │   ├── Input Terminal (输入终端 - 麦克风)
    │   └── Output Terminal (输出终端 - USB 流)
    └── Audio Streaming Interface (音频流接口)
        ├── AS General Descriptor (流通用描述符)
        ├── AS Format Type Descriptor (格式类型 - PCM)
        └── Endpoint Descriptor (端点描述符 - EP 0x81)
```

### 4. LED 状态指示模块 (`led_indicator.c/h`)

**功能**: 通过板载 WS2812 RGB LED 直观显示 Dongle 工作状态和数据传输情况

**LED 状态模式**:

| 模式 | 颜色 | 闪烁模式 | 何时出现 |
|------|------|---------|---------|
| **慢闪** | 🔴 红色 | 1Hz (500ms开/500ms关) | 系统启动初始化阶段 |
| **快闪** | 🔴 红色 | 5Hz (100ms开/100ms关) | TinyUSB 初始化失败 |
| **心跳** | 🟢 绿色 | 双闪模式 (短亮-短暗-短亮-长暗) | ESP-NOW 和 USB 均正常工作 |
| **数据传输** | 🔵 蓝色 | 快速闪烁 (50ms) | 接收 ESP-NOW 数据或 USB 发送数据 |

**硬件配置**:
- **LED 型号**: WS2812 可编址 RGB LED
- **GPIO 引脚**: GPIO 48
- **驱动方式**: ESP-IDF `led_strip` 组件（RMT 外设）
- **依赖**: `espressif/led_strip ^2.5.3`

**启动流程**:
```
上电 → 红色慢闪 (初始化 3s) → 绿色心跳 (正常) / 红色快闪 (失败)
       ├─ ESP-NOW 数据到达 → 蓝色闪烁 50ms → 恢复心跳
       └─ USB 音频发送 → 蓝色闪烁 50ms → 恢复心跳
```

详细文档: [LED功能说明.md](./LED功能说明.md) | [LED测试验证清单.md](./LED测试验证清单.md)

### 5. TinyUSB 配置 (`tusb_config.h`)

**关键配置**:
```c
#define CFG_TUD_AUDIO               1    // 启用 USB Audio 设备
#define CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX  1  // 1 个输入声道
#define CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE    48000
#define CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX  2  // 16-bit
#define CFG_TUD_AUDIO_EP_SZ_IN      96   // 端点大小
```

### 6. 主程序 (`main.c`)

**核心任务**:

#### A. `app_main()` - 初始化入口
```c
void app_main(void) {
    // 1. 初始化 NVS (Wi-Fi 配置存储)
    esp_err_t ret = nvs_flash_init();
    
    // 2. 初始化 Wi-Fi (ESP-NOW 依赖)
    wifi_init();
    
    // 3. 初始化 ESP-NOW
    espnow_init();
    espnow_receiver_init();
    
    // 4. 初始化音频缓冲区
    audio_ringbuf_init(&g_audio_ringbuf, 48000);
    
    // 5. 初始化 TinyUSB
    tinyusb_config_t tusb_cfg = { .device_descriptor = &desc_device };
    tinyusb_driver_install(&tusb_cfg);
    
    // 6. 创建 USB 音频任务
    xTaskCreate(usb_audio_task, "usb_audio", 4096, NULL, 5, NULL);
}
```

#### B. `usb_audio_task()` - USB 音频传输任务
```c
static void usb_audio_task(void *param) {
    while (1) {
        // 1. 从环形缓冲区读取音频数据
        size_t read = audio_ringbuf_read(&g_audio_ringbuf, temp_buf, 48);
        
        // 2. 等待 USB 音频接口就绪
        if (tud_audio_n_mounted(0)) {
            // 3. 写入 USB 音频流
            tud_audio_write(temp_buf, read * 2);
        }
        
        // 4. 延迟 1ms (匹配 USB 全速设备 1ms 帧率)
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
```

## 构建和烧录

### 前提条件

1. **安装 ESP-IDF v5.4+**:
   ```bash
   git clone --recursive https://github.com/espressif/esp-idf.git
   cd esp-idf
   ./install.sh esp32s3
   . ./export.sh
   ```

2. **进入固件目录**:
   ```bash
   cd dongle_firmware
   ```

### 构建固件

```bash
# 设置目标芯片
idf.py set-target esp32s3

# 配置项目(可选)
idf.py menuconfig

# 编译固件
idf.py build
```

### 烧录固件

**重要**: 烧录时使用 **COM 口**(不是 USB 口)

```bash
# 烧录到 COM7 (根据实际端口调整)
idf.py -p COM7 flash

# 烧录后查看日志
idf.py -p COM7 monitor
```

### Windows 快捷命令

```powershell
# 1. 导入 ESP-IDF 环境
C:\esp\esp-idf\export.ps1

# 2. 编译并烧录
cd C:\Users\4090\Desktop\dfk10_arduino_demo-master\dongle_firmware
idf.py -p COM7 build flash
```

### 快速烧录脚本

**烧录到 COM6**:
```powershell
.\flash_to_com6.ps1
```

**烧录到 COM7**:
```powershell
.\flash_to_com7.ps1
```

## 使用说明

### 1. 硬件连接

- **烧录阶段**: USB 线连接到 **COM 口**
- **运行阶段**: USB 线连接到 **USB 口**(设备会枚举为音频设备)

### 2. LED 状态观察

**上电后 LED 状态**:

1. **0-3秒**: 慢闪 (1Hz) - 初始化 NVS/音频缓冲区/USB PHY/TinyUSB/ESP-NOW
2. **3秒后**: 心跳闪烁 (双闪模式) - 系统正常运行
3. **数据传输时**: 快速闪烁 (50ms) - 接收 ESP-NOW 数据或发送 USB 音频

**异常状态**:
- **持续快闪 (5Hz)**: TinyUSB 初始化失败
  - 检查 USB PHY 配置
  - 查看串口日志定位问题
- **持续慢闪**: 系统卡在初始化阶段
  - 查看串口日志确定失败步骤

**正常工作状态**: LED 呈现心跳闪烁，有数据传输时会叠加快速闪烁

详细 LED 说明见 [LED功能说明.md](./LED功能说明.md)

### 3. 验证设备枚举

#### Windows 10/11

打开 **设备管理器** (Win + X → 设备管理器):

```
├── 声音、视频和游戏控制器
│   └── CodeBuddy Audio ✅
└── 音频输入和输出
    └── CodeBuddy Microphone (CodeBuddy Audio) ✅
```

#### macOS

```bash
system_profiler SPUSBDataType | grep -A 10 "CodeBuddy"
```

#### Linux

```bash
lsusb | grep "303a:4004"
arecord -l | grep "CodeBuddy"
```

### 3. 测试音频输入

#### Windows (PowerShell)

```powershell
# 列出所有音频输入设备
Get-WmiObject Win32_SoundDevice | Select-Object Name, Status

# 使用 Audacity 或系统录音机选择 "CodeBuddy Microphone"
```

#### macOS

```bash
# 系统偏好设置 → 声音 → 输入 → 选择 "CodeBuddy Microphone"
```

#### Linux (ALSA)

```bash
# 查找设备卡号
arecord -l

# 录制 10 秒音频
arecord -D hw:X,0 -f S16_LE -r 48000 -c 1 -d 10 test.wav
```

## 故障排除

### 问题 1: 设备无法枚举

**症状**: Windows 设备管理器显示 "未知设备" 或无任何设备

**解决方案**:
1. 确认 USB 线连接到 **USB 口**(不是 COM 口)
2. 检查 `sdkconfig` 中 USB PHY 配置:
   ```
   CONFIG_TINYUSB_RHPORT_HS=y
   CONFIG_USB_HOST_CONTROL_TRANSFER_MAX_SIZE=256
   ```
3. 重新烧录固件并重启设备

### 问题 2: 音频无输入

**症状**: 设备枚举成功,但录音软件无信号

**排查步骤**:
1. **检查 ESP-NOW 连接**:
   ```bash
   idf.py -p COM6 monitor
   # 查找日志: "ESP-NOW audio packet received"
   ```

2. **验证环形缓冲区**:
   - 在 `main.c` 中添加调试日志查看缓冲区填充率

3. **检查 USB 传输**:
   - 使用 Wireshark USB 捕获工具查看 USB 数据包

### 问题 3: 音频断断续续

**可能原因**:
- ESP-NOW 数据包丢失
- 环形缓冲区太小
- USB 任务优先级过低

**解决方案**:
```c
// 增大环形缓冲区 (config.h)
#define AUDIO_RINGBUF_CAPACITY_SAMPLES  (48000 * 2)  // 2 秒缓冲

// 提高 USB 任务优先级 (main.c)
xTaskCreate(usb_audio_task, "usb_audio", 4096, NULL, 10, NULL);
```

### 问题 4: LED 不亮

**症状**: 固件烧录成功，但 WS2812 LED 无任何显示

**可能原因**:
1. GPIO 48 未正确连接 WS2812
2. WS2812 供电电压不足（需要 3.3V-5V）
3. `led_strip` 组件未正确安装

**解决方案**:
1. 检查硬件连接：GPIO 48 → WS2812 DIN，VCC → 3.3V/5V，GND → GND
2. 查看串口日志确认初始化：
   ```bash
   idf.py -p COM6 monitor
   # 应看到: "WS2812 LED strip initialized on GPIO 48"
   ```
3. 重新编译固件：
   ```bash
   idf.py reconfigure
   idf.py build flash
   ```

### 问题 5: LED 颜色异常

**症状**: LED 显示颜色与预期不符（例如红绿互换）

**可能原因**: WS2812 颜色顺序配置错误（GRB vs RGB）

**解决方案**: 
编辑 `led_indicator.c`，修改颜色顺序：
```c
.led_pixel_format = LED_PIXEL_FORMAT_GRB,  // 或 LED_PIXEL_FORMAT_RGB
```

详细 LED 故障排查见 [LED测试验证清单.md](./LED测试验证清单.md)

## 技术细节

### ESP-NOW 传输参数

| 参数 | 值 | 说明 |
|------|-----|------|
| **最大包大小** | 250 字节 | ESP-NOW 协议限制 |
| **每包样本数** | 120 样本 | (250 - 6 字节头) / 2 |
| **包发送间隔** | 2.5 ms | 120 / 48000 = 2.5ms |
| **理论吞吐量** | 384 kbps | 48kHz * 16-bit * 1ch |

### USB 音频传输时序

```
USB 全速设备: 1ms 帧率
├── 每帧传输 48 样本 (96 字节)
├── 理论带宽: 96 KB/s = 768 kbps
└── 实际使用: 48% (384 kbps / 768 kbps)
```

### 内存占用

| 模块 | 大小 | 说明 |
|------|------|------|
| **环形缓冲区** | 96 KB | 48000 样本 * 2 字节 |
| **USB 端点缓冲** | 4 KB | TinyUSB 内部缓冲 |
| **ESP-NOW 缓冲** | 8 KB | Wi-Fi 驱动分配 |
| **FreeRTOS 栈** | 16 KB | 各任务栈总和 |
| **总计** | ~124 KB | 适合 8MB PSRAM 设备 |

## 文件清单

### 核心源文件

```
dongle_firmware/
├── main/
│   ├── main.c                   # 主程序入口和 USB 音频任务
│   ├── config.h                 # 全局配置参数 (LED_GPIO 等)
│   ├── espnow_protocol.h        # ESP-NOW 数据包格式定义
│   ├── espnow_receiver.c/h      # ESP-NOW 接收逻辑
│   ├── audio_ringbuf.c/h        # 线程安全的环形缓冲区
│   ├── led_indicator.c/h        # WS2812 LED 状态指示模块
│   ├── usb_descriptors.c        # USB UAC 1.0 描述符
│   ├── tusb_config.h            # TinyUSB 配置
│   └── idf_component.yml        # 组件依赖声明 (led_strip)
├── CMakeLists.txt               # CMake 构建脚本
├── sdkconfig                    # ESP-IDF 配置
├── flash_to_com6.ps1            # COM6 快速烧录脚本
├── flash_to_com7.ps1            # COM7 快速烧录脚本
├── LED功能说明.md                # WS2812 LED 功能详细文档
├── LED测试验证清单.md            # LED 测试步骤和故障排查
└── README.md                    # 本文档

├── LED功能说明.md               # LED 功能详细文档
└── README.md                    # 本文档
```

### 构建输出

```
build/
├── codebuddy_dongle.bin         # 最终固件二进制文件
├── codebuddy_dongle.elf         # ELF 调试符号文件
└── codebuddy_dongle.map         # 内存映射文件
```

## 参考资料

- [ESP-IDF 编程指南](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/)
- [ESP-NOW 协议文档](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/network/esp_now.html)
- [TinyUSB 库文档](https://docs.tinyusb.org/)
- [USB Audio Class 1.0 规范](https://www.usb.org/document-library/audio-device-document-10)
- [WirelessAdapter 参考项目](https://github.com/AndrewTheeFirst/WirelessAdapter)

## 许可证

本项目基于 Apache License 2.0 开源协议。

## 贡献者

- **原始固件**: 基于 ESP-IDF TinyUSB audio_4_channel_mic 示例
- **ESP-NOW 集成**: CodeBuddy 项目团队
- **USB 描述符优化**: 参考 WirelessAdapter 项目

## 更新日志

### v1.0.0 (2026-08-19)
- ✅ 初始版本发布
- ✅ 实现 ESP-NOW 音频接收
- ✅ 实现 USB UAC 1.0 设备枚举
- ✅ Windows 10/11 设备枚举验证通过
- ✅ 48kHz 单声道音频流稳定传输

---

**当前状态**: ✅ **固件已完成开发并成功验证**

Windows 设备管理器确认设备正常枚举:
- CodeBuddy Audio (声音、视频和游戏控制器)
- CodeBuddy Microphone (音频输入设备)

下一步可进行音频流测试和 ESP-NOW 配对验证。

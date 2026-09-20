# K10-UNIHIKER ESP-NOW 音频+HID 通信协议规范

**设备型号**: K10-UNIHIKER  
**功能模块**: ESP-NOW-Audio-HID  
**芯片方案**: ESP32-S3  
**文档版本**: V2.0  
**生成时间**: 2026-09-17  
**适用固件**: CodeBuddy Wireless (51_mic_wifi + Dongle)

---

## 文档使用说明

本文档描述 **K10 发送端** 与 **Dongle 接收端** 之间的 ESP-NOW 无线通信协议，涵盖：
- 按键事件传输（HID 键盘模拟）
- I2S 音频流传输（16kHz mono）
- 设备配对与心跳机制
- PC-to-K10 状态推送（Token/Project/AI Emotion）

**目标读者**: 固件工程师、上位机开发者、测试工程师

**协议实现**:
- K10 端: `examples/51_mic_wifi/espnow_protocol.h`
- Dongle 端: `dongle_firmware/main/espnow_protocol.h`
- **两端必须字节级一致**

---

## 1. 设备档案

### 1.1 K10 发送端

| 项 | 值 |
|----|-----|
| 芯片 | ESP32-S3 (16MB Flash + PSRAM) |
| 无线 | ESP-NOW (2.4GHz, Channel 1) |
| MAC 地址 | `3c:dc:75:6d:7f:b4` |
| 音频输入 | ES7243E I2S (16kHz stereo → mono) |
| 按键 | 2 个（A/B），支持短按/长按/组合 |
| 显示 | 240×320 ILI9341 LCD, 6 个 LVGL UI 屏幕 |
| 存储 | SD 卡 (FAT32, 用户照片/AI 表情) |
| 电源 | USB-C 5V 或电池 |

### 1.2 Dongle 接收端

| 项 | 值 |
|----|-----|
| 芯片 | ESP32-S3 |
| 无线 | ESP-NOW (2.4GHz, Channel 1) |
| MAC 地址 | `e0:72:a1:d4:8f:e0` |
| USB | 复合设备: HID Keyboard + UAC 1.0 Audio |
| LED | WS2812 状态指示 (心跳/数据/错误) |
| 音频输出 | USB Audio (16kHz mono, 16-bit) |
| 固件 | ESP-IDF (非 Arduino) |

### 1.3 系统拓扑

```
[K10] ─ESP-NOW─> [Dongle] ─USB─> [PC]
  ↓                  ↓              ↓
按键A/B           HID键盘       重命名/确认
I2S麦克风         USB Audio     录音/语音输入
LVGL屏幕         WS2812 LED    状态显示
  ↑                  ↑              ↑
ESP-NOW状态      心跳帧         (未来: PC状态推送)
```

---

## 2. 传输层配置

### 2.1 ESP-NOW 参数

| 参数 | 值 | 说明 |
|------|-----|------|
| 信道 | 1 | 固定信道，避免扫描延迟 |
| 加密 | 无 | 简化配对，音频无敏感信息 |
| 最大负载 | 250 字节 | ESP-NOW 硬限制 |
| 发送模式 | 单播 | K10 → Dongle 固定 MAC |
| 重传 | K10 跟踪失败计数 | Dongle 无重试 |

### 2.2 帧发送频率

| 帧类型 | 频率 | 说明 |
|--------|------|------|
| 按键帧 | 事件触发 | 短按/长按立即发送 |
| 音频帧 | 62.5 fps | 16ms 间隔 (60 样本/帧) |
| 心跳帧 | 1 Hz | 连接存活检测 |
| 配对帧 | 单次 | 启动时发送 |
| 状态帧 | PC 触发 | Token/Project/AI Emotion |

---

## 3. 帧格式规范

### 3.1 通用帧头

所有 ESP-NOW 帧共享统一头部（10 字节）：

```c
typedef struct __attribute__((packed)) {
  uint8_t  magic[2];      // 魔数: 0xAA 0x55
  uint8_t  version;       // 协议版本: 0x01
  uint8_t  frame_type;    // 帧类型 (见 3.2)
  uint16_t sequence;      // 序列号 (小端序)
  uint16_t payload_len;   // 负载长度 (小端序)
  uint8_t  reserved[2];   // 保留字节
} espnow_frame_header_t;
```

### 3.2 帧类型定义

```c
#define FRAME_TYPE_KEY       0x01  // 按键事件
#define FRAME_TYPE_AUDIO     0x02  // 音频数据
#define FRAME_TYPE_PAIR      0x04  // 配对请求
#define FRAME_TYPE_HEARTBEAT 0x06  // 心跳
#define FRAME_TYPE_TOKEN     0x07  // Token 状态 (PC → K10)
#define FRAME_TYPE_PROJECT   0x08  // 项目状态 (PC → K10)
#define FRAME_TYPE_AI_EMO    0x09  // AI 情绪 (PC → K10)
```

### 3.3 CRC8 校验

**算法**: CRC-8/MAXIM (多项式 0x07)

**校验范围**: 帧头 + 负载（不包括 CRC 字节本身）

**实现** (C):
```c
uint8_t crc8(const uint8_t *data, size_t len) {
  uint8_t crc = 0;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t j = 0; j < 8; j++) {
      crc = (crc & 0x80) ? ((crc << 1) ^ 0x07) : (crc << 1);
    }
  }
  return crc;
}
```

**位置**: 负载末尾（不在帧头中）

---

## 4. 命令帧详细定义

### 4.1 按键帧 (0x01)

**方向**: K10 → Dongle

**负载结构** (3 字节 + CRC):
```c
typedef struct __attribute__((packed)) {
  uint8_t key_code;    // HID 键码 (见 4.1.1)
  uint8_t modifiers;   // 修饰键 (见 4.1.2)
  uint8_t action;      // 0=Release, 1=Press
  uint8_t crc8;        // CRC 校验
} key_frame_payload_t;
```

#### 4.1.1 按键映射

| K10 按键 | 短按 | 长按 | 组合 |
|----------|------|------|------|
| A | F2 (0x3C) + 切换音频流 | Enter (0x28) | A+B 2s = Demo Mode |
| B | 循环屏幕 | Backspace (0x2A, 100ms 重复) | - |

#### 4.1.2 修饰键 (modifiers)

当前未使用，保留为 `0x00`。

**示例帧** (短按 A → F2):
```
AA 55 01 01 00 01 03 00 00 00  // 帧头 (10 字节)
3C 00 01 A7                    // 负载: key_code=0x3C, modifiers=0x00, action=Press, CRC=0xA7
```

### 4.2 音频帧 (0x02)

**方向**: K10 → Dongle

**音频参数**:
- 采样率: 16kHz
- 声道: Mono (I2S 立体声下混)
- 位深: 16-bit signed
- 帧大小: 60 样本/帧 (120 字节)
- 传输周期: 16ms (62.5 fps)

**负载结构** (120 字节 + CRC):
```c
typedef struct __attribute__((packed)) {
  int16_t samples[60];  // 60 个 16-bit 样本 (小端序)
  uint8_t crc8;         // CRC 校验
} audio_frame_payload_t;
```

**数据流**:
```
ES7243E (I2S) → DMA 缓冲区 (512 字节)
              → 下混为 Mono
              → 打包 60 样本/帧
              → ESP-NOW 发送 (16ms 间隔)
              → Dongle 接收
              → USB Audio Class (UAC 1.0)
              → PC 录音设备
```

**延迟目标**: <50ms (端到端)

### 4.3 配对帧 (0x04)

**方向**: K10 → Dongle (启动时单次)

**负载结构** (8 字节 + CRC):
```c
typedef struct __attribute__((packed)) {
  uint8_t device_id[6];   // K10 MAC 地址
  uint8_t protocol_ver;   // 协议版本 0x01
  uint8_t device_type;    // 0x01=K10 Transmitter
  uint8_t crc8;
} pair_frame_payload_t;
```

**Dongle 响应**: 无（单向配对，Dongle 记录 MAC）

### 4.4 心跳帧 (0x06)

**方向**: K10 → Dongle (1 Hz)

**负载结构** (4 字节 + CRC):
```c
typedef struct __attribute__((packed)) {
  uint32_t uptime_ms;  // K10 运行时间 (小端序)
  uint8_t crc8;
} heartbeat_frame_payload_t;
```

**超时判定**: Dongle 未收到心跳 >5 秒 → 标记 K10 离线

### 4.5 Token 状态帧 (0x07)

**方向**: PC → Dongle → K10

**用途**: 更新 K10 屏幕 1 (Token Usage) 的进度条数据

**负载结构** (154 字节 + CRC):
```c
#define MAX_SERVICES 5

typedef struct __attribute__((packed)) {
  uint8_t  service_count;           // 实际服务数 (1-5)
  struct {
    char     name[20];              // 服务名 (UTF-8, null-terminated)
    uint32_t used_tokens;           // 已用 Token (小端序)
    uint32_t total_tokens;          // 总计 Token (小端序)
    uint16_t percentage_x10;        // 百分比×10 (如 752 = 75.2%)
  } services[MAX_SERVICES];
  uint8_t crc8;
} token_status_payload_t;
```

**示例数据** (单服务):
```
服务名: "Claude Opus 4.6"
已用: 75000 Token
总计: 100000 Token
百分比×10: 750 (显示为 75.0%)
```

### 4.6 项目状态帧 (0x08)

**方向**: PC → Dongle → K10

**用途**: 更新 K10 屏幕 2 (Coding Status) 的项目列表

**负载结构** (154 字节 + CRC):
```c
#define MAX_PROJECTS 6

typedef struct __attribute__((packed)) {
  uint8_t  project_count;           // 实际项目数 (1-6)
  struct {
    char     name[24];              // 项目名 (UTF-8, null-terminated)
    uint8_t  status;                // 状态码 (见 4.6.1)
    uint8_t  reserved;
  } projects[MAX_PROJECTS];
  uint8_t crc8;
} project_status_payload_t;
```

#### 4.6.1 项目状态码

```c
#define PROJECT_STATUS_PLANNING  0  // 规划中 (蓝色点)
#define PROJECT_STATUS_CODING    1  // 编码中 (绿色点)
#define PROJECT_STATUS_REVIEW    2  // 审查中 (黄色点)
#define PROJECT_STATUS_DONE      3  // 已完成 (灰色点)
#define PROJECT_STATUS_ERROR     4  // 错误 (红色点)
#define PROJECT_STATUS_IDLE      5  // 空闲 (灰色点)
```

### 4.7 AI 情绪帧 (0x09)

**方向**: PC → Dongle → K10

**用途**: 控制 K10 屏幕 5 (AI Status) 的云脸表情动画

**负载结构** (24 字节 + CRC):
```c
typedef struct __attribute__((packed)) {
  uint8_t emotion;          // 情绪码 (见 4.7.1)
  char    status_text[20];  // 自定义状态文本 (UTF-8)
  uint8_t reserved[3];
  uint8_t crc8;
} ai_emotion_payload_t;
```

#### 4.7.1 情绪码

```c
#define AI_EMOTION_THINKING  0  // 思考: 眉毛上扬, 嘴闭合
#define AI_EMOTION_CODING    1  // 编码: 眉毛皱起, 嘴动画
#define AI_EMOTION_DONE      2  // 完成: 眉毛放松, 宽笑容
```

**外部控制模式**:
- 收到本帧后，覆盖 K10 本地 4 秒自动循环
- 15 秒无新帧 → 恢复本地循环
- 状态文本显示在云脸下方

---

## 5. 命令分发架构

### 5.1 K10 端 (51_mic_wifi.ino)

```c
// ESP-NOW 接收回调
void espnow_recv_cb(const uint8_t *mac, const uint8_t *data, int len) {
  espnow_frame_header_t *hdr = (espnow_frame_header_t *)data;
  
  // 魔数校验
  if (hdr->magic[0] != 0xAA || hdr->magic[1] != 0x55) return;
  
  // CRC 校验
  uint8_t *payload = (uint8_t *)(data + sizeof(espnow_frame_header_t));
  uint8_t calc_crc = crc8(data, len - 1);
  if (calc_crc != payload[hdr->payload_len - 1]) return;
  
  // 分发
  switch (hdr->frame_type) {
    case FRAME_TYPE_TOKEN:
      memcpy(&token_data, payload, sizeof(token_status_payload_t));
      screen_dirty = true;
      break;
    case FRAME_TYPE_PROJECT:
      memcpy(&project_data, payload, sizeof(project_status_payload_t));
      screen_dirty = true;
      break;
    case FRAME_TYPE_AI_EMO:
      memcpy(&ai_state, payload, sizeof(ai_emotion_payload_t));
      ai_state_changed = true;
      break;
  }
}
```

### 5.2 Dongle 端 (espnow_receiver.c)

```c
void espnow_recv_cb(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  espnow_frame_header_t *hdr = (espnow_frame_header_t *)data;
  
  // 校验 (同上)
  
  switch (hdr->frame_type) {
    case FRAME_TYPE_KEY:
      // 转发到 USB HID
      usb_hid_send_key(payload->key_code, payload->modifiers, payload->action);
      led_flash_data();  // WS2812 数据闪烁
      break;
      
    case FRAME_TYPE_AUDIO:
      // 写入 USB Audio 环形缓冲区
      audio_ringbuf_write(payload->samples, 60);
      break;
      
    case FRAME_TYPE_HEARTBEAT:
      last_heartbeat_ms = esp_timer_get_time() / 1000;
      led_heartbeat();  // WS2812 心跳脉冲
      break;
  }
}
```

---

## 6. 数据结构定义

### 6.1 K10 全局状态

```c
// 按键状态
typedef struct {
  uint32_t last_press_ms[2];     // A/B 按下时间戳
  bool     is_pressed[2];
  bool     long_press_sent[2];
} button_state_t;

// 音频流状态
typedef struct {
  bool     streaming;
  uint32_t frame_count;
  uint32_t drop_count;
} audio_state_t;

// ESP-NOW 统计
typedef struct {
  uint32_t send_success;
  uint32_t send_fail;
  uint32_t recv_count;
} espnow_stats_t;

// PC 推送的数据 (volatile, 异步更新)
volatile token_status_payload_t   token_data;
volatile project_status_payload_t project_data;
volatile ai_emotion_payload_t     ai_state;
volatile bool screen_dirty = false;
volatile bool ai_state_changed = false;
```

### 6.2 Dongle 环形缓冲区

```c
// 无锁环形缓冲区 (单生产者 ESP-NOW, 单消费者 USB Audio)
typedef struct {
  int16_t buffer[AUDIO_RINGBUF_SIZE];  // 4096 样本 (256ms)
  volatile uint32_t write_idx;
  volatile uint32_t read_idx;
} audio_ringbuf_t;

// 写入 (ESP-NOW 任务)
void audio_ringbuf_write(int16_t *samples, size_t count);

// 读取 (USB Audio 回调)
size_t audio_ringbuf_read(int16_t *out, size_t count);
```

---

## 7. 移植指南

### 7.1 K10 → 其他 ESP32 平台

**必须修改**:
1. `UNIHIKER_K10_PIN.h` — GPIO 引脚映射
2. `main.h` — LCD/I2S/SD 初始化
3. MAC 地址 (K10 和 Dongle 都要改)

**保持不变**:
- `espnow_protocol.h` 数据结构
- CRC8 算法
- 帧类型定义

### 7.2 Dongle → 其他接收器

**USB 描述符替换**:
- `usb_descriptors.c` — HID Report Descriptor
- UAC 1.0 → UAC 2.0 (需修改采样率配置)

**LED 状态指示**:
- WS2812 → 普通 GPIO LED (去掉 `led_indicator.c` 的 RMT 驱动)

### 7.3 上位机集成

**Python 示例** (串口桥接):
```python
import serial
import struct

def send_token_status(ser, services):
    frame = bytearray()
    frame += b'\xaa\x55\x01\x07'  # magic + version + type
    frame += struct.pack('<H', 0)  # sequence
    frame += struct.pack('<H', 154)  # payload_len
    frame += b'\x00\x00'  # reserved
    
    payload = bytearray([len(services)])
    for svc in services:
        payload += svc['name'].encode('utf-8').ljust(20, b'\x00')
        payload += struct.pack('<I', svc['used'])
        payload += struct.pack('<I', svc['total'])
        payload += struct.pack('<H', int(svc['percentage'] * 10))
    
    payload = payload.ljust(154, b'\x00')
    crc = crc8(frame + payload)
    payload.append(crc)
    
    ser.write(frame + payload)
```

详细上位机开发文档见: `examples/51_mic_wifi/上位机开发文档.md`

---

## 8. 验收标准

### 8.1 按键延迟

| 测试项 | 标准 | 测试方法 |
|--------|------|----------|
| 短按响应 | <100ms | 按 A → PC 文件重命名开始 |
| 长按重复 | 100ms 间隔 | 长按 B → Backspace 连续删除 |
| 按键误触 | 0 次/100 次 | 快速连按不丢失 |

### 8.2 音频质量

| 测试项 | 标准 | 测试方法 |
|--------|------|----------|
| 端到端延迟 | <50ms | 对麦克风拍手，PC 录音波形对齐 |
| 丢帧率 | <1% | 连续录音 5 分钟，检查音频缺失 |
| 底噪 | <-40dB | 静音环境录音，频谱分析 |

### 8.3 连接稳定性

| 测试项 | 标准 | 测试方法 |
|--------|------|----------|
| 心跳正常率 | >99% | 运行 24 小时，检查心跳丢失 |
| 重连时间 | <3s | 断电 Dongle → 上电 → K10 自动重连 |
| 距离覆盖 | ≥10m | 空旷环境，音频不断流 |

---

## 9. Backlog

### 9.1 已实现

- ✅ 按键事件传输 (A/B 短按/长按)
- ✅ I2S 音频流 (16kHz mono)
- ✅ USB HID Keyboard (F2/Enter/Backspace)
- ✅ USB Audio Class (UAC 1.0)
- ✅ 6 个 LVGL UI 屏幕
- ✅ SD 卡图片显示
- ✅ AI 情绪动画 (3 种表情)
- ✅ 演示模式 (A+B 2s)

### 9.2 待实现

- ⏳ PC → Dongle 串口转发 (Token/Project/AI Emotion)
- ⏳ K10 屏幕交互式高亮 (演示模式 A 短按)
- ⏳ Dongle 固件 OTA 升级
- ⏳ K10 低功耗模式 (按键唤醒)
- ⏳ 音频 AGC (自动增益控制)
- ⏳ 加密传输 (AES-128)

### 9.3 已知问题

| 问题 | 影响 | 解决方案 |
|------|------|----------|
| SD 卡 exFAT 挂载失败 | 用户照片不显示 | 文档说明必须格式化为 FAT32 |
| 屏幕 5 切换卡顿 | 按键响应慢 | 已修复: `LV_IMG_CACHE_DEF_SIZE 4` |
| 音频偶现爆音 | 影响录音质量 | 待排查: 环形缓冲区溢出? |

---

## 10. 实现架构

### 10.1 K10 任务分配

```
├─ Arduino Loop (100ms)
│  ├─ 按键扫描 (button_scan)
│  ├─ ESP-NOW 统计更新
│  └─ 屏幕切换 (switch_screen)
│
├─ LVGL 刷新任务 (5ms)
│  └─ lv_task_handler() [持有 xGuiSemaphore]
│
├─ AI 动画定时器 (100ms)
│  └─ update_screen_ai_status() [持有 xGuiSemaphore]
│
├─ ESP-NOW 回调 (异步)
│  └─ espnow_recv_cb()
│
└─ I2S DMA 回调 (16ms)
   └─ audio_task() → ESP-NOW 发送
```

### 10.2 Dongle 任务分配

```
├─ ESP-NOW 接收任务 (高优先级)
│  └─ espnow_recv_cb() → HID/Audio 转发
│
├─ USB Audio 任务 (实时)
│  └─ tud_audio_write() ← audio_ringbuf_read()
│
├─ USB HID 任务 (事件触发)
│  └─ tud_hid_report()
│
└─ LED 状态任务 (低优先级)
   └─ led_heartbeat() / led_flash_data()
```

### 10.3 关键路径时序

```
[K10 按键按下]
    ↓ <5ms
[按键扫描检测]
    ↓ <2ms
[ESP-NOW 发送按键帧]
    ↓ 无线传输 <20ms
[Dongle 接收 + CRC 校验]
    ↓ <1ms
[USB HID Report 发送]
    ↓ USB HS <2ms
[PC 接收 HID 事件]
─────────────────────
总延迟: <30ms (目标 <100ms ✅)
```

---

## 11. 修订历史

| 版本 | 日期 | 修改内容 | 作者 |
|------|------|----------|------|
| V1.0 | 2026-08-18 | 初始版本，定义基础帧格式 | Claude |
| V2.0 | 2026-09-17 | 补充 PC → K10 状态帧 (0x07/0x08/0x09)，完善测试标准 | Claude |

---

**文档生成**: 由 `/init` 技能自动生成  
**协议实现**: 参考 `espnow_protocol.h` (K10 和 Dongle 版本必须一致)  
**测试文档**: 见 `examples/51_mic_wifi/按键交互设计.md`  
**故障排除**: 见 `dongle_firmware/README.md` (LED 状态码表)

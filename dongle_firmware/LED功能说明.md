# CodeBuddy Dongle - WS2812 RGB LED 状态指示功能

## 概述

Dongle 固件集成了 WS2812 可编址 RGB LED 状态指示功能，通过不同颜色和闪烁模式直观显示设备工作状态和数据传输情况。

## 硬件说明

- **LED 型号**: WS2812 可编址 RGB LED（单颗）
- **GPIO 引脚**: GPIO 48
- **驱动方式**: ESP-IDF `led_strip` 组件（通过 RMT 外设生成 WS2812 时序）
- **参考文档**: `ESP32-S3-N16R8_User_Guide.pdf`

> ⚠️ **重要**: WS2812 是可编址 RGB LED，**不能**用普通 GPIO 高低电平驱动。必须使用 RMT 外设发送特定时序协议。本固件使用官方 `led_strip` 组件实现。

## LED 状态定义

| 状态 | 颜色 | 闪烁模式 | 含义 |
|-----|------|---------|------|
| 初始化中 | 🔴 红色 | 慢闪 1Hz | USB PHY/TinyUSB 加载中 |
| 初始化失败 | 🔴 红色 | 快闪 5Hz | TinyUSB 初始化失败 |
| 正常工作 | 🟢 绿色 | 心跳双闪 | Dongle 就绪，等待数据 |
| 数据传输 | 🔵 蓝色 | 快闪 50ms | ESP-NOW 接收 / USB 发送 |

### 心跳模式时序

```
绿色亮 100ms → 灭 100ms → 绿色亮 100ms → 灭 1000ms → (循环)
   ●            ○            ●              ○○○○○○○○○○
```

### 数据传输覆盖机制

数据传输指示会临时覆盖心跳模式：
```
心跳模式 → [收到数据] → 蓝色快闪 → [500ms 无数据] → 恢复心跳
```

## 代码结构

```
dongle_firmware/main/
├── led_indicator.h       # LED API 接口定义
├── led_indicator.c       # WS2812 驱动实现（led_strip + RMT）
├── config.h              # LED_GPIO 引脚配置 (GPIO 48)
├── main.c                # LED 初始化和模式切换
├── espnow_receiver.c     # 接收数据时调用 led_indicate_data_activity()
└── idf_component.yml     # 依赖声明（espressif/led_strip）
```

## API 接口

```c
#include "led_indicator.h"

// LED 工作模式
typedef enum {
    LED_OFF = 0,        // 关闭
    LED_ON,             // 常亮（绿色）
    LED_SLOW_BLINK,     // 慢闪 (1Hz) - 初始化中 (蓝色)
    LED_FAST_BLINK,     // 快闪 (5Hz) - 错误状态 (红色)
    LED_HEARTBEAT,      // 心跳 (双闪) - 正常工作 (绿色)
    LED_DATA_BLINK      // 数据活动快闪 (橙色)
} led_mode_t;

// 初始化 WS2812 LED（在 app_main 中调用）
esp_err_t led_indicator_init(void);

// 设置 LED 工作模式
void led_set_mode(led_mode_t mode);

// 获取当前 LED 模式
led_mode_t led_get_mode(void);

// 指示数据活动（自动蓝色快闪 500ms 后恢复）
void led_indicate_data_activity(void);

// 清理资源
void led_deinit(void);
```

## 实现要点

### 1. WS2812 初始化

```c
led_strip_config_t strip_config = {
    .strip_gpio_num = LED_GPIO,      // GPIO 48
    .max_leds = 1,                   // 单颗 LED
    .led_model = LED_MODEL_WS2812,   // WS2812 型号
    .flags.invert_out = false,
};

led_strip_rmt_config_t rmt_config = {
    .clk_src = RMT_CLK_SRC_DEFAULT,
    .resolution_hz = 10 * 1000 * 1000,  // 10MHz
    .flags.with_dma = false,
};

led_strip_new_rmt_device(&strip_config, &rmt_config, &g_led_strip);
```

### 2. 颜色设置

```c
// 设置 RGB 颜色并刷新
led_strip_set_pixel(g_led_strip, 0, r, g, b);
led_strip_refresh(g_led_strip);
```

### 3. 非阻塞任务

LED 控制运行在独立 FreeRTOS 任务中（Core 0，优先级 3），10ms 节拍，不影响主任务（USB 任务优先级 5）。

## 依赖管理

`main/idf_component.yml` 声明了 led_strip 依赖：

```yaml
dependencies:
  idf: ">=5.0"
  espressif/tinyusb: "^0.17.0"
  espressif/led_strip: "^2.5.3"
```

首次编译时 ESP-IDF 会自动从组件注册表下载 `led_strip`（当前使用 v2.5.5）。

## 配置修改

### 修改 LED 引脚

编辑 `main/config.h`:
```c
#define LED_GPIO    48    // WS2812 数据引脚
```

### 修改状态颜色

编辑 `main/led_indicator.c` 中的颜色定义：
```c
static const rgb_color_t COLOR_INIT      = {0, 0, 50};     // 初始化：蓝色
static const rgb_color_t COLOR_ERROR     = {50, 0, 0};     // 错误：红色
static const rgb_color_t COLOR_HEARTBEAT = {0, 20, 0};     // 心跳：绿色
static const rgb_color_t COLOR_DATA      = {30, 15, 0};    // 数据：橙色
```

> 💡 亮度建议不超过 50/255，避免 LED 过亮刺眼且降低功耗。

## 验证结果

固件已烧录到 COM6 dongle，串口日志确认 LED 初始化成功：

```
I (870) led: WS2812 LED strip initialized on GPIO 48
I (873) led: LED indicator initialized on GPIO 48
I (878) main: >>> LED Indicator Initialized <<<
I (3888) main: >>> USB PHY INITIALIZED SUCCESSFULLY <<<
I (3889) main: >>> TinyUSB INITIALIZED SUCCESSFULLY <<<
```

USB 设备枚举正常：
- ✅ CodeBuddy Wireless Dongle (MEDIA)
- ✅ 麦克风 (CodeBuddy Wireless Dongle) (AudioEndpoint)

详细测试步骤见 [LED测试验证清单.md](./LED测试验证清单.md)

---

**最后更新**: 2026-08-20
**LED 型号**: WS2812 (GPIO 48)
**驱动组件**: espressif/led_strip v2.5.5

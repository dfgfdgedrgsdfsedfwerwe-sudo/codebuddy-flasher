# ADC 分压四键硬件注意事项

## 故障排查记录

**现象**: 在 GPIO2 上实现 ADC 分压四键后，K10 无法启动（串口无输出，屏幕不亮）

**根本原因**: GPIO2 是 ESP32-S3 的 **strapping pin**
- 上电时采样 GPIO2 电平决定启动模式
- GPIO2 高电平 → 正常启动用户程序
- GPIO2 低电平 → 进入 Download 模式（不运行代码）
- 分压网络 + ADC 配置会影响 GPIO2 电平，导致误判为 Download 模式

**验证结果**: 
- 注释掉 `adc_key_init()` → 系统正常启动，串口有输出 ✅
- 启用 `adc_key_init()` 配置 GPIO2 → 启动失败 ❌

## 硬件要求

**必须使用非 strapping pin**，推荐 GPIO1 (ADC1_CH0)：

```
GPIO1 (ADC1_CH0) ─┬─ 10kΩ ─ 3.3V (上拉)
                  │
                  ├─ [按键 UP]    ─ 220Ω  ─ GND
                  ├─ [按键 DOWN]  ─ 1kΩ   ─ GND
                  ├─ [按键 LEFT]  ─ 4.7kΩ ─ GND
                  └─ [按键 RIGHT] ─ 10kΩ  ─ GND
```

**ESP32-S3 strapping pins（禁止用于 ADC 分压键）**:
- GPIO0  (BOOT 按键)
- GPIO2  ← 本次故障源
- GPIO3  (JTAG)
- GPIO45
- GPIO46

## 电压阈值（代码中已配置，适配上述电阻值）

| 按键  | 串联电阻 | 预期电压 (mV) | 代码阈值 (mV) |
|-------|---------|--------------|--------------|
| UP    | 220Ω    | ~200         | 0 ~ 300      |
| DOWN  | 1kΩ     | ~500         | 300 ~ 800    |
| LEFT  | 4.7kΩ   | ~1100        | 800 ~ 1400   |
| RIGHT | 10kΩ    | ~1650        | 1400 ~ 2100  |
| NONE  | (开路)   | ~3300        | > 2100       |

## 当前代码配置

文件: `51_mic_wifi.ino`

```c
#define ADC_KEY_PIN     1       // GPIO1 (ADC1_CH0)
#define ADC_KEY_CHANNEL ADC1_CHANNEL_0
#define ADC_SAMPLES     8       // 8 次采样平均
#define ADC_DEBOUNCE_MS 50      // 50ms 消抖
```

## 移植到其他项目的注意事项

1. **优先选择 ADC1 通道**（GPIO1/4/5/6/7/8/9/10）
   - ADC2 通道在 WiFi 激活时不可用
   - 本项目使用 ESP-NOW（需要 WiFi），因此必须用 ADC1

2. **避开 strapping pins**（见上表）

3. **验证引脚可用性**：检查目标 GPIO 是否已被其他外设占用
   - 本项目中 GPIO1 未占用，可安全使用
   - 参考 `UNIHIKER_K10_PIN.h` 查看引脚分配

## 测试方法

烧录固件后，打开串口监视器（115200 baud）：

```bash
python -m platformio device monitor -p COM15 -b 115200
```

预期输出：
```
ADC key initialized on GPIO1 (ADC1_CH0)
...
ADC key pressed: UP (voltage: 250 mV, short press)
ADC key pressed: DOWN (voltage: 520 mV, long press)
```

按住按键 ≥600ms 触发长按。

---
**最后更新**: 2026-09-18  
**固件版本**: 51_K10_wifi (ADC 四键扩展)

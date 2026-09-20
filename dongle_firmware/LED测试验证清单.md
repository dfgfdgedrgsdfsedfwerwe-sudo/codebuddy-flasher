# CodeBuddy Dongle - WS2812 RGB LED 测试验证清单

## ✅ 硬件信息

- **LED 型号**: WS2812 可编址 RGB LED
- **GPIO 引脚**: GPIO 48
- **驱动库**: ESP-IDF `led_strip` 组件
- **固件版本**: codebuddy_dongle v1.0 (2026-08-20)

## 📋 LED 状态指示说明

### 1. 初始化阶段（上电 0-3 秒）

| LED 状态 | 颜色 | 闪烁频率 | 含义 |
|---------|------|---------|------|
| **慢闪** | 🔴 红色 | 1 Hz (500ms 亮 / 500ms 灭) | 正在初始化 USB PHY 和 TinyUSB |

**预期日志**:
```
I (870) led: WS2812 LED strip initialized on GPIO 48
I (873) led: LED indicator initialized on GPIO 48
I (878) main: >>> LED Indicator Initialized <<<
I (881) main: >>> Delay 3s before USB PHY <<<
```

### 2. 初始化失败（罕见情况）

| LED 状态 | 颜色 | 闪烁频率 | 含义 |
|---------|------|---------|------|
| **快闪** | 🔴 红色 | 5 Hz (100ms 亮 / 100ms 灭) | TinyUSB 初始化失败 |

**触发条件**: `tusb_init()` 返回 false

### 3. 正常工作模式（初始化成功后）

| LED 状态 | 颜色 | 闪烁模式 | 含义 |
|---------|------|---------|------|
| **心跳** | 🟢 绿色 | 双闪模式 (100ms 亮 → 100ms 灭 → 100ms 亮 → 1000ms 灭) | Dongle 正常工作，等待数据 |

**预期日志**:
```
I (3888) main: >>> USB PHY INITIALIZED SUCCESSFULLY <<<
I (3889) main: >>> TinyUSB INITIALIZED SUCCESSFULLY <<<
I (3894) main: >>> ESP-NOW Receiver STARTED <<<
```

### 4. 数据传输指示

| LED 状态 | 颜色 | 触发时机 | 持续时间 |
|---------|------|---------|---------|
| **快速闪烁** | 🔵 蓝色 | ESP-NOW 接收到音频帧 | 50 ms |
| **快速闪烁** | 🔵 蓝色 | USB 发送 HID 键盘报告 | 50 ms |
| **快速闪烁** | 🔵 蓝色 | USB 发送 UAC 音频帧 | 50 ms |

**工作原理**: 
- 数据传输时 LED 短暂变蓝闪烁
- 50ms 后自动恢复心跳模式
- 频繁传输时会看到持续蓝光

---

## 🧪 测试步骤

### 测试 1: 验证上电初始化

**步骤**:
1. 将 Dongle USB 线连接到 **COM 口**（用于串口日志）
2. 打开串口监视器: `idf.py -p COM6 monitor`
3. 按下 Dongle 的 RESET 按钮（或重新上电）
4. **观察 LED**: 应看到红色慢闪（1 Hz）持续约 3 秒

**预期结果**:
- ✅ LED 红色慢闪（初始化中）
- ✅ 串口输出: `LED indicator initialized on GPIO 48`
- ✅ 3 秒后变为绿色心跳模式

### 测试 2: 验证 USB 枚举后的心跳

**步骤**:
1. 拔出 USB 线，从 **COM 口** 改插到 **USB 口**
2. 等待 Windows 枚举 USB 设备（约 2-3 秒）
3. **观察 LED**: 应看到绿色双闪心跳模式

**预期结果**:
- ✅ LED 显示绿色心跳：闪-闪-停-闪-闪-停...
- ✅ 设备管理器出现 "CodeBuddy Wireless Dongle"
- ✅ 设备管理器出现 "麦克风 (CodeBuddy Wireless Dongle)"

**验证命令** (PowerShell):
```powershell
Get-PnpDevice | Where-Object { $_.FriendlyName -like "*CodeBuddy*" } | Select Status, FriendlyName
```

### 测试 3: 验证 ESP-NOW 接收指示（需要 K10 设备）

**前提**: K10 设备已烧录发送固件并正常工作

**步骤**:
1. Dongle 插在 **USB 口**（保持心跳模式）
2. K10 设备上电，开始发送 ESP-NOW 数据
3. **观察 LED**: 每次接收音频帧时应短暂变蓝闪烁

**预期结果**:
- ✅ 心跳模式期间，间歇性出现蓝色快闪
- ✅ K10 对着麦克风说话时，LED 蓝光闪烁更频繁
- ✅ K10 静音时，LED 恢复纯心跳模式

### 测试 4: 验证 HID 键盘数据指示（需要 K10 设备）

**步骤**:
1. K10 设备按下按键（例如 F2 语音键）
2. **观察 LED**: 按键时应短暂变蓝闪烁
3. 同时观察电脑是否接收到键盘输入

**预期结果**:
- ✅ 按键瞬间 LED 蓝色闪烁
- ✅ 电脑接收到对应按键（F2/Enter/Backspace）

### 测试 5: 验证 USB 音频发送指示

**步骤**:
1. 打开 Windows "录音机" 或 Audacity
2. 选择 "CodeBuddy Wireless Dongle" 作为输入设备
3. 开始录音
4. K10 对着麦克风说话
5. **观察 LED**: 录音期间应持续蓝光闪烁

**预期结果**:
- ✅ 录音时 LED 持续蓝光（频繁数据传输）
- ✅ 停止录音后恢复心跳模式
- ✅ 录音文件有声音（验证音频链路完整）

---

## 🐛 故障排查

### 问题 1: LED 不亮

**可能原因**:
1. GPIO 48 未正确连接 WS2812
2. WS2812 需要 5V 供电（检查硬件原理图）
3. `led_strip` 组件未正确安装

**解决方案**:
```bash
# 重新下载 led_strip 组件
cd dongle_firmware
idf.py reconfigure
idf.py build
```

### 问题 2: LED 显示错误颜色

**可能原因**: WS2812 颜色顺序配置错误（GRB vs RGB）

**解决方案**: 
检查 `led_indicator.c` 中的配置：
```c
.led_pixel_format = LED_PIXEL_FORMAT_GRB,  // 确认颜色顺序
```

### 问题 3: LED 一直红色快闪

**含义**: TinyUSB 初始化失败

**解决方案**:
1. 检查串口日志: `idf.py -p COM6 monitor`
2. 查找错误信息: `TinyUSB INIT FAILED`
3. 确认 USB PHY 配置正确（sdkconfig）

### 问题 4: LED 心跳正常但无数据指示

**可能原因**:
1. K10 设备未发送数据
2. ESP-NOW 配对失败（MAC 地址不匹配）
3. WiFi 信道不一致

**解决方案**:
1. 检查 K10 串口日志确认发送成功
2. 验证 Dongle MAC 地址: 在串口日志中查找 `MAC: xx:xx:xx:xx:xx:xx`
3. 确认两端信道一致（默认信道 1）

---

## 📊 LED 状态速查表

| 场景 | LED 颜色 | 闪烁模式 | 说明 |
|-----|---------|---------|------|
| 上电初始化 | 🔴 红色 | 慢闪 (1Hz) | 正在加载 USB |
| 初始化失败 | 🔴 红色 | 快闪 (5Hz) | TinyUSB 错误 |
| 待机状态 | 🟢 绿色 | 心跳双闪 | 正常工作 |
| 接收音频 | 🔵 蓝色 | 快闪 (50ms) | ESP-NOW 接收 |
| 发送 USB | 🔵 蓝色 | 快闪 (50ms) | HID/UAC 发送 |
| 频繁传输 | 🔵 蓝色 | 持续亮 | 高速数据流 |

---

## ✅ 验收标准

完整功能验收需满足：

- [x] ✅ 上电红色慢闪 3 秒
- [x] ✅ USB 枚举后绿色心跳
- [ ] ⏳ ESP-NOW 接收时蓝色闪烁（需 K10 联调）
- [ ] ⏳ HID 按键时蓝色闪烁（需 K10 联调）
- [ ] ⏳ 录音时持续蓝光（需 K10 联调）

**当前状态**: 
- COM6 Dongle 固件已烧录 ✅
- LED 初始化成功 ✅
- 等待 K10 设备联调验证完整功能 ⏳

---

**最后更新**: 2026-08-20
**固件版本**: codebuddy_dongle build 2026-08-20 11:17
**测试设备**: ESP32-S3-N16R8 (MAC: e0:72:a1:d4:8f:e0)

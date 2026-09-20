# USB 设备枚举检查清单

## 烧录完成后的验证步骤

### 1. 串口监视器检查
观察烧录完成后的串口输出，应该看到：
```
I (xxx) main: ========================================
I (xxx) main: CodeBuddy Dongle Starting...
I (xxx) main: ========================================
I (xxx) main: [1] Initializing NVS...
I (xxx) main: [2] Initializing WiFi + ESP-NOW...
I (xxx) main: [3] Initializing Audio Ring Buffer (64KB PSRAM)...
I (xxx) main: [4] Initializing USB PHY (OTG Mode)...
I (xxx) main: [5] Initializing TinyUSB...
I (xxx) main: [6] Creating FreeRTOS tasks...
I (xxx) main: ========================================
I (xxx) main: Dongle Ready!
I (xxx) main: ========================================
```

**关键点：** 
- 如果卡在"Initializing USB PHY"，说明 USB PHY 初始化失败
- 如果看不到"TinyUSB mounted"，说明 USB 枚举未成功

### 2. Windows 设备管理器检查

#### 方法 A: 设备管理器 (devmgmt.msc)
1. 按 `Win+X` → 选择"设备管理器"
2. 展开以下分类：
   - **人体学输入设备** → 应该看到 `HID-compliant device` 或 `CodeBuddy Dongle Keyboard`
   - **音频输入和输出** → 应该看到 `CodeBuddy Mic` (麦克风)

3. 右键设备 → 属性 → 详细信息 → 硬件 ID：
   ```
   USB\VID_CAFE&PID_4011&MI_00  (HID Keyboard)
   USB\VID_CAFE&PID_4011&MI_01  (UAC Microphone)
   ```

#### 方法 B: USB Device Tree Viewer (推荐)
使用 [USB Device Tree Viewer](https://www.uwe-sieber.de/usbtreeview_e.html) 查看：
- **Device Descriptor**:
  - idVendor: 0xCAFE
  - idProduct: 0x4011
  - iManufacturer: "DFRobot"
  - iProduct: "CodeBuddy Dongle"

- **Configuration Descriptor**:
  - Interface 0: HID (Keyboard)
  - Interface 1: Audio Control
  - Interface 2: Audio Streaming (Microphone)

### 3. 声音设置检查

1. **Windows 声音设置**:
   - 右键任务栏音量图标 → "声音设置"
   - 滚动到"输入" → 选择设备下拉菜单
   - 应该看到 **"CodeBuddy Mic"** 或 **"麦克风 (CodeBuddy Mic)"**

2. **测试麦克风**:
   - 选择"CodeBuddy Mic"
   - 点击"测试麦克风"按钮
   - 对着 K10 说话（当 K10 端启动 ESP-NOW 发送后）
   - 输入音量条应该有反应

### 4. 故障排查

#### 问题 1: 设备显示为 `USB Serial/JTAG`
**症状**: 设备管理器中显示 `VID_303A&PID_1001` 而不是 `VID_CAFE&PID_4011`

**原因**: USB PHY 未正确切换到 OTG 模式

**解决方案**:
- 检查串口输出是否有 USB PHY 初始化错误
- 确认 `usb_new_phy()` 在 `tusb_init()` 之前调用
- 验证 `esp_private/usb_phy.h` 头文件存在

#### 问题 2: 枚举为 HID 但没有音频设备
**症状**: 只看到键盘设备，没有麦克风

**原因**: USB 描述符配置错误或 TinyUSB 音频类未启用

**解决方案**:
- 检查 `usb_descriptors.c` 中的接口/端点描述符
- 确认 `CFG_TUD_AUDIO=1` 在 `tusb_config.h` 中定义
- 验证 `tud_audio_write()` 是否被调用

#### 问题 3: 音频设备有但无声音
**症状**: Windows 识别到麦克风，但录音测试无波形

**原因**: 
- K10 端未启动 ESP-NOW 发送（最可能）
- 环形缓冲区为空
- UAC 音频回调未执行

**检查步骤**:
1. 串口输出是否有 "ESP-NOW packet received" 日志？
2. 环形缓冲区是否有数据？（添加调试日志）
3. `tud_audio_write()` 返回值是多少？

### 5. 成功标志

✅ **完全成功的标志**:
- 设备管理器中出现两个设备（HID + Audio）
- 声音设置中可选择"CodeBuddy Mic"
- 串口监视器输出"TinyUSB mounted"
- VID/PID 正确 (CAFE:4011)
- 音频测试时有音量反应（需要 K10 端发送数据）

---

## 下一步：K10 端 ESP-NOW 发送

Dongle 端验证成功后，需要开发 K10 端固件：

1. **复制 `examples/51_mic_wifi` 为 `examples/51_mic_espnow`**
2. **替换 WiFi UDP 发送为 ESP-NOW 发送**:
   - 移除 `WiFiUDP` 相关代码
   - 添加 `esp_now_send()` 调用
   - 使用 `espnow_protocol.h` 定义的数据包格式
3. **配置 Dongle MAC 地址**（在 `config.h` 中已定义）
4. **测试端到端音频传输**

---

**当前状态**: 等待 Dongle 固件烧录完成...

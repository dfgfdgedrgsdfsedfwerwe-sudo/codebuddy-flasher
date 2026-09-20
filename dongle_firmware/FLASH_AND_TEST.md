# CodeBuddy Dongle 固件烧录与测试指南

## 1. 烧录固件

### 1.1 确认编译成功

检查生成的固件文件：
```bash
ls -lh build/*.bin
```

应该看到：
- `build/codebuddy_dongle.bin` - 主固件
- `build/bootloader/bootloader.bin` - Bootloader
- `build/partition_table/partition-table.bin` - 分区表

### 1.2 连接 ESP32-S3 开发板

1. 使用 USB 数据线连接 ESP32-S3-WROOM-1-N16R8 开发板到 PC
2. 记录串口号（Windows: `COM3`/`COM4`/...，Linux: `/dev/ttyUSB0`/...）

### 1.3 烧录命令

进入 ESP-IDF 环境并烧录：

**Windows (PowerShell):**
```powershell
cd C:\Users\4090\Desktop\dfk10_arduino_demo-master\dongle_firmware
& C:\Users\4090\esp\esp-idf\export.ps1
idf.py -p COM_PORT flash monitor
```

**Linux / macOS:**
```bash
cd /path/to/dongle_firmware
. ~/esp/esp-idf/export.sh
idf.py -p /dev/ttyUSB0 flash monitor
```

替换 `COM_PORT` 为实际串口号。

### 1.4 预期串口输出

烧录成功后，monitor 应该显示：

```
I (xxx) main: CodeBuddy Dongle Firmware Starting...
I (xxx) main: Chip: esp32s3
I (xxx) main: USB VID:PID = 0xCAFE:0x4011
I (xxx) main: Audio ringbuf initialized: 65536 bytes
I (xxx) main: Initializing USB PHY for OTG Device mode...
I (xxx) main: USB PHY initialized successfully
I (xxx) main: Initializing TinyUSB...
I (xxx) main: Initializing ESP-NOW receiver...
I (xxx) main: WiFi initialized for ESP-NOW
I (xxx) main: ESP-NOW initialized
I (xxx) main: USB task started
I (xxx) main: Dongle ready. Waiting for ESP-NOW frames from K10 device...
```

## 2. Windows 设备识别测试

### 2.1 检查 USB 设备枚举

烧录完成后，**重新插拔 USB 线**，然后打开 PowerShell：

```powershell
# 检查 USB 设备
Get-PnpDevice | Where-Object { $_.FriendlyName -like "*CodeBuddy*" -or $_.FriendlyName -like "*CAFE*" }
```

**预期结果：**

应该看到两个设备：
1. **CodeBuddy Dongle Keyboard** (USB HID)
   - VID: CAFE
   - PID: 4011
   - 设备类型: HID (键盘)
   - 状态: OK

2. **CodeBuddy Mic** (USB Audio)
   - VID: CAFE
   - PID: 4011
   - 设备类型: Audio (麦克风)
   - 状态: OK

### 2.2 设备管理器检查

打开设备管理器 (`devmgmt.msc`)，检查：

1. **人体学输入设备** 或 **HID 兼容设备**
   - 应该有 "CodeBuddy Dongle Keyboard" 或显示为 "USB 输入设备"

2. **音频输入和输出**
   - 应该有 "CodeBuddy Mic" 或显示为 "USB 音频设备"

### 2.3 声音设置检查

1. 打开 **设置 → 系统 → 声音**
2. 在 **输入** 部分，应该看到 **CodeBuddy Mic** 或 **USB 音频设备**
3. 选择该设备作为默认输入
4. 对着 K10 麦克风说话（需要 K10 端固件配合），观察音量条是否有波动

## 3. 问题排查

### 3.1 设备仍显示为 VID_303A&PID_1001

**原因:** USB PHY 没有正确初始化，USB-Serial/JTAG 仍在接管 USB 引脚。

**解决方案:**
1. 检查 `main.c` 中 `usb_new_phy()` 是否成功调用
2. 检查串口日志中的 "USB PHY initialized successfully"
3. 确保 `sdkconfig` 中 `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=n`
4. 清理重新构建：
   ```bash
   idf.py fullclean
   idf.py build
   ```

### 3.2 USB 设备无法识别或黄色感叹号

**可能原因:**
- 描述符有错误
- USB PHY 时钟配置问题
- Windows 驱动问题

**解决方案:**
1. 检查串口日志是否有 TinyUSB 错误
2. 尝试在另一台 PC 上测试
3. 使用 USBTreeView 工具查看原始描述符
4. 检查 `usb_descriptors.c` 中的描述符是否符合 USB 规范

### 3.3 USB 枚举成功但无音频

**原因:** ESP-NOW 还未接收到 K10 的音频数据。

**解决方案:**
1. 确认 K10 端固件已修改为 ESP-NOW 发送模式
2. 检查 K10 和 Dongle 的 MAC 地址配置是否匹配
3. 观察 Dongle 串口日志中是否有 "ESP-NOW: received audio packet" 消息

### 3.4 HID 按键无响应

**原因:** K10 端未发送按键控制包，或 Dongle 端 `on_key_frame()` 回调未触发。

**解决方案:**
1. 在 K10 端按下按钮
2. 检查 Dongle 串口日志中的 "Key event: ..." 消息
3. 使用键盘测试工具（如 KeyboardTester）验证 F2/Enter 键是否发送

## 4. 性能测试

### 4.1 延迟测试

1. 在 PC 上打开 Audacity 或其他录音软件
2. 选择 "CodeBuddy Mic" 作为输入设备
3. 开始录音
4. 对着 K10 麦克风说话或播放声音
5. 停止录音，观察波形是否清晰，延迟是否可接受（应 < 100ms）

### 4.2 丢包测试

观察串口日志中的统计信息（如果启用了 `DEBUG_STATS`）：
- 接收帧数
- 丢弃帧数
- 缓冲区溢出次数

理想情况下，丢包率应 < 1%。

## 5. 下一步：K10 端集成

Dongle 端固件验证通过后，需要修改 K10 端固件：

1. 从 `examples/51_mic_wifi` 复制到 `examples/51_mic_espnow`
2. 移除 WiFi AP 连接代码
3. 添加 ESP-NOW 初始化和 peer 注册（Dongle MAC 地址）
4. 将 UDP 发送替换为 ESP-NOW 发送
5. 测试 K10 → Dongle 的无线音频传输链路

详细步骤见主 CodeBuddy 文档。

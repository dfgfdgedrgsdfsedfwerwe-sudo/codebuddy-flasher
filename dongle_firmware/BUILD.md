# CodeBuddy Dongle 固件编译说明

## 前置条件

1. **安装 ESP-IDF**
   - 推荐版本：ESP-IDF 5.0 或更高
   - 设置环境变量：`. $HOME/esp/esp-idf/export.sh` (Linux/Mac) 或运行 `export.bat` (Windows)

2. **硬件**
   - ESP32-S3-WROOM-1-N16R8 模组
   - USB Type-C 连接线

## 编译步骤

```bash
cd dongle_firmware

# 1. 设置目标芯片
idf.py set-target esp32s3

# 2. (可选) 配置 - 使用默认配置通常不需要手动配置
# idf.py menuconfig

# 3. 编译
idf.py build

# 4. 烧录 (替换 COM_PORT 为实际端口，如 COM3 或 /dev/ttyUSB0)
idf.py -p COM_PORT flash

# 5. 查看串口输出 (获取 Dongle MAC 地址)
idf.py -p COM_PORT monitor

# 退出 monitor: Ctrl + ]
```

## 一键编译和烧录

```bash
idf.py -p COM_PORT flash monitor
```

## 获取 MAC 地址

烧录后，串口会输出 Dongle 的 MAC 地址：

```
I (xxx) espnow_rx: Dongle MAC: AA:BB:CC:DD:EE:FF
```

**将此 MAC 地址填入 K10 设备端的配置文件**，以建立配对。

## 配置修改

编辑 `main/config.h`：

- `ESPNOW_CHANNEL`: WiFi 信道（必须与 K10 一致，默认 6）
- `PEER_MAC_ADDR`: K10 设备端的 MAC 地址（烧录 K10 后从串口读取）
- `AUDIO_RINGBUF_SIZE`: 音频缓冲区大小（默认 2KB）

修改后需重新编译烧录。

## 验证

1. **设备管理器** (Windows) 或 `lsusb` (Linux) 应看到：
   - USB HID 键盘设备
   - USB 音频输入设备 (麦克风)

2. **按键测试**：K10 按下按键 → 电脑收到对应按键
3. **音频测试**：对着 K10 麦克风说话 → 电脑录音软件能捕获声音

## 调试

- 查看统计信息：串口每秒输出接收统计（音频帧数、按键帧数、丢包数等）
- 调整日志级别：修改 `sdkconfig.defaults` 中的 `CONFIG_LOG_DEFAULT_LEVEL_*`

## 故障排查

| 问题 | 可能原因 | 解决方案 |
|------|---------|---------|
| 电脑无法识别 USB 设备 | USB 描述符错误 | 检查 `usb_descriptors.c` 配置 |
| 收不到 ESP-NOW 数据 | 信道不一致 | 确保两端 `ESPNOW_CHANNEL` 相同 |
| 音频断续 | 缓冲区太小 | 增大 `AUDIO_RINGBUF_SIZE` |
| 按键无响应 | MAC 地址未配对 | 检查 `PEER_MAC_ADDR` 配置 |
| 编译错误 | ESP-IDF 版本过低 | 升级到 ESP-IDF 5.0+ |

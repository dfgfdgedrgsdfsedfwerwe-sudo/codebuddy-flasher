# K10 ESP-NOW 发送端调试报告 (Agent B)

## 执行时间
2026-08-20

## 任务状态
⚠️ **部分完成** - 固件已刷入，但串口通信中断

## 完成的工作

### 1. ESP-NOW 发送端代码创建 ✅
- 创建了 `espnow_protocol.h` - 与 Dongle 共享的协议定义
- 创建了 `51_mic_espnow.ino` - ESP-NOW 音频发送固件
- 重命名旧的 UDP 版本 (`51_mic_wifi.ino.bak`) 避免编译冲突

### 2. 固件配置 ✅
**ESP-NOW 参数:**
- WiFi 信道: **1** (与 Dongle 一致)
- Dongle MAC 地址: `e0:72:a1:d4:8f:e0`
- 音频格式: 16kHz, 16bit, mono
- 帧大小: 64 samples (128 bytes) per frame
- 协议: 使用 `audio_frame_t` 结构 + CRC8 校验

**关键功能:**
- I2S 麦克风采集 (ES7243E 初始化)
- ESP-NOW peer 管理
- 按键 A (P5) 控制录音开始/停止
- LCD 实时显示: ESP-NOW 状态、发送计数、失败率
- 发送回调监控成功/失败率

### 3. 固件编译和刷写 ✅
```
Platform: ESP32-S3
Flash: 812160 bytes (17.2%)
RAM: 34988 bytes (10.7%)
Upload: COM4 @ 921600 baud
Status: SUCCESS (37.51 seconds)
```

## 遇到的问题

### ❌ 串口通信中断
**现象:**
- 固件刷写成功后，COM4 端口消失
- 设备仍然在 USB 总线上可见: `USB\VID_303A&PID_1001\3CDC756D7FB4`
- 但没有枚举为 CDC 串口

**可能原因 (来自 CodeBuddy 文档的已知坑):**
1. **CDC 供电阻塞问题**: 文档中提到 `51_mic_wifi.ino` 的 `Serial.flush()` 在独立供电时会卡死
2. **固件卡死/异常状态**: 文档提到过"板子在 USB 上不再枚举(COM 口消失)"的情况
3. **ESP-NOW 初始化失败**: 可能在 `setup()` 中的某个步骤卡住

**文档原文引用:**
> "后续遇到板子在 USB 上也不再枚举(COM 口消失):疑似固件卡死/进入异常状态,需 BOOT+RESET 强制下载模式救砖。"

## 代码关键点

### ESP-NOW 初始化流程 (51_mic_espnow.ino)
```cpp
setup() {
    init_board()           // XL95x5 扩展芯片 + 背光
    tft.init()             // LCD 初始化
    i2c_scan(47, 48)       // 扫描 I2C (找 ES7243E)
    es7243e_init(0x11)     // 麦克风 ADC 配置
    espnow_init()          // ESP-NOW + peer 配置
    mic_i2s_init()         // I2S 驱动
}

espnow_init() {
    WiFi.mode(WIFI_STA)
    esp_wifi_set_channel(1)     // 设置信道 1
    esp_now_init()
    esp_now_add_peer(e0:72:a1:d4:8f:e0)  // 添加 Dongle
}
```

### 音频发送循环
```cpp
loop() {
    if (streaming && espnow_ready) {
        i2s_read() → stereoBuf[]
        提取单声道 → monoBuf[]
        构建 audio_frame_t (seq_num, timestamp, CRC8)
        esp_now_send(dongle_mac, &frame)
        更新 TX/FAIL 计数
    }
}
```

## 下一步建议

### 立即行动 (救砖)
1. **物理复位 K10**: 
   - 按住 BOOT 按钮
   - 短按 RESET
   - 松开 BOOT
   - 进入下载模式
2. **重新刷写固件** (可选择恢复 50_codebuddy 有线版本先验证硬件)

### 代码修改 (避免 CDC 卡死)
在 `51_mic_espnow.ino` 的 `setup()` 中:
```cpp
// 当前: delay(1000)
// 改为: delay(3000)  // 更长的 CDC 稳定时间

// 移除所有 Serial.flush() 调用
// 或添加超时保护
```

### 验证步骤 (重新刷写后)
1. 观察 LCD 是否点亮并显示状态
2. 串口监视器查看:
   - "ESP-NOW init OK"
   - "Peer added: e0:72:a1:d4:8f:e0"
   - K10 MAC 地址
3. 按键 A 启动录音，观察:
   - LCD "REC *" 指示
   - TX 计数递增
   - DIAG 日志显示音频 peak/rms

### 与 Dongle 联调 (Agent A 协作)
当 K10 恢复正常后:
1. 确认 K10 和 Dongle 都在**信道 1**
2. 确认 Dongle 已添加 K10 MAC (3CDC756D7FB4) 为 peer
3. 同时监视两端串口:
   - K10: "TX=xxx FAIL=0"
   - Dongle: "ESP-NOW recv: seq=xxx"
4. 触发音频: 对 K10 麦克风说话，验证 Dongle 是否收到并转发到 USB Audio

## 文件位置
- K10 固件: `C:\Users\4090\Desktop\dfk10_arduino_demo-master\examples\51_mic_wifi\51_mic_espnow.ino`
- 协议定义: `C:\Users\4090\Desktop\dfk10_arduino_demo-master\examples\51_mic_wifi\espnow_protocol.h`
- 旧 UDP 版本: `C:\Users\4090\Desktop\dfk10_arduino_demo-master\examples\51_mic_wifi\51_mic_wifi.ino.bak`

## K10 硬件信息
- MAC 地址: `3c:dc:75:6d:7f:b4`
- 芯片: ESP32-S3 (revision v0.2)
- I2S 麦克风: ES7243E @ I2C 0x11
- 按键: P5 (eP5_KeyA, 低电平有效)

---
**Agent B 当前状态**: 等待 K10 硬件复位并重新建立串口连接
**需要用户操作**: 物理复位 K10 板子 (BOOT + RESET)

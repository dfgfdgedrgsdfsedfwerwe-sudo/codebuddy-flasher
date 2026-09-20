# FluidBox 烧录故障排查指南

## 当前状态
✅ 固件编译成功 (RAM: 50.5%, Flash: 7.2%)  
❌ 设备连接失败 (COM3无法通信)

## 问题原因
首次烧录的固件存在I2C初始化bug，导致设备陷入**无限重启循环**。设备可能：
1. 仍在崩溃重启，USB驱动异常
2. USB完全挂死，需要重新插拔
3. 进入下载模式失败

## 已修复的Bug
原始代码：
```cpp
Wire.begin(48, 45);           // 初始化Wire
xl9555.begin(Wire);           // ❌ Wire被初始化两次，导致NULL TX buffer
```

修复后：
```cpp
xl9555.begin();               // ✅ 内部自动调用Wire.begin(48, 45)
```

## 烧录步骤

### 方案1: 手动进入下载模式 (推荐)
1. **拔掉USB线，等5秒，重新插入**
2. 打开PowerShell，检测端口：
   ```powershell
   [System.IO.Ports.SerialPort]::GetPortNames()
   ```
3. **按住BOOT按钮不放**
4. **按一下RESET按钮**
5. **松开BOOT按钮**
6. 立即烧录（替换COMX为实际端口）：
   ```powershell
   cd C:\Users\4090\Desktop\dfk10_arduino_demo-master
   python -m platformio run -t upload -e 53_esp32-fluidbox --upload-port COMX
   ```

### 方案2: 使用Web Flasher
1. 打开 `web_flasher/` 目录
2. 运行：
   ```powershell
   cd C:\Users\4090\Desktop\dfk10_arduino_demo-master\web_flasher
   python serve.py
   ```
3. 浏览器打开 http://localhost:8000
4. 选择固件文件（在项目根目录）：
   ```
   .pio/build/53_esp32-fluidbox/firmware.bin
   ```
5. 手动进入下载模式（步骤同方案1）
6. 点击"开始写入"

### 方案3: ESP Flash Download Tool
如果上述方法都失败，使用乐鑫官方工具：
1. 下载 Flash Download Tool (https://www.espressif.com/en/support/download/other-tools)
2. 固件地址配置：
   - `0x0` → `bootloader.bin`
   - `0x8000` → `partitions.bin`
   - `0x10000` → `firmware.bin`
3. 所有bin文件在 `.pio/build/53_esp32-fluidbox/`

## 预期串口输出 (正常启动)
```
[     0][I][main.cpp:XXX] setup(): Starting FluidBox...
[    XX][I][atk_display.cpp:XXX] atk_display_init(): Display initialized
[    XX][I][atk_display.cpp:XXX] atk_display_init(): DMA buffer allocated (8 strips)
[    XX][I][53_esp32-fluidbox.ino:XXX] setup(): Simulation initialized (80x106 grid)
```

## 串口监控命令
烧录成功后，监控输出：
```powershell
python -m platformio device monitor -p COMX -b 115200
```

按 `Ctrl+C` 退出监控。

## 技术细节
- **固件路径**: `.pio/build/53_esp32-fluidbox/firmware.bin` (341KB)
- **波特率**: 115200
- **芯片**: ESP32-S3 (16MB Flash, 8MB PSRAM)
- **显示**: 320x240 ILI9341 (8080并口)
- **物理引擎**: 80×106粒子, ~30 FPS

## 下一步
成功烧录后，LCD应显示：
- 彩色流体场（蓝→绿→红渐变）
- 重力场自动旋转（0.5°/帧）
- 粒子随重力方向流动

如果画面静止或黑屏，检查串口输出是否有错误。

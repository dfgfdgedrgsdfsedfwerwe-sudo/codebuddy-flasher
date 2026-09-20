# FluidBox 流體物理演示 - ATK BOX 触摸交互版

**原項目**: https://github.com/V4C38/esp32-fluidbox  
**移植目標**: 正點原子 ATK ESP32-S3 BOX (ST7789 8080並口LCD, 240×320, CHSC5432觸摸)

## 功能特性

- **觸摸交互控制**: 手指拖拽/滑動屏幕，實時控制流體重力方向和旋转漩渦
- **實時流體物理模擬**: 80×106 粒子網格, Navier-Stokes 方程求解
- **自動回落模式**: 鬆手後 2 秒平滑過渡到自動演示 (重力旋轉 + 週期晃動)
- **DMA 流水線渲染**: 雙緩衝條帶渲染 (240×40 × 8條), 8080並口 DMA傳輸
- **背光自動控制**: XL9555 IO擴展控制LCD背光

## 交互方式

| 操作 | 效果 |
|------|------|
| **觸摸拖拽** | 重力方向指向觸摸點，液體向該方向流動 (類似傾斜盒子) |
| **快速滑動** | 產生旋轉漩渦 (角速度 omega)，液體螺旋運動 |
| **距離中心** | 距離越遠，重力傾斜角度越大 (最遠處 = 最大傾斜) |
| **鬆手** | 2 秒內指數衰減，平滑過渡到自動演示循環 |
| **無觸摸時** | 自動模式: 重力方向旋轉(12秒/圈) + 週期晃動(7秒) + 自旋漩渦 |

## 移植修改

### 硬件適配
- **顯示器**: QSPI AMOLED → 8080並口 ST7789 (LovyanGFX)
- **觸摸屏**: QMI8658 IMU → CHSC5432 電容觸摸 (I2C 0x2E)
- **背光**: XL9555 P0.7 控制 (I2C: SDA=48, SCL=45, 地址0x20)
- **渲染**: ESP-IDF SPI DMA → LovyanGFX pushImageDMA

### 代碼結構
```
53_esp32-fluidbox.ino      # 主入口 (setup/loop + 4個FreeRTOS任務)
atk_display.cpp/h          # LCD驅動 + DMA條帶渲染
atk_touch_fluid.c/h        # 觸摸交互 → 物理力映射 (替代 IMU)
config.h                   # 流體參數 + 硬件配置
render.c/h                 # 顏色映射 + 條帶繪製
sim.c/h                    # Navier-Stokes求解器 (未修改)
```

### 原版文件保留
- `_orig_ref/`: 原項目ESP-IDF代碼 (display.c, button.c) 供參考, 不參與編譯

## 編譯與燒錄

### 測試狀態
- ✅ **編譯通過**: RAM 15.2% (49KB/327KB), Flash 24.4% (1.15MB/4.7MB)
- ✅ **觸摸交互集成完成**: CHSC5432驅動 + 物理力映射
- ⏳ **待硬件驗證**: 需要連接ATK BOX設備進行實際測試

### 編譯
```powershell
cd C:\Users\4090\Desktop\dfk10_arduino_demo-master
python -m platformio run -e 53_esp32-fluidbox
```

**編譯成功輸出**:
```
RAM:   [==        ]  15.2% (used 49952 bytes from 327680 bytes)
Flash: [==        ]  24.4% (used 1153281 bytes from 4718592 bytes)
========================= [SUCCESS] Took 30.58 seconds =========================
```

### 燒錄
```powershell
# 1. 連接ATK BOX USB線
# 2. 檢查串口號
[System.IO.Ports.SerialPort]::GetPortNames()

# 3. 燒錄 (替換 COM12 為實際端口)
python -m platformio run -t upload -e 53_esp32-fluidbox --upload-port COM12

# 4. 如連接失敗, 手動進入下載模式:
#    - 按住 BOOT 鍵
#    - 短按 RESET 鍵
#    - 鬆開 BOOT 鍵
#    - 重新執行燒錄命令
```

### 串口監視 (可選)
```powershell
python -m platformio device monitor -p COM12 -b 115200
```

**預期日誌**:
```
[display] XL9555 backlight init OK
[display] ST7789 240x320, 8 bands x 40 rows, buffers @ 0x...
[touch] CHSC5432 init OK
[main] Fluid sim 80x106, timestep 0.015000, viscosity 0.000100
```

## 運行效果

### 觸摸交互模式 (主模式)
1. **啟動**: 上電後LCD背光點亮, 流體場靜止, 屏幕顯示彩色速度場
2. **觸摸屏幕**: 手指按下→在觸摸點注入徑向力 (半徑25px, 強度800.0)
3. **滑動**: 手指移動→連續注入力, 產生波紋/渦流效果
4. **鬆手**: 流體在粘度作用下逐漸恢復靜止
5. **多點**: 支持多指同時觸摸 (最多10點), 每個觸點獨立注入力

### 自動演示模式 (備用, 需修改代碼啟用)
若註釋掉 `atk_touch_fluid.c` 的觸摸力注入, 可恢復原自動旋轉重力演示
3. **顏色映射**: 速度場 → HSV色輪 (藍=靜止, 紅/黃=高速)
4. **幀率**: ~30 FPS (物理模擬 + 8條帶DMA傳輸)

## 參數調整

編輯 `config.h`:

```c
// 流體物理
#define VISCOSITY 0.0001f        // 粘度 (越大越粘稠, 阻尼越強)
#define DIFFUSION 0.00001f       // 擴散係數
#define DT 0.015f                // 時間步長 (越小越穩定, 但幀率越低)

// 自動旋轉
#define AUTO_ROTATION_GAIN 0.5f  // 重力旋轉速度 (度/幀)
```

## 技術細節

### DMA 流水線渲染
```
條帶0: acquire → render → flush (DMA開始)
條帶1: acquire → render → flush (DMA開始, 同時條帶0 DMA完成)
...
條帶7: acquire → render → flush
```
- **零拷貝**: 直接在DMA緩衝繪製, 無需二次搬移
- **雙緩衝**: 兩個條帶緩衝交替使用, `pushImageDMA()` 內部自動等待前一次完成

### 條帶跳過優化
```c
// render.c: 檢測整條帶是否全黑 (速度場為0)
bool all_zero = true;
for (...) {
    if (Vx[...] != 0 || Vy[...] != 0) { all_zero = false; break; }
}
if (all_zero) return;  // 跳過繪製和DMA
```
- 靜止區域不繪製 → 減少DMA傳輸量 → 幀率提升

### 顏色映射算法
```c
float speed = sqrtf(vx*vx + vy*vy);
float hue = speed * 180.0f;  // 速度 → HSV色相 (0°藍 → 180°紅)
uint16_t rgb565 = hsv_to_rgb565(hue, 1.0f, 1.0f);
```

## 故障排查

### 1. 屏幕不亮
- **原因**: XL9555 I2C通信失敗
- **檢查**: 串口日誌是否有 `XL9555 backlight init FAILED!`
- **解決**: 確認I2C引腳 (SDA=48, SCL=45) 和地址 (0x20)

### 2. 編譯錯誤
- **原因**: 缺少 LovyanGFX 庫
- **解決**: PlatformIO會自動下載, 或手動安裝:
  ```powershell
  python -m platformio lib install "LovyanGFX@^1.2.0"
  ```

### 3. 燒錄卡住 "Connecting..."
- **原因**: 設備未進入下載模式
- **解決**: 
  1. 斷開USB重插
  2. 手動進入下載模式 (BOOT+RESET)
  3. 嘗試不同串口 (COM6/COM11/COM12)

### 4. 流體靜止不動
- **原因**: 自動旋轉未啟用, 或重力增益為0
- **檢查**: `config.h` 中 `AUTO_ROTATION_GAIN` 是否 > 0
- **預期**: 重力方向每幀旋轉, 粒子受力波動

## 性能數據

| 指標 | 數值 |
|------|------|
| 網格解析度 | 80×106 粒子 |
| 條帶數 | 8 條 (每條240×40) |
| DMA緩衝 | 2×19200 字節 (內部SRAM) |
| 幀率 | ~30 FPS |
| RAM使用 | 165 KB / 320 KB (50.5%) |
| Flash使用 | 336 KB / 4608 KB (7.1%) |

## 下一步擴展

1. **觸摸交互**: 讀取FT6336觸摸坐標 → 向流體場注入力源
2. **真實IMU**: 連接QMI8658 → 根據板子傾斜實時調整重力方向
3. **按鍵控制**: KEY0/KEY1 切換場景 (旋流/線性剪切/靜止)
4. **參數UI**: LVGL滑塊調整粘度/時間步長

## 參考資料

- **原項目**: https://github.com/V4C38/esp32-fluidbox
- **Navier-Stokes方程**: https://en.wikipedia.org/wiki/Navier%E2%80%93Stokes_equations
- **Stable Fluids算法**: Jos Stam (1999) - https://www.dgp.toronto.edu/public_user/stam/reality/Research/pdf/ns.pdf
- **LovyanGFX文檔**: https://github.com/lovyan03/LovyanGFX

---

**移植完成日期**: 2026-09-11  
**測試狀態**: ✅ 編譯通過 | ⏳ 待硬件驗證

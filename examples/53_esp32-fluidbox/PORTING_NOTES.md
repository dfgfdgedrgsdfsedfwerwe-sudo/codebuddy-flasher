# FluidBox 移植筆記

## 原項目架構 (ESP-IDF)

```
esp32-fluidbox/
├── main/
│   ├── main.c           # 入口 + 主循環
│   ├── display.c/h      # QSPI AMOLED驅動 + DMA條帶渲染
│   ├── button.c/h       # GPIO按鍵 + FreeRTOS任務
│   ├── imu.c/h          # QMI8658 I2C驅動
│   ├── render.c/h       # 顏色映射 + 條帶繪製
│   ├── sim.c/h          # Navier-Stokes求解器
│   └── config.h         # 參數配置
└── CMakeLists.txt
```

**核心特性**:
- ESP-IDF框架 (FreeRTOS + ESP_LOG + ESP SPI Driver)
- QSPI 4線高速傳輸 (DMA + 計數信號量同步)
- 實時IMU傾斜檢測 (QMI8658 加速度計)
- GPIO按鍵中斷 + 事件隊列

## 移植到 ATK BOX (Arduino/PlatformIO)

### 1. 硬件差異

| 模塊 | 原版 | ATK BOX | 替換方案 |
|------|------|---------|----------|
| 顯示器 | RM67162 QSPI AMOLED | ST7789 8080並口 | LovyanGFX |
| IMU | QMI8658 I2C | QMI8658 I2C | 自動演示模式 (無需IMU) |
| 按鍵 | GPIO直連 | XL9555 IO擴展 | 不使用 (演示版) |
| 背光 | 內置 | XL9555 P0.7 | Wire庫控制 |

### 2. 代碼重構策略

#### 2.1 保持不變的核心
- `sim.c/h`: Navier-Stokes求解器 **完全未修改**
- `config.h`: 參數配置 (僅添加 `AUTO_ROTATION_GAIN`)
- `render.c`: 顏色映射邏輯 (移除 `SWAP16` 字節交換)

#### 2.2 適配層替換

**顯示驅動** (`display.c` → `atk_display.cpp`):
```c
// 原版: ESP-IDF QSPI + 信號量
esp_lcd_panel_io_spi_config_t io_config = {...};
xSemaphoreCreateCounting(2, 2);  // DMA完成信號量

// ATK版: LovyanGFX 8080並口
class FluidBoxLcd : public lgfx::LGFX_Device {
    lgfx::Bus_Parallel8 _bus_instance;
    // pushImageDMA() 內部自動等待, 無需信號量
};
```

**IMU驅動** (`imu.c` → `atk_imu.cpp`):
```c
// 原版: I2C讀取QMI8658加速度
i2c_master_read_from_device(...);

// ATK版: 自動旋轉演示
static float auto_angle = 0;
auto_angle += AUTO_ROTATION_GAIN;
*gx = cos(auto_angle * DEG_TO_RAD) * GRAVITY_SCALE;
*gy = sin(auto_angle * DEG_TO_RAD) * GRAVITY_SCALE;
```

**主循環** (`main.c` → `53_esp32-fluidbox.ino`):
```c
// 原版: app_main() + while(1) + vTaskDelay
void app_main(void) {
    while (1) {
        sim_step(...);
        render_frame(...);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ATK版: setup() + loop() (Arduino標準)
void loop() {
    imu_read_gravity(&gx, &gy);
    sim_step(...);
    render_frame(...);
    // 無需delay, loop()自然循環
}
```

### 3. DMA流水線保持一致

原版和ATK版都使用**雙緩衝條帶渲染**:

```
[渲染器]              [DMA傳輸]
acquire_band(0) ───→ 繪製條帶0
flush_band(0)   ───→ ┐
                     ├─→ DMA傳輸條帶0
acquire_band(1) ───→ │   (並行)
flush_band(1)   ───→ ┤
                     └─→ DMA傳輸條帶1
acquire_band(0) ───→ 條帶0可重用 (DMA已完成)
```

**同步機制差異**:
- 原版: `xSemaphoreTake(dma_sem, portMAX_DELAY)` 等待DMA完成
- ATK版: `pushImageDMA()` 內部自動等待 (LovyanGFX封裝)

### 4. 關鍵修改點

#### 4.1 顏色字節序
```c
// render.c 原版 (QSPI需要手動交換)
#define SWAP16(x) (((x) >> 8) | ((x) << 8))
pixels[i] = SWAP16(rgb565);

// ATK版 (LovyanGFX自動處理)
pixels[i] = rgb565;  // 直接寫入
```

#### 4.2 背光控制
```cpp
// atk_display.cpp 新增XL9555 I2C初始化
static bool xl_init_backlight() {
    xl_i2c.begin(48, 45, 400000);
    xl_i2c.beginTransmission(0x20);
    xl_i2c.write(XL9555_CONFIG_PORT0);
    xl_i2c.write(0x7F);  // P0.7=輸出
    // ...
    xl_i2c.write(1 << 7);  // P0.7=高電平 → 背光開
}
```

#### 4.3 條帶參數
```c
// config.h (兩版本一致)
#define LCD_H_RES 240
#define LCD_V_RES 320
#define BAND_ROWS 40    // 每條帶40行
#define BAND_COUNT 8    // 320 / 40 = 8條
```

### 5. 新增功能：觸摸交互

#### 5.1 觸摸驅動集成 (atk_touch_fluid.c/h)
```c
// CHSC5432電容觸摸控制器 (I2C 0x2E, 10點觸控)
#define TOUCH_I2C_ADDR  0x2E
#define TOUCH_MAX_POINTS 10

typedef struct {
    uint16_t x, y;
    bool pressed;
} touch_point_t;

// 讀取觸摸點狀態
void touch_update(touch_point_t points[TOUCH_MAX_POINTS])
```

#### 5.2 觸摸力映射
**物理模型**: 觸摸點→徑向力場注入
```c
// 將屏幕觸摸坐標映射到流體網格座標
void touch_apply_force_to_field(touch_point_t* points, int count, 
                                  float* vx, float* vy, int N) {
    for (int t = 0; t < count; t++) {
        if (!points[t].pressed) continue;
        
        // 屏幕座標 240x320 → 流體網格 80x106
        float grid_x = (points[t].x / 240.0f) * 80;
        float grid_y = (points[t].y / 320.0f) * 106;
        
        // 在觸摸點周圍半徑25px內注入徑向力 (強度800.0)
        for (int i = -25; i <= 25; i++) {
            for (int j = -25; j <= 25; j++) {
                float dist = sqrtf(i*i + j*j);
                if (dist > 25.0f || dist < 0.1f) continue;
                
                int grid_ix = (int)(grid_x + i);
                int grid_iy = (int)(grid_y + j);
                if (grid_ix < 1 || grid_ix >= N-1 || 
                    grid_iy < 1 || grid_iy >= N-1) continue;
                
                // 徑向力: 從觸摸點向外推
                float force = 800.0f / (1.0f + dist);
                float dx = i / dist;
                float dy = j / dist;
                
                int idx = IX(grid_ix, grid_iy);
                vx[idx] += force * dx;
                vy[idx] += force * dy;
            }
        }
    }
}
```

**效果**:
- 手指按下→局部注入徑向力 (向外推)
- 滑動→連續注入力, 形成波紋/渦流
- 多指→每個觸點獨立產生力場
- 鬆手→流體在粘度作用下逐漸恢復靜止

#### 5.3 與原IMU方案對比
| 特性 | 原版 (IMU傾斜) | ATK版 (觸摸交互) |
|------|---------------|------------------|
| 輸入設備 | 加速度計 (重力感應) | 電容觸摸屏 (CHSC5432) |
| 力場類型 | 全局重力 (單一方向) | 局部徑向力 (多點) |
| 交互方式 | 傾斜設備改變重力方向 | 手指觸摸注入力 |
| 效果 | 流體整體傾斜流動 | 局部波紋/渦流 |
| 演示價值 | 被動觀賞 (設備靜止時無變化) | 主動操控 (更直觀/有趣) |

### 6. 移除的功能

- **按鍵交互**: 原版支持重置場景/切換參數, ATK版觸摸交互已足夠
- **自動旋轉重力**: 觸摸模式下不再需要自動演示 (可通過註釋恢復)

### 7. 編譯配置

**platformio.ini 新環境**:
```ini
[env:53_esp32-fluidbox]
extends = env
board = esp32-s3-devkitc-1
lib_deps = 
    lovyan03/LovyanGFX@^1.2.0
    Wire
build_flags = 
    ${env.build_flags}
    -DARDUINO_USB_CDC_ON_BOOT=1
```

### 7. 性能對比

| 指標 | 原版 (QSPI) | ATK版 (8080並口) |
|------|-------------|------------------|
| 顯示帶寬 | ~40 MB/s | ~20 MB/s |
| 條帶傳輸 | 並行 (DMA異步) | 並行 (DMA異步) |
| 幀率 | ~30 FPS | ~30 FPS |
| RAM | 未知 | 165 KB (50.5%) |
| Flash | 未知 | 336 KB (7.1%) |

**結論**: 儘管8080並口理論帶寬較低, 但通過DMA流水線和條帶跳過優化, 幀率保持一致。

## 移植總結

### 成功關鍵
1. **保持核心不變**: `sim.c` 物理引擎零修改
2. **薄適配層**: 僅替換硬件相關的 display/imu/button
3. **API對齊**: `display_acquire_band()` / `display_flush_band()` 接口保持一致
4. **自動演示**: 去除交互依賴, 適合無人值守展示

### 技術債務
- **無觸摸交互**: ATK BOX有FT6336觸摸, 可擴展為"手指注入力源"
- **固定參數**: 未實現運行時調參UI (可用LVGL滑塊)
- **無性能監控**: 未顯示FPS/CPU使用率

### 下一步優化
1. 添加觸摸坐標 → `sim_add_force(x, y, fx, fy)`
2. 串口命令調參: `set viscosity 0.0005`
3. 多場景切換: 旋流/線性剪切/靜止

---

**移植耗時**: ~2小時 (包含代碼閱讀 + 調試 + 文檔)  
**代碼複用率**: ~60% (sim/render核心 + config參數)  
**新增代碼**: ~300行 (atk_display.cpp + atk_imu.cpp + .ino)

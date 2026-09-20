/**
 * @file atk_display.cpp
 * @brief ATK BOX 显示驱动 (LovyanGFX 8080并口 ST7789, 替代原 display.c 的 QSPI AMOLED)
 *
 * 保持 FluidBox 的"条带 DMA 流水线"渲染模型:
 *   - 两个条带缓冲交替使用, 一个绘制时另一个 DMA 传输
 *   - display_acquire_band() 返回空闲缓冲, display_flush_band() 异步 DMA
 *
 * 与原版差异:
 *   - 原版用计数信号量 + QSPI ISR, 此处用 LovyanGFX 的 pushImageDMA
 *     (内部自动等待上一次 DMA 完成, 天然串行化, 无需信号量)
 *   - 原版颜色字节交换 (SWAP16), LovyanGFX 自动处理字节序, render.c 已改为不交换
 *   - XL9555 IO 扩展控制背光 (P0.7)
 */

#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <Wire.h>
#include "esp_heap_caps.h"

extern "C" {
    #include "config.h"
    #include "atk_display.h"
}

// XL9555 背光控制
#define XL9555_I2C_ADDR     0x20
#define XL9555_OUTPUT_PORT0 2
#define XL9555_CONFIG_PORT0 6
#define ATK_LCD_BL_BIT      7   // P0.7

static TwoWire xl_i2c(0);

static bool xl_init_backlight() {
    xl_i2c.begin(48, 45, 400000);  // SDA=48, SCL=45

    // P0.7 设为输出, 其他位保持默认
    xl_i2c.beginTransmission(XL9555_I2C_ADDR);
    xl_i2c.write(XL9555_CONFIG_PORT0);
    xl_i2c.write(0x7F);  // P0.7=0(输出), 其他=1(输入)
    if (xl_i2c.endTransmission() != 0) return false;

    // P0.7=1 打开背光
    xl_i2c.beginTransmission(XL9555_I2C_ADDR);
    xl_i2c.write(XL9555_OUTPUT_PORT0);
    xl_i2c.write(1 << ATK_LCD_BL_BIT);
    return xl_i2c.endTransmission() == 0;
}

// ATK BOX ST7789 8080并口驱动 (引脚见 AtkBoxGfx.hpp)
class FluidBoxLcd : public lgfx::LGFX_Device {
    lgfx::Panel_ST7789   _panel_instance;
    lgfx::Bus_Parallel8  _bus_instance;

public:
    FluidBoxLcd(void) {
        {   // 8080 并口总线
            auto cfg = _bus_instance.config();
            cfg.port = 0;
            cfg.freq_write = 20000000;  // 20MHz, 流体渲染需要带宽
            cfg.pin_wr = 42;
            cfg.pin_rd = 41;
            cfg.pin_rs = 2;   // DC
            cfg.pin_d0 = 40; cfg.pin_d1 = 39; cfg.pin_d2 = 38; cfg.pin_d3 = 12;
            cfg.pin_d4 = 11; cfg.pin_d5 = 10; cfg.pin_d6 = 9;  cfg.pin_d7 = 46;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }
        {   // 面板
            auto cfg = _panel_instance.config();
            cfg.pin_cs   = 1;
            cfg.pin_rst  = -1;
            cfg.pin_busy = -1;
            cfg.panel_width  = 240;
            cfg.panel_height = 320;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            cfg.offset_rotation = 0;
            cfg.readable   = false;
            cfg.invert     = true;   // ST7789 出厂反色
            cfg.rgb_order  = false;
            cfg.dlen_16bit = false;
            cfg.bus_shared = false;
            _panel_instance.config(cfg);
        }
        setPanel(&_panel_instance);
    }
};

static FluidBoxLcd s_tft;

#define BAND_PIXELS (LCD_H_RES * BAND_ROWS)

// 两个条带缓冲, 交替使用。分配在内部 DMA 可访问的 SRAM。
static uint16_t *s_band_buf[2];

void atk_display_init(void) {
    // 初始化 XL9555 背光控制
    if (!xl_init_backlight()) {
        Serial.println("[display] XL9555 backlight init FAILED!");
    }

    // 分配 DMA 缓冲 (内部 SRAM, 8080并口 GDMA 可直接访问)
    for (int i = 0; i < 2; i++) {
        s_band_buf[i] = (uint16_t *)heap_caps_malloc(
            BAND_PIXELS * sizeof(uint16_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
        if (!s_band_buf[i]) {
            Serial.printf("[display] band buffer %d alloc FAILED!\n", i);
        }
    }

    s_tft.init();
    s_tft.initDMA();
    s_tft.setRotation(0);
    s_tft.fillScreen(0x0000);  // 清黑

    // 打开写事务并保持: 并口总线为 LCD 独占, 不释放以支持连续 DMA 流水线
    s_tft.startWrite();

    Serial.printf("[display] ST7789 %dx%d, %d bands x %d rows, buffers @ %p/%p\n",
                  LCD_H_RES, LCD_V_RES, BAND_COUNT, BAND_ROWS,
                  s_band_buf[0], s_band_buf[1]);
}

uint16_t *display_acquire_band(void) {
    // 交替返回两个缓冲。由于 pushImageDMA 内部会等待上一次 DMA 完成,
    // 且同一总线 DMA 严格串行, 当再次拿到某缓冲时它的 DMA 必已结束。
    // (计数按获取次数而非条带号, 与原版一致: 渲染器会跳过空条带)
    static unsigned next = 0;
    return s_band_buf[next++ & 1];
}

void display_flush_band(int band_index, const uint16_t *buffer) {
    const int y0 = band_index * BAND_ROWS;
    // pushImageDMA: 异步传输, 内部自动等待上一次 DMA。返回后 CPU 可绘制下一条带。
    s_tft.pushImageDMA(0, y0, LCD_H_RES, BAND_ROWS, (const lgfx::rgb565_t *)buffer);
}

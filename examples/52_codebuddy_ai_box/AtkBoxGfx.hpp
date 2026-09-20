/**
 * @file AtkBoxGfx.hpp
 * @brief 正点原子 ESP32-S3 BOX (DNESP32S3B) 显示驱动
 *
 * 硬件: 2.4寸 320x240 TFT LCD (ST7789VW) — 8080 并口 8bit
 * 引脚 (来自 ATK_DNESP32S3B 原理图 / 官方例程 lcd.h):
 *   D0=40 D1=39 D2=38 D3=12 D4=11 D5=10 D6=9 D7=46
 *   WR=42  RD=41  DC=2  CS=1  RST=NC(悬空)
 *   背光 = XL9555 P0.7 (LCD_BL_IO 0x0080)
 *
 * 参考: K10 原驱动 lib/bsp_handler/Lcd_handler.hpp (Bus_SPI + Panel_ILI9341)
 * 移植说明: 仅替换总线类型 (Bus_Parallel8) 和面板类型 (Panel_ST7789)
 */
#ifndef __ATK_BOX_GFX_HPP__
#define __ATK_BOX_GFX_HPP__

#include <LovyanGFX.hpp>

class AtkBoxLcd : public lgfx::LGFX_Device {
    lgfx::Panel_ST7789   _panel_instance;
    lgfx::Bus_Parallel8  _bus_instance;   // 8080 8bit 并口总线

public:
    AtkBoxLcd(void) {
        { // 8080 并口总线设置
            auto cfg = _bus_instance.config();

            cfg.port = 0;              // 并口编号 (ESP32-S3 无 I2S 版本用 0)
            cfg.freq_write = 16000000; // 并口写时钟 (先保守, 稳定后可提 20M)
            cfg.pin_wr = 42;           // WR 写时钟
            cfg.pin_rd = 41;           // RD 读时钟
            cfg.pin_rs = 2;            // DC 数据/命令 (LovyanGFX 中叫 rs)

            cfg.pin_d0 = 40;
            cfg.pin_d1 = 39;
            cfg.pin_d2 = 38;
            cfg.pin_d3 = 12;
            cfg.pin_d4 = 11;
            cfg.pin_d5 = 10;
            cfg.pin_d6 = 9;
            cfg.pin_d7 = 46;

            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }
        { // 面板设置
            auto cfg = _panel_instance.config();

            cfg.pin_cs     = 1;    // CS 片选
            cfg.pin_rst    = -1;   // RST 悬空 (原理图 NC)
            cfg.pin_busy   = -1;

            cfg.panel_width  = 240;
            cfg.panel_height = 320;
            cfg.offset_x     = 0;
            cfg.offset_y     = 0;
            cfg.offset_rotation = 0;

            cfg.readable     = false;   // 并口屏 RD 接线存在但 ST7789 读时序慢, 先禁用
            cfg.invert       = true;    // ST7789 出厂默认反色, IP 或屏幕明暗反了改这里
            cfg.rgb_order    = false;   // 红蓝互换时改 true
            cfg.dlen_16bit   = false;
            cfg.bus_shared   = false;   // SD 卡走独立 SPI2, 与并口无共享

            _panel_instance.config(cfg);
        }
        setPanel(&_panel_instance);
    }
};

#endif // __ATK_BOX_GFX_HPP__

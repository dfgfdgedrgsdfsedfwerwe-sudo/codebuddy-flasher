#ifndef __LCD_HANDLER__H__
#define __LCD_HANDLER__H__



#include <LovyanGFX.hpp>
#include <Wire.h>
// #include "pin_config.h"

/// 要进行自定义设置的类，从LGFX_Device派生创建。
class K10_Lcd : public lgfx::LGFX_Device
{
// 准备与要连接的面板类型相匹配的实例。
  lgfx::Panel_ILI9341     _panel_instance;
// 准备与连接面板的总线种类相匹配的实例。
  lgfx::Bus_SPI        _bus_instance;   // SPI总线的实例
//lgfx::Bus_I2C        _bus_instance;   // I2C总线的实例
//lgfx::Bus_Parallel8  _bus_instance;   // 8位并行总线的实例
public:
  K10_Lcd(void)
  {
    { // 进行总线控制的设置。
      auto cfg = _bus_instance.config();    // 获取用于总线设置的结构体。

// SPI总线的设置
      cfg.spi_host = SPI2_HOST;     // 选择要使用的SPI  ESP32-S2,C3 : SPI2_HOST或SPI3_HOST / ESP32 : VSPI_HOST或HSPI_HOST
      // ※ 随着ESP-IDF版本升级，VSPI_HOST、HSPI_HOST的写法已不推荐使用，若出现错误，请使用SPI2_HOST、SPI3_HOST替代。
      cfg.spi_mode = 0;             // 设置SPI通信模式 (0 ~ 3)
      cfg.freq_write = 40000000;    // 发送时的SPI时钟（最大80MHz，会被舍入为80MHz除以整数后的值）
      cfg.freq_read  = 16000000;    // 接收时的SPI时钟
      cfg.spi_3wire  = true;        // 如果使用MOSI引脚进行接收，则设置为true
      cfg.use_lock   = true;        // 如果使用事务锁，则设置为true
      cfg.dma_channel = SPI_DMA_CH_AUTO; // 设置要使用的DMA通道 (0=不使用DMA / 1=1通道 / 2=通道 / SPI_DMA_CH_AUTO=自动设置)
      // ※ 随着ESP-IDF版本升级，DMA通道推荐使用SPI_DMA_CH_AUTO（自动设置），指定1通道、2通道已不推荐使用。
      cfg.pin_sclk = 12;            // 设置SPI的SCLK引脚编号
      cfg.pin_mosi = 21;            // 设置SPI的MOSI引脚编号
      cfg.pin_miso = -1;            // 设置SPI的MISO引脚编号 (-1 = 禁用)
      cfg.pin_dc   = 13;            // 设置SPI的D/C引脚编号  (-1 = 禁用)

      _bus_instance.config(cfg);    // 将设置值反映到总线上。
      _panel_instance.setBus(&_bus_instance);      // 将总线设置到面板上。
    }

    { // 进行显示面板控制的设置。
      auto cfg = _panel_instance.config();    // 获取用于显示面板设置的结构体。

      cfg.pin_cs           =    14;  // CS所连接的引脚编号   (-1 = 禁用)
      cfg.pin_rst          =    -1;  // RST所连接的引脚编号  (-1 = 禁用)
      cfg.pin_busy         =    -1;  // BUSY所连接的引脚编号 (-1 = 禁用)
      cfg.panel_width      =   240;  // 实际可显示的宽度
      cfg.panel_height     =   320;  // 实际可显示的高度
      cfg.offset_x         =     0;  // 面板在X方向的偏移量
      cfg.offset_y         =     0;  // 面板在Y方向的偏移量
      cfg.offset_rotation  =     0;  // 旋转方向值的偏移量，取值范围是0~7（4~7表示上下翻转）
      cfg.dummy_read_pixel =     8;  // 像素读取前的虚拟读取位数
      cfg.dummy_read_bits  =     1;  // 读取非像素数据前的虚拟读取位数
      cfg.readable         =  true;  // 如果可以读取数据，则设置为true
      cfg.invert           = false;  // 如果面板的明暗出现反转情况，则设置为true
      cfg.rgb_order        = false;  // 如果面板的红色和蓝色发生互换情况，则设置为true
      cfg.dlen_16bit       = false;  // 对于通过16位并行或SPI以16位为单位发送数据长度的面板，设置为true
      cfg.bus_shared       =  true;  // 如果与SD卡共享总线的情况（在drawJpgFile等操作中会进行总线控制），设置为true

      _panel_instance.config(cfg);
    }

    setPanel(&_panel_instance); // 设置要使用的面板。
  }
};


#endif  //!__LCD_HANDLER__H__
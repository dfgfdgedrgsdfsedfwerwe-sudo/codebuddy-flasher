/**
 * @file atk_display.h
 * @brief ATK BOX 显示接口 (适配 FluidBox display.h)
 *
 * 原 display.c 为 ESP-IDF SPI QSPI AMOLED 接口。
 * 此文件用 LovyanGFX 8080并口驱动 ST7789 替代。
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// 初始化显示 (创建 LovyanGFX 实例, 初始化 DMA)
void atk_display_init(void);

// 获取空闲条带缓冲 (阻塞直到两个缓冲之一空闲)
// 返回: BAND_ROWS * LCD_H_RES 像素的 uint16_t* 缓冲
uint16_t *display_acquire_band(void);

// 刷新条带到屏幕 (异步 DMA)
// band_index: 0..(BAND_COUNT-1)
// buffer: display_acquire_band() 返回的缓冲
void display_flush_band(int band_index, const uint16_t *buffer);

#ifdef __cplusplus
}
#endif

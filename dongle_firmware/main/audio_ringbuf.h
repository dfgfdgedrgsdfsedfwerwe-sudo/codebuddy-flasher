/*
 * audio_ringbuf.h - 音频环形缓冲区（全局单例）
 *
 * ESP-NOW 每 4ms 写入 128 字节 (不均匀到达)
 * USB UAC 每 1ms 读取 32 字节 (SOF 中断驱动，均匀)
 * 缓冲区平滑两者节奏差异，防止 USB 欠载
 */
#ifndef AUDIO_RINGBUF_H
#define AUDIO_RINGBUF_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 初始化环形缓冲区（全局单例）
 */
esp_err_t audio_ringbuf_init(void);

/**
 * 写入音频数据 (ESP-NOW 接收回调调用)
 * @return 实际写入字节数
 */
size_t audio_ringbuf_write(const uint8_t *data, size_t len);

/**
 * 读取音频数据 (USB UAC 回调调用)
 * @return 实际读取字节数
 */
size_t audio_ringbuf_read(uint8_t *data, size_t len);

/**
 * 获取当前可读字节数
 */
size_t audio_ringbuf_available(void);

/**
 * 获取缓冲区使用率百分比 (0-100)
 */
uint8_t audio_ringbuf_usage_percent(void);

/**
 * 重置缓冲区 (清空所有数据)
 */
void audio_ringbuf_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_RINGBUF_H */

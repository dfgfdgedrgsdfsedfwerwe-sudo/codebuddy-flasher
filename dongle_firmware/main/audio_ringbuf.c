/*
 * audio_ringbuf.c - 音频环形缓冲区实现（全局单例）
 */
#include "audio_ringbuf.h"
#include "config.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// 全局单例状态
static uint8_t s_buffer[AUDIO_RINGBUF_SIZE];
static uint32_t s_write_idx = 0;
static uint32_t s_read_idx = 0;
static uint32_t s_available = 0;
static SemaphoreHandle_t s_mutex = NULL;
static bool s_prebuffering = true;

esp_err_t audio_ringbuf_init(void)
{
    memset(s_buffer, 0, AUDIO_RINGBUF_SIZE);
    s_write_idx = 0;
    s_read_idx = 0;
    s_available = 0;
    s_prebuffering = true;
    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

size_t audio_ringbuf_write(const uint8_t *data, size_t len)
{
    if (s_mutex == NULL) return 0;
    xSemaphoreTake(s_mutex, portMAX_DELAY);

    /* 计算可写空间；若空间不足则丢弃最旧数据 (覆盖式) 优先保证低延迟 */
    uint32_t free_space = AUDIO_RINGBUF_SIZE - s_available;
    if (len > free_space) {
        /* 缓冲区将溢出：丢弃最旧的 (len - free_space) 字节 */
        uint32_t drop = len - free_space;
        s_read_idx = (s_read_idx + drop) % AUDIO_RINGBUF_SIZE;
        s_available -= drop;
    }

    for (size_t i = 0; i < len; i++) {
        s_buffer[s_write_idx] = data[i];
        s_write_idx = (s_write_idx + 1) % AUDIO_RINGBUF_SIZE;
    }
    s_available += len;
    if (s_available > AUDIO_RINGBUF_SIZE) {
        s_available = AUDIO_RINGBUF_SIZE;
    }

    /* 预缓冲阶段：累积到阈值后开始允许读取 */
    if (s_prebuffering && s_available >= AUDIO_PREBUFFER_BYTES) {
        s_prebuffering = false;
    }

    xSemaphoreGive(s_mutex);
    return len;
}

size_t audio_ringbuf_read(uint8_t *data, size_t len)
{
    if (s_mutex == NULL) return 0;
    xSemaphoreTake(s_mutex, portMAX_DELAY);

    /* 预缓冲阶段尚未完成：返回 0 (调用方会填静音) */
    if (s_prebuffering) {
        xSemaphoreGive(s_mutex);
        return 0;
    }

    uint32_t to_read = (len <= s_available) ? (uint32_t)len : s_available;

    for (uint32_t i = 0; i < to_read; i++) {
        data[i] = s_buffer[s_read_idx];
        s_read_idx = (s_read_idx + 1) % AUDIO_RINGBUF_SIZE;
    }
    s_available -= to_read;

    /* 缓冲区被读空：回到预缓冲状态，重新积累 */
    if (s_available == 0) {
        s_prebuffering = true;
    }

    xSemaphoreGive(s_mutex);
    return to_read;
}

size_t audio_ringbuf_available(void)
{
    if (s_mutex == NULL) return 0;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    uint32_t avail = s_available;
    xSemaphoreGive(s_mutex);
    return avail;
}

uint8_t audio_ringbuf_usage_percent(void)
{
    if (s_mutex == NULL) return 0;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    uint8_t pct = (uint8_t)((s_available * 100) / AUDIO_RINGBUF_SIZE);
    xSemaphoreGive(s_mutex);
    return pct;
}

void audio_ringbuf_reset(void)
{
    if (s_mutex == NULL) return;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_write_idx = 0;
    s_read_idx = 0;
    s_available = 0;
    s_prebuffering = true;
    xSemaphoreGive(s_mutex);
}

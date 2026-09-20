/*
 * config.h - CodeBuddy Dongle 端配置
 */
#ifndef CONFIG_H
#define CONFIG_H

/* ============================================================
 * ESP-NOW 配置
 * ============================================================ */

/* WiFi 信道 — 必须与设备端 (K10) 一致 (建议 6) */
#define ESPNOW_CHANNEL       1

/* 设备端 (K10) 的 MAC 地址。
 * 烧录设备端固件后，从其串口读取自身 MAC 填入这里。
 * 若为全 0，则 Dongle 接受任意来源的帧 (开发期方便，生产建议锁定)。 */
#define PEER_MAC_ADDR        { 0xB9, 0x8C, 0xAC, 0xA7, 0x04, 0xEF }

/* 是否启用 ESP-NOW 加密 (生产环境建议开启)。
 * 开启时需在两端配置相同的 PMK/LMK。 */
#define ESPNOW_ENCRYPT       0

/* ============================================================
 * 音频参数配置
 * ============================================================ */

/* 音频采样率 (Hz) */
#define AUDIO_SAMPLE_RATE    16000

/* 音频通道数 (1=单声道, 2=立体声) */
#define AUDIO_CHANNELS       1

/* 音频位深 (16-bit) */
#define AUDIO_BIT_DEPTH      16

/* 每帧字节数 (USB Full-Speed 1ms 传输) */
#define AUDIO_FRAME_BYTES    (AUDIO_SAMPLE_RATE / 1000 * AUDIO_CHANNELS * (AUDIO_BIT_DEPTH / 8))

/* ============================================================
 * 音频环形缓冲区配置
 * ESP-NOW 每 4ms 写入 128 字节；USB UAC 每 1ms 读取 32 字节
 * ============================================================ */

/* 环形缓冲区容量 (字节)。64KB = ~4 秒音频，用于平滑抖动 */
#define AUDIO_RINGBUF_SIZE   (64 * 1024)

/* 预缓冲阈值：缓冲区累积到该字节数后才开始向 USB 输出，
 * 避免刚开始就欠载 (underrun)。约 16ms 音频。 */
#define AUDIO_PREBUFFER_BYTES 512

/* ============================================================
 * USB 设备标识
 * ============================================================ */
#define USB_VID              0x303A   /* Espressif VID */
#define USB_PID              0x8000   /* 自定义 PID (HID+UAC 复合) */
#define USB_MANUFACTURER     "CodeBuddy"
#define USB_PRODUCT          "CodeBuddy Wireless Dongle"
#define USB_SERIAL           "CB-DONGLE-001"

/* ============================================================
 * LED 状态指示配置
 * ============================================================ */

/* LED GPIO 引脚 (ESP32-S3 开发板常用 GPIO 38/48，根据实际板子调整) */
#define LED_GPIO             48

/* 注意: LED 模式常量 (LED_OFF/LED_ON/LED_SLOW_BLINK 等) 由 led_indicator.h
 * 的 led_mode_t 枚举定义，此处不再重复定义，避免冲突。 */

/* ============================================================
 * 调试
 * ============================================================ */
#define DEBUG_STATS          1        /* 每秒打印接收/丢包统计 */
#define DEBUG_STATS_PERIOD_MS 1000

/* 注意: audio_control_range_4_n_t 宏由 TinyUSB 的 audio.h 提供，无需重复定义 */

#endif /* CONFIG_H */

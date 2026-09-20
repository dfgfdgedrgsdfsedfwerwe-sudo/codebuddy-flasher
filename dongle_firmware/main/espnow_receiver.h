/*
 * espnow_receiver.h - ESP-NOW 接收与帧分发
 */
#ifndef ESPNOW_RECEIVER_H
#define ESPNOW_RECEIVER_H

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 按键帧回调：解析出的 HID 数据交给主程序转发到 USB */
typedef void (*key_frame_handler_t)(uint8_t modifier, const uint8_t keycode[6]);

/* 接收统计 */
typedef struct {
    uint32_t audio_frames;    /* 收到的音频帧数 */
    uint32_t key_frames;      /* 收到的按键帧数 */
    uint32_t fec_frames;      /* 收到的 FEC 帧数 */
    uint32_t crc_errors;      /* CRC 校验失败数 */
    uint32_t lost_frames;     /* 检测到的丢包数 (按序列号) */
    uint32_t recovered_frames;/* 通过 FEC 恢复的帧数 */
} espnow_stats_t;

/**
 * 初始化 ESP-NOW 接收器
 * @param key_cb  按键帧处理回调（可选，传 NULL 则忽略按键帧）
 */
esp_err_t espnow_receiver_init(key_frame_handler_t key_cb);

/**
 * 获取接收统计 (拷贝一份，线程安全)
 */
void espnow_receiver_get_stats(espnow_stats_t *out);

#ifdef __cplusplus
}
#endif

#endif /* ESPNOW_RECEIVER_H */

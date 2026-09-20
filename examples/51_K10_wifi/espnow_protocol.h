/*
 * espnow_protocol.h - CodeBuddy ESP-NOW 数据帧协议定义
 *
 * ⚠️ 关键接口文件：设备端 (K10 / Arduino) 和 Dongle 端 (ESP-IDF) 必须使用
 *    完全一致的定义。修改此文件时，两端必须同步更新并重新编译。
 *
 * 帧结构使用 __attribute__((packed)) 保证两端内存布局完全一致，
 * 不受编译器对齐策略影响。
 */
#ifndef ESPNOW_PROTOCOL_H
#define ESPNOW_PROTOCOL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 音频规格 (设备端和 Dongle 端必须一致)
 * ============================================================ */
#define AUDIO_SAMPLE_RATE    16000   /* 采样率 16kHz */
#define AUDIO_BITS_PER_SAMPLE 16      /* 位深 16bit */
#define AUDIO_CHANNELS        1       /* 单声道 */

/* 每个音频帧携带的 PCM 字节数 = 64 采样点 × 2 字节 = 128 字节
 * 128 字节 / 32KB每秒 = 4ms，即每 4ms 发送一个音频帧 */
#define AUDIO_PAYLOAD_BYTES  128
#define AUDIO_SAMPLES_PER_FRAME (AUDIO_PAYLOAD_BYTES / (AUDIO_BITS_PER_SAMPLE / 8))  /* 64 */

/* FEC: 每 N 个音频包生成 1 个 XOR 冗余包 */
#define FEC_GROUP_SIZE       4

/* ============================================================
 * 帧类型标识
 * ============================================================ */
typedef enum {
    FRAME_TYPE_KEY   = 0x01,   /* 按键事件帧 */
    FRAME_TYPE_AUDIO = 0x02,   /* 音频数据帧 */
    FRAME_TYPE_FEC   = 0x03,   /* 音频 FEC 冗余帧 */
    FRAME_TYPE_PAIR  = 0x04,   /* 配对请求帧 */
    FRAME_TYPE_PAIR_ACK = 0x05,/* 配对响应帧 */
    FRAME_TYPE_HEARTBEAT = 0x06,/* 心跳帧 */
    FRAME_TYPE_TOKEN_STATUS = 0x07,  /* Token 使用状态帧 */
    FRAME_TYPE_PROJECT_STATUS = 0x08, /* 项目状态帧 */
    FRAME_TYPE_AI_STATE = 0x09,      /* AI 情绪状态帧 (驱动云朵表情) */
} frame_type_t;

/* ============================================================
 * 按键动作
 * ============================================================ */
typedef enum {
    KEY_ACTION_RELEASE = 0x00,  /* 释放 */
    KEY_ACTION_PRESS   = 0x01,  /* 按下 */
} key_action_t;

/* ============================================================
 * 按键事件帧 (13 字节)
 * ============================================================ */
typedef struct __attribute__((packed)) {
    uint8_t  frame_type;   /* = FRAME_TYPE_KEY */
    uint8_t  action;       /* key_action_t: 按下 / 释放 */
    uint8_t  modifier;     /* HID 修饰键位掩码 (Ctrl/Shift/Alt/GUI) */
    uint8_t  keycode[6];   /* HID 键码，最多同时 6 键 (USB HID 标准) */
    uint16_t timestamp;    /* 时间戳 (ms 低 16 位)，防重放 */
    uint8_t  seq_num;      /* 序列号，丢包检测 */
    uint8_t  crc8;         /* CRC8 校验 (覆盖前面所有字节) */
} key_frame_t;

/* ============================================================
 * 音频数据帧 (133 字节)
 * ============================================================ */
typedef struct __attribute__((packed)) {
    uint8_t  frame_type;                 /* = FRAME_TYPE_AUDIO */
    uint8_t  seq_num;                    /* 序列号 (0-255 循环)，丢包检测 */
    uint16_t timestamp;                  /* 时间戳 (ms) */
    uint8_t  audio_data[AUDIO_PAYLOAD_BYTES]; /* 128 字节 PCM */
    uint8_t  crc8;                       /* CRC8 校验 */
} audio_frame_t;

/* ============================================================
 * FEC 冗余帧 (131 字节)
 * ============================================================ */
typedef struct __attribute__((packed)) {
    uint8_t  frame_type;                 /* = FRAME_TYPE_FEC */
    uint8_t  fec_seq_base;               /* 对应 4 包音频的起始序列号 */
    uint8_t  audio_data[AUDIO_PAYLOAD_BYTES]; /* 4 包音频 XOR 结果 */
    uint8_t  crc8;                       /* CRC8 校验 */
} fec_frame_t;

/* ============================================================
 * 配对 / 心跳帧 (小帧)
 * ============================================================ */
typedef struct __attribute__((packed)) {
    uint8_t  frame_type;    /* = FRAME_TYPE_PAIR / PAIR_ACK / HEARTBEAT */
    uint8_t  mac[6];        /* 发送方 MAC */
    uint8_t  crc8;
} ctrl_frame_t;

/* ============================================================
 * Token 使用状态帧
 *
 * 一帧携带多个 AI 服务的 Token 使用情况。名称为定长字符串，
 * used/total 为 32 位整数，percent 由接收端计算或直接携带。
 * ============================================================ */
#define TOKEN_NAME_LEN     20   /* 服务名称最大长度 (含结尾 \0) */
#define TOKEN_MAX_ITEMS     5   /* 单帧最多携带的服务数 */

typedef struct __attribute__((packed)) {
    char     name[TOKEN_NAME_LEN];  /* 服务名, 如 "OpenAI Codex" */
    uint32_t used;                  /* 已用 token 数 */
    uint32_t total;                 /* 总配额 */
    uint16_t percent_x10;           /* 百分比 ×10 (847 = 84.7%) */
} token_item_t;                     /* 30 字节 */

typedef struct __attribute__((packed)) {
    uint8_t      frame_type;        /* = FRAME_TYPE_TOKEN_STATUS */
    uint8_t      count;             /* 有效条目数 (0~TOKEN_MAX_ITEMS) */
    uint8_t      seq_num;           /* 序列号 */
    token_item_t items[TOKEN_MAX_ITEMS]; /* 5 × 30 = 150 字节 */
    uint8_t      crc8;              /* CRC8 校验 */
} token_status_frame_t;             /* 1+1+1+150+1 = 154 字节 */

/* ============================================================
 * 项目状态帧
 *
 * 一帧携带多个项目的编码状态。status_code 映射到状态文字和颜色。
 * ============================================================ */
#define PROJECT_NAME_LEN   24   /* 项目名称最大长度 (含结尾 \0) */
#define PROJECT_MAX_ITEMS   6   /* 单帧最多携带的项目数 */

/* 项目状态码 -> 状态文字 + 圆点颜色 (接收端映射) */
typedef enum {
    PROJ_STATUS_PLANNING   = 0x00,  /* Planning     - 蓝色 */
    PROJ_STATUS_CODING     = 0x01,  /* Coding       - 紫色 */
    PROJ_STATUS_REVIEW     = 0x02,  /* Review Needed - 橙色 */
    PROJ_STATUS_COMPLETED  = 0x03,  /* Completed    - 绿色 */
    PROJ_STATUS_ERROR      = 0x04,  /* Error        - 红色 */
    PROJ_STATUS_IDLE       = 0x05,  /* Idle         - 灰色 */
} project_status_code_t;

typedef struct __attribute__((packed)) {
    char    name[PROJECT_NAME_LEN]; /* 项目名, 如 "Smart Todo App" */
    uint8_t status_code;            /* project_status_code_t */
} project_item_t;                   /* 25 字节 */

typedef struct __attribute__((packed)) {
    uint8_t        frame_type;      /* = FRAME_TYPE_PROJECT_STATUS */
    uint8_t        count;           /* 有效条目数 (0~PROJECT_MAX_ITEMS) */
    uint8_t        seq_num;         /* 序列号 */
    project_item_t items[PROJECT_MAX_ITEMS]; /* 6 × 25 = 150 字节 */
    uint8_t        crc8;            /* CRC8 校验 */
} project_status_frame_t;           /* 1+1+1+150+1 = 154 字节 */

/* ============================================================
 * AI 情绪状态帧 (驱动 K10 云朵表情)
 *
 * 上位机根据 AI 软件实时状态发送, 覆盖 K10 本地的自动循环。
 * text 为可选的自定义显示文字; 为空(首字节\0)时 K10 用 state 的默认文字。
 * ============================================================ */
#define AI_STATE_TEXT_LEN  20   /* 自定义状态文字最大长度 (含结尾 \0) */

/* AI 情绪码 -> 表情 (K10 端映射) */
typedef enum {
    AI_EMOTION_THINKING  = 0x00,  /* 思考: 挑眉 + 闭嘴 */
    AI_EMOTION_CODING    = 0x01,  /* 编码: 皱眉 + 说话动嘴 */
    AI_EMOTION_DONE      = 0x02,  /* 完成: 平眉 + 微笑 */
} ai_emotion_t;

typedef struct __attribute__((packed)) {
    uint8_t  frame_type;               /* = FRAME_TYPE_AI_STATE */
    uint8_t  emotion;                  /* ai_emotion_t 情绪码 */
    uint8_t  seq_num;                  /* 序列号 */
    char     text[AI_STATE_TEXT_LEN];  /* 自定义文字, 空则用默认 */
    uint8_t  crc8;                     /* CRC8 校验 */
} ai_state_frame_t;                    /* 1+1+1+20+1 = 24 字节 */

/* ============================================================
 * CRC8 校验 (多项式 0x07, 初值 0x00) — 两端共用同一实现
 * ============================================================ */
static inline uint8_t espnow_crc8(const uint8_t *data, uint32_t len)
{
    uint8_t crc = 0x00;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; b++) {
            if (crc & 0x80) {
                crc = (uint8_t)((crc << 1) ^ 0x07);
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

#ifdef __cplusplus
}
#endif

#endif /* ESPNOW_PROTOCOL_H */

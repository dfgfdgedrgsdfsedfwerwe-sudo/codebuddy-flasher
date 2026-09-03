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
    FRAME_TYPE_DECISION_REQ   = 0x0A,  /* 决策请求 (PC→Dongle→K10) */
    FRAME_TYPE_DECISION_REPLY = 0x0B,  /* 决策回执 (K10→Dongle→PC) */
    FRAME_TYPE_AGENT_STATUS       = 0x0C,  /* Agent 工作状态帧 (PC→BOX, 可视化) */
    FRAME_TYPE_AGENT_APPROVAL_REQ = 0x0D,  /* Agent 审批请求 (PC→BOX) */
    FRAME_TYPE_AGENT_APPROVAL_REPLY = 0x0E,/* Agent 审批回执 (BOX→PC) */
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
 * 决策帧 (Claude Code ⇄ K10 双向触摸决策)
 * ============================================================ */
#define DECISION_TITLE_LEN   32
#define DECISION_OPT_LEN     24
#define DECISION_MAX_OPTS     4

typedef struct __attribute__((packed)) {
    uint8_t  frame_type;                 /* = FRAME_TYPE_DECISION_REQ */
    uint16_t decision_id;                /* 递增, 回执对齐 */
    uint8_t  kind;                       /* 0=工具权限 1=多选题 */
    char     title[DECISION_TITLE_LEN];
    uint8_t  opt_count;                  /* 有效选项数 (1~4) */
    char     opts[DECISION_MAX_OPTS][DECISION_OPT_LEN];
    uint8_t  crc8;
} decision_request_frame_t;              /* 134 字节 */

typedef struct __attribute__((packed)) {
    uint8_t  frame_type;    /* = FRAME_TYPE_DECISION_REPLY */
    uint16_t decision_id;
    uint8_t  chosen_index;  /* 0xFF=超时/取消 */
    uint8_t  crc8;
} decision_reply_frame_t;   /* 5 字节 */

/* ============================================================
 * Agent 工作流可视化帧 (0x0C/0x0D/0x0E)
 * ============================================================ */
#define AGENT_TASK_MSG_LEN  48   /* Agent 任务回显文字最大长度 */

/* Agent 状态码 */
typedef enum {
    AGENT_STATE_READY    = 0x00,  /* 就绪 */
    AGENT_STATE_THINKING = 0x01,  /* 思考中 */
    AGENT_STATE_RUNNING  = 0x02,  /* 执行中 */
    AGENT_STATE_DONE     = 0x03,  /* 完成 */
    AGENT_STATE_ERROR    = 0x04,  /* 错误 */
} agent_state_t;

/* 0x0C Agent Status 帧 - Agent 工作状态推送 */
typedef struct __attribute__((packed)) {
    uint8_t  frame_type;               /* = FRAME_TYPE_AGENT_STATUS */
    uint8_t  state;                    /* agent_state_t 状态码 */
    uint8_t  seq_num;                  /* 序列号 */
    char     task_message[AGENT_TASK_MSG_LEN]; /* 任务回显："Reading config.py" */
    uint8_t  crc8;                     /* CRC8 校验 */
} agent_status_frame_t;                 /* 1+1+1+48+1 = 52 字节 */

/* 审批风险等级 */
typedef enum {
    APPROVAL_RISK_LOW    = 0x00,  /* 低风险 - 绿色横幅 */
    APPROVAL_RISK_MEDIUM = 0x01,  /* 中风险 - 黄色横幅 */
    APPROVAL_RISK_HIGH   = 0x02,  /* 高风险 - 红色横幅 */
} approval_risk_t;

/* 0x0D Agent Approval Request 帧 - Agent 审批请求 */
#define APPROVAL_TITLE_LEN  32
#define APPROVAL_TARGET_LEN 32
#define APPROVAL_DIFF_LEN   40

typedef struct __attribute__((packed)) {
    uint8_t  frame_type;               /* = FRAME_TYPE_AGENT_APPROVAL_REQ */
    uint16_t task_id;                  /* 任务 ID (递增) */
    uint8_t  risk_level;               /* approval_risk_t 风险等级 */
    char     title[APPROVAL_TITLE_LEN];     /* "Deploy to production" */
    char     target[APPROVAL_TARGET_LEN];   /* "api/deploy.sh" */
    char     diff_summary[APPROVAL_DIFF_LEN]; /* "+12 -3 lines" */
    uint8_t  crc8;                     /* CRC8 校验 */
} agent_approval_request_frame_t;       /* 1+2+1+32+32+40+1 = 109 字节 */

/* 审批动作码 */
typedef enum {
    APPROVAL_ACTION_APPROVE   = 0x00,  /* 批准 (OK 键) */
    APPROVAL_ACTION_REJECT    = 0x01,  /* 拒绝 (UP 键) */
    APPROVAL_ACTION_VIEW_DIFF = 0x02,  /* 查看详情 (DOWN 键) */
    APPROVAL_ACTION_TIMEOUT   = 0xFF,  /* 超时 */
} approval_action_t;

/* 0x0E Agent Approval Reply 帧 - Agent 审批回执 */
typedef struct __attribute__((packed)) {
    uint8_t  frame_type;    /* = FRAME_TYPE_AGENT_APPROVAL_REPLY */
    uint16_t task_id;       /* 对应请求的任务 ID */
    uint8_t  action;        /* approval_action_t 动作码 */
    uint8_t  crc8;          /* CRC8 校验 */
} agent_approval_reply_frame_t;  /* 1+2+1+1 = 5 字节 */

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

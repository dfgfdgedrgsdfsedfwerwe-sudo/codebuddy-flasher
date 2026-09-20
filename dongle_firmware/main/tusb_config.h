/*
 * tusb_config.h - TinyUSB 配置
 */
#ifndef TUSB_CONFIG_H
#define TUSB_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 通用配置
 * ============================================================ */
#define CFG_TUSB_MCU                OPT_MCU_ESP32S3
#define CFG_TUSB_OS                 OPT_OS_FREERTOS

#define CFG_TUSB_DEBUG              0

/* USB DMA on some MCUs */
#define CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_ALIGN          __attribute__ ((aligned(4)))

/* ============================================================
 * 设备模式配置
 * ============================================================ */
#define CFG_TUD_ENABLED             1
#define CFG_TUSB_RHPORT0_MODE       (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)
#define CFG_TUD_MAX_SPEED           OPT_MODE_FULL_SPEED

/* 端点 0 大小 */
#define CFG_TUD_ENDPOINT0_SIZE      64

/* ============================================================
 * USB 类启用
 * ============================================================ */
#define CFG_TUD_HID                 1  /* HID 键盘 */
#define CFG_TUD_AUDIO               1  /* UAC 麦克风 */

#define CFG_TUD_CDC                 0  /* 不用虚拟串口 */
#define CFG_TUD_MSC                 0  /* 不用 U 盘 */
#define CFG_TUD_MIDI                0
#define CFG_TUD_VENDOR              0
#define CFG_TUD_DFU                 0

/* ============================================================
 * HID 配置
 * ============================================================ */
#define CFG_TUD_HID_EP_BUFSIZE      16

/* ============================================================
 * UAC 音频配置
 * ============================================================ */

/* 音频参数 */
#define CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE                    16000  /* 16kHz 采样率 (关键!) */
#define CFG_TUD_AUDIO_FUNC_1_DESC_LEN                       TUD_AUDIO_MIC_ONE_CH_DESC_LEN
#define CFG_TUD_AUDIO_FUNC_1_N_AS_INT                       1   /* 1 个音频流接口 */
#define CFG_TUD_AUDIO_FUNC_1_CTRL_BUF_SZ                    64

/* 格式 */
#define CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX                  1   /* 单声道 */
#define CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX          2   /* 16bit = 2 字节 */
#define CFG_TUD_AUDIO_FUNC_1_RESOLUTION_PER_SAMPLE_TX       16  /* 16bit */

/* 端点配置 - 使用官方 TUD_AUDIO_EP_SIZE 宏计算 (含 1 样本余量, 34 字节) */
#define CFG_TUD_AUDIO_ENABLE_EP_IN                          1
#define CFG_TUD_AUDIO_EP_SZ_IN         TUD_AUDIO_EP_SIZE(CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE, CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX, CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX)
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX                   CFG_TUD_AUDIO_EP_SZ_IN
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ                (CFG_TUD_AUDIO_EP_SZ_IN * 2)  /* 2 帧缓冲 */

/* 不启用编码支持（简单PCM传输） */
#define CFG_TUD_AUDIO_ENABLE_ENCODING                       0

#ifdef __cplusplus
}
#endif

#endif /* TUSB_CONFIG_H */

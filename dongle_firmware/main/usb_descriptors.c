/*
 * usb_descriptors.c - USB HID+UAC 复合设备描述符
 *
 * 参考：
 * - TinyUSB hid_composite 示例 (HID 键盘部分)
 * - TinyUSB audio_4_channel_mic 示例 (UAC 麦克风部分)
 *
 * 复合设备结构：
 *   Interface 0: HID Keyboard (中断端点 EP1 IN)
 *   Interface 1: UAC Audio Control
 *   Interface 2: UAC Audio Streaming (同步端点 EP2 IN)
 */

#include <string.h>
#include "tusb.h"
#include "config.h"

/* ============================================================
 * 设备描述符
 * ============================================================ */
tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,  /* USB 2.0 */
    .bDeviceClass       = 0x00,    /* 复合设备：在接口级定义类 */
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = USB_VID,
    .idProduct          = USB_PID,
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

/* TinyUSB 调用 */
uint8_t const* tud_descriptor_device_cb(void) {
    return (uint8_t const*) &desc_device;
}

/* ============================================================
 * HID Report 描述符 - 标准键盘
 * ============================================================ */
uint8_t const desc_hid_report[] = {
    TUD_HID_REPORT_DESC_KEYBOARD()
};

/* TinyUSB 调用 */
uint8_t const* tud_hid_descriptor_report_cb(uint8_t instance) {
    (void) instance;
    return desc_hid_report;
}

/* ============================================================
 * 配置描述符
 * ============================================================ */

/* 接口编号 */
enum {
    ITF_NUM_HID = 0,
    ITF_NUM_AUDIO_CONTROL,
    ITF_NUM_AUDIO_STREAMING,
    ITF_NUM_TOTAL
};

/* 端点地址 */
#define EPNUM_HID_IN   0x81  /* EP1 IN: HID 中断 */
#define EPNUM_AUDIO_IN 0x82  /* EP2 IN: UAC 同步 */

/* UAC 音频参数 */
#define AUDIO_SAMPLE_RATE   16000
#define AUDIO_CHANNELS      1      /* 单声道 */
#define AUDIO_BIT_DEPTH     16
#define AUDIO_BYTES_PER_SAMPLE  (AUDIO_BIT_DEPTH / 8)

/* USB Full Speed EP 大小由 tusb_config.h 的 TUD_AUDIO_EP_SIZE 宏计算
 * = ((16000+999)/1000 + 1) × 2 × 1 = 34 字节 (含 1 样本余量，UAC 规范要求)
 * 注意: CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX 已在 tusb_config.h 中定义 */

#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN + TUD_AUDIO_MIC_ONE_CH_DESC_LEN)

uint8_t const desc_configuration[] = {
    /* Config 描述符 */
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),

    /* Interface 0: HID 键盘 */
    TUD_HID_DESCRIPTOR(ITF_NUM_HID, 0, HID_ITF_PROTOCOL_KEYBOARD, sizeof(desc_hid_report), EPNUM_HID_IN, CFG_TUD_HID_EP_BUFSIZE, 10),

    /* Interface 1-2: UAC 麦克风 (单声道 16kHz/16bit) */
    TUD_AUDIO_MIC_ONE_CH_DESCRIPTOR(ITF_NUM_AUDIO_CONTROL,
                                    /* stridx */ 0,
                                    /* nBytesPerSample */ AUDIO_BYTES_PER_SAMPLE,
                                    /* nBitsUsedPerSample */ AUDIO_BIT_DEPTH,
                                    EPNUM_AUDIO_IN,
                                    CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX)
};

/* TinyUSB 调用 */
uint8_t const* tud_descriptor_configuration_cb(uint8_t index) {
    (void) index;
    return desc_configuration;
}

/* ============================================================
 * 字符串描述符
 * ============================================================ */
char const* string_desc_arr[] = {
    (const char[]) { 0x09, 0x04 }, /* 0: 英文 (0x0409) */
    USB_MANUFACTURER,               /* 1: 制造商 */
    USB_PRODUCT,                    /* 2: 产品名 */
    USB_SERIAL,                     /* 3: 序列号 */
};

static uint16_t _desc_str[32];

/* TinyUSB 调用 */
uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void) langid;

    uint8_t chr_count;
    if (index == 0) {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else {
        if (index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) return NULL;
        const char* str = string_desc_arr[index];
        chr_count = (uint8_t) strlen(str);
        if (chr_count > 31) chr_count = 31;
        for(uint8_t i = 0; i < chr_count; i++) {
            _desc_str[1 + i] = str[i];
        }
    }

    _desc_str[0] = (uint16_t) ((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
    return _desc_str;
}

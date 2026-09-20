/*
 * main.c - CodeBuddy Dongle 主程序
 *
 * 功能：
 * 1. 初始化 TinyUSB (HID 键盘 + UAC 麦克风复合设备)
 * 2. 初始化 ESP-NOW 接收器
 * 3. 接收按键帧 → tud_hid_report() 转发到 PC
 * 4. 接收音频帧 → 环形缓冲区 → USB SOF 中断 → tud_audio_write() 转发到 PC
 * 5. 定期打印统计信息
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_private/usb_phy.h"
#include "nvs_flash.h"
#include "tusb.h"

#include "config.h"
#include "audio_ringbuf.h"
#include "espnow_receiver.h"
#include "led_indicator.h"
#include "pairing.h"
#include "pc_link.h"

static const char *TAG = "main";

// USB PHY 句柄
static usb_phy_handle_t phy_hdl = NULL;

// 统计信息
static uint32_t usb_audio_frames_sent = 0;
static uint32_t usb_hid_reports_sent = 0;

//--------------------------------------------------------------------+
// UAC Clock Source 控制
//--------------------------------------------------------------------+

// 当前采样率 (16kHz)
static uint32_t sampFreq = AUDIO_SAMPLE_RATE;

// 时钟有效标志
static uint8_t clkValid = 1;

// 采样率范围描述符 (仅支持 16kHz)
static audio_control_range_4_n_t(1) sampleFreqRng = {
    .wNumSubRanges = 1,
    .subrange[0] = {
        .bMin = AUDIO_SAMPLE_RATE,  // 16000
        .bMax = AUDIO_SAMPLE_RATE,  // 16000
        .bRes = 0                    // 步进为 0 表示只支持这一个值
    }
};

//--------------------------------------------------------------------+
// Feature Unit 控制 (Entity ID = 2)
//--------------------------------------------------------------------+

// 静音状态: [0]=master, [1]=channel 1
static uint8_t mute[2] = {0, 0};

// 音量 (单位: dB): [0]=master, [1]=channel 1
static int16_t volume[2] = {0, 0};  // 默认 0 dB

// 音量范围: -90 dB 到 +90 dB，步进 1 dB
static audio_control_range_2_n_t(1) volumeRng = {
    .wNumSubRanges = 1,
    .subrange[0] = {
        .bMin = -90,  // -90 dB
        .bMax = 90,   // +90 dB
        .bRes = 1     // 1 dB 步进
    }
};

//--------------------------------------------------------------------+
// USB 设备回调
//--------------------------------------------------------------------+

// 设备挂载回调
void tud_mount_cb(void)
{
    ESP_LOGI(TAG, "USB Device Mounted");
}

// 设备卸载回调
void tud_unmount_cb(void)
{
    ESP_LOGI(TAG, "USB Device Unmounted");
}

// USB 总线挂起回调
void tud_suspend_cb(bool remote_wakeup_en)
{
    ESP_LOGW(TAG, "USB Suspended (remote_wakeup=%d)", remote_wakeup_en);
}

// USB 总线恢复回调
void tud_resume_cb(void)
{
    ESP_LOGI(TAG, "USB Resumed");
}

//--------------------------------------------------------------------+
// USB HID 回调
//--------------------------------------------------------------------+

// HID 获取报告回调（主机请求输入报告时调用）
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                                 hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
    // 当前不支持主机请求报告
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) reqlen;
    return 0;
}

// HID 设置报告回调（主机发送输出报告时调用，例如键盘 LED 状态）
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                             hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
    // 接收主机发送的 LED 状态等
    (void) instance;
    (void) report_id;
    (void) report_type;

    if (bufsize > 0) {
        ESP_LOGD(TAG, "HID Set Report: %02X", buffer[0]);
    }
}

//--------------------------------------------------------------------+
// USB Audio 回调
//--------------------------------------------------------------------+

// UAC 设置接口回调（主机选择 alternate setting 时调用）
bool tud_audio_set_itf_cb(uint8_t rhport, tusb_control_request_t const * p_request)
{
    (void) rhport;
    uint8_t const itf = tu_u16_low(p_request->wIndex);
    uint8_t const alt = tu_u16_low(p_request->wValue);

    ESP_LOGI(TAG, "Audio set interface %u alt %u", itf, alt);
    return true;
}

// UAC 设置实体单元控制回调（音量、静音等）
bool tud_audio_set_req_entity_cb(uint8_t rhport, tusb_control_request_t const * p_request, uint8_t *pBuff)
{
    (void) rhport;

    // 通道号
    uint8_t const channelNum = tu_u16_low(p_request->wValue);

    // 控制选择器
    uint8_t const entity_sel = tu_u16_high(p_request->wValue);

    // 实体 ID
    uint8_t const entity_id = tu_u16_high(p_request->wIndex);

    ESP_LOGD(TAG, "Audio SET: entityID=%u sel=0x%02x ch=%u len=%u",
             entity_id, entity_sel, channelNum, p_request->wLength);

    // Feature Unit (ID=2) - 设置音量/静音
    if (entity_id == 2) {
        switch (entity_sel) {
            case AUDIO_FU_CTRL_MUTE:
                // 设置静音
                if (p_request->wLength == sizeof(audio_control_cur_1_t)) {
                    mute[channelNum] = ((audio_control_cur_1_t*)pBuff)->bCur;
                    ESP_LOGI(TAG, "FU set mute: ch=%u val=%u", channelNum, mute[channelNum]);
                    return true;
                }
                break;

            case AUDIO_FU_CTRL_VOLUME:
                // 设置音量
                if (p_request->wLength == sizeof(audio_control_cur_2_t)) {
                    volume[channelNum] = (int16_t)((audio_control_cur_2_t*)pBuff)->bCur;
                    ESP_LOGI(TAG, "FU set volume: ch=%u val=%d dB", channelNum, volume[channelNum]);
                    return true;
                }
                break;

            default:
                ESP_LOGW(TAG, "FU unsupported SET selector: 0x%02x", entity_sel);
                return false;
        }
    }

    // Clock Source (ID=4) - 设置采样率
    if (entity_id == 4) {
        if (entity_sel == AUDIO_CS_CTRL_SAM_FREQ) {
            if (p_request->wLength == sizeof(audio_control_cur_4_t)) {
                uint32_t new_freq = (uint32_t)((audio_control_cur_4_t*)pBuff)->bCur;
                ESP_LOGI(TAG, "Clock set freq: %d Hz (current: %d Hz)", (int)new_freq, (int)sampFreq);

                // 我们只支持 16kHz
                if (new_freq == AUDIO_SAMPLE_RATE) {
                    sampFreq = new_freq;
                    return true;
                } else {
                    ESP_LOGW(TAG, "Unsupported sample rate: %d Hz", (int)new_freq);
                    return false;
                }
            }
        }
    }

    ESP_LOGW(TAG, "Unsupported SET entity: %u", entity_id);
    return false;
}

// UAC 获取实体单元控制回调
bool tud_audio_get_req_entity_cb(uint8_t rhport, tusb_control_request_t const * p_request)
{
    (void) rhport;

    // 通道号 (wValue 低字节)
    uint8_t const channelNum = tu_u16_low(p_request->wValue);

    // 当前控制选择器 (wValue 高字节)
    uint8_t const entity_sel = tu_u16_high(p_request->wValue);

    // 目标实体 ID (wIndex 高字节)
    uint8_t const entity_id = tu_u16_high(p_request->wIndex);

    ESP_LOGD(TAG, "Audio GET: entityID=%u sel=0x%02x ch=%u bRequest=0x%02x",
             entity_id, entity_sel, channelNum, p_request->bRequest);

    // Feature Unit (ID=2) - 音量/静音控制
    if (entity_id == 2) {
        switch (entity_sel) {
            case AUDIO_FU_CTRL_MUTE:
                // 静音状态查询
                ESP_LOGD(TAG, "FU get mute: ch=%u val=%u", channelNum, mute[channelNum]);
                return tud_control_xfer(rhport, p_request, &mute[channelNum], 1);

            case AUDIO_FU_CTRL_VOLUME:
                // 音量查询
                if (p_request->bRequest == AUDIO_CS_REQ_CUR) {
                    ESP_LOGI(TAG, "FU get volume: ch=%u val=%d dB", channelNum, volume[channelNum]);
                    return tud_control_xfer(rhport, p_request, &volume[channelNum], sizeof(volume[channelNum]));
                }
                else if (p_request->bRequest == AUDIO_CS_REQ_RANGE) {
                    ESP_LOGI(TAG, "FU get volume range: ch=%u (-90~+90 dB)", channelNum);
                    return tud_control_xfer(rhport, p_request, &volumeRng, sizeof(volumeRng));
                }
                break;

            default:
                ESP_LOGW(TAG, "FU unsupported selector: 0x%02x", entity_sel);
                return false;
        }
    }

    // Clock Source (ID=4) 请求
    if (entity_id == 4) {
        switch (entity_sel) {
            case AUDIO_CS_CTRL_SAM_FREQ:
                // 采样率请求
                if (p_request->bRequest == AUDIO_CS_REQ_CUR) {
                    ESP_LOGI(TAG, "Clock get current freq: %d Hz", (int)sampFreq);
                    return tud_control_xfer(rhport, p_request, &sampFreq, sizeof(sampFreq));
                }
                else if (p_request->bRequest == AUDIO_CS_REQ_RANGE) {
                    ESP_LOGI(TAG, "Clock get freq range: %d Hz only", AUDIO_SAMPLE_RATE);
                    return tud_control_xfer(rhport, p_request, &sampleFreqRng, sizeof(sampleFreqRng));
                }
                break;

            case AUDIO_CS_CTRL_CLK_VALID:
                // 时钟有效标志
                ESP_LOGD(TAG, "Clock get valid: %u", clkValid);
                return tud_control_xfer(rhport, p_request, &clkValid, sizeof(clkValid));

            default:
                ESP_LOGW(TAG, "Clock unsupported selector: 0x%02x", entity_sel);
                return false;
        }
    }

    ESP_LOGW(TAG, "Unsupported entity: %u", entity_id);
    return false;  // 不支持的实体
}

// UAC TX 完成前加载回调（在 SOF 中断中调用，用于提供音频数据）
bool tud_audio_tx_done_pre_load_cb(uint8_t rhport, uint8_t itf, uint8_t ep_in, uint8_t cur_alt_setting)
{
    (void) rhport;
    (void) itf;
    (void) ep_in;
    (void) cur_alt_setting;

    // 从环形缓冲区读取音频数据 (使用 EP 大小，含 1 样本余量)
    uint8_t audio_data[CFG_TUD_AUDIO_EP_SZ_IN];
    size_t bytes_read = audio_ringbuf_read(audio_data, sizeof(audio_data));

    if (bytes_read > 0) {
        // 写入 USB 音频 FIFO
        tud_audio_write(audio_data, bytes_read);
        usb_audio_frames_sent++;
        led_indicate_data_activity();  // LED 指示数据发送
        return true;
    }

    // 缓冲区为空，发送静音
    memset(audio_data, 0, sizeof(audio_data));
    tud_audio_write(audio_data, sizeof(audio_data));
    return true;
}

//--------------------------------------------------------------------+
// USB 设备任务
//--------------------------------------------------------------------+

static void usb_device_task(void *param)
{
    ESP_LOGI(TAG, "USB Device Task Started");

    while (1) {
        // TinyUSB 设备任务处理
        tud_task();

        // 定期打印统计信息
        static uint32_t last_print_time = 0;
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (now - last_print_time > 5000) {
            ESP_LOGI(TAG, "Stats: Audio Frames=%lu, HID Reports=%lu, RingBuf=%u%%",
                     usb_audio_frames_sent, usb_hid_reports_sent,
                     audio_ringbuf_usage_percent());
            last_print_time = now;
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

//--------------------------------------------------------------------+
// 按键帧处理回调
//--------------------------------------------------------------------+

static void on_key_frame(uint8_t modifier, const uint8_t keycode[6])
{
    // 录音键改键：K10 的录音功能发送 F2 (0x3B)，在此补上左 Ctrl 修饰位，
    // 使 PC 收到的快捷键为 左Ctrl+F2。其他按键（Enter/Backspace 等）保持原样转发。
    uint8_t out_modifier = modifier;
    for (int i = 0; i < 6; i++) {
        if (keycode[i] == HID_KEY_F2) {
            out_modifier |= KEYBOARD_MODIFIER_LEFTCTRL;
            break;
        }
    }

    // 发送 HID 键盘报告到 PC (TinyUSB API)
    if (tud_hid_ready()) {
        tud_hid_keyboard_report(0, out_modifier, keycode);
        usb_hid_reports_sent++;
        led_indicate_data_activity();  // LED 指示数据发送

        // 打印调试信息（仅显示非零按键）
        ESP_LOGI(TAG, "HID Report: mod=0x%02X keys=[0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X]",
                 out_modifier, keycode[0], keycode[1], keycode[2], keycode[3], keycode[4], keycode[5]);
    } else {
        ESP_LOGW(TAG, "HID not ready, dropped key report");
    }
}

//--------------------------------------------------------------------+
// ESP-NOW 接收任务
//--------------------------------------------------------------------+

static void espnow_rx_task(void *param)
{
    ESP_LOGI(TAG, "ESP-NOW RX Task Started");

    // 当前版本：ESP-NOW 接收在中断回调中直接写入环形缓冲区
    // 此任务预留用于未来的数据处理逻辑

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

//--------------------------------------------------------------------+
// 主函数
//--------------------------------------------------------------------+

void app_main(void)
{
    ESP_LOGI(TAG, ">>> CodeBuddy Dongle Starting <<<");
    ESP_LOGI(TAG, "Chip: %s", CONFIG_IDF_TARGET);
    ESP_LOGI(TAG, "IDF Version: %s", esp_get_idf_version());

    // 1. 初始化 NVS (用于 WiFi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, ">>> NVS Initialized <<<");

    // 1.5. 初始化配对模块（从 NVS 加载已配对设备 MAC）
    // 注: 已改用动态配对协议，不再覆盖 MAC 地址。
    // 设备端上电发 PAIR 帧 → Dongle 学习并持久化 → 回 PAIR_ACK。
    ESP_ERROR_CHECK(pairing_init());
    ESP_LOGI(TAG, ">>> Pairing module initialized <<<");

    // 2. 初始化音频环形缓冲区
    ESP_ERROR_CHECK(audio_ringbuf_init());
    ESP_LOGI(TAG, ">>> Audio Ring Buffer Initialized <<<");

    // 3. 初始化 LED 指示灯
    ESP_ERROR_CHECK(led_indicator_init());
    ESP_LOGI(TAG, ">>> LED Indicator Initialized <<<");
    led_set_mode(LED_SLOW_BLINK);  // 初始化阶段慢闪

    // 4. 初始化 USB PHY
    // 延迟 3 秒让 JTAG 日志完全刷出（USB PHY 接管后 JTAG 控制台会失效）
    ESP_LOGI(TAG, ">>> Delay 3s before USB PHY (flush JTAG logs) <<<");
    vTaskDelay(pdMS_TO_TICKS(3000));

    ESP_LOGI(TAG, ">>> Initializing USB PHY <<<");
    usb_phy_config_t phy_conf = {
        .controller = USB_PHY_CTRL_OTG,
        .target = USB_PHY_TARGET_INT,
        .otg_mode = USB_OTG_MODE_DEVICE,
        .otg_speed = USB_PHY_SPEED_FULL,
    };
    ESP_ERROR_CHECK(usb_new_phy(&phy_conf, &phy_hdl));
    ESP_LOGI(TAG, ">>> USB PHY INITIALIZED SUCCESSFULLY <<<");

    // 5. 初始化 TinyUSB
    ESP_LOGI(TAG, ">>> Initializing TinyUSB <<<");
    bool usb_init_result = tusb_init();
    if (!usb_init_result) {
        ESP_LOGE(TAG, "!!! TinyUSB INIT FAILED !!!");
        led_set_mode(LED_FAST_BLINK);  // 初始化失败快闪
        return;
    }
    ESP_LOGI(TAG, ">>> TinyUSB INITIALIZED SUCCESSFULLY <<<");

    // 6. 初始化 ESP-NOW 接收器（注册按键帧处理回调）
    ESP_ERROR_CHECK(espnow_receiver_init(on_key_frame));
    ESP_LOGI(TAG, ">>> ESP-NOW Receiver STARTED (with key handler) <<<");

    // 6.5. 启动 pc_link (接管 UART0, PC↔Dongle 双向串口转发)
    ESP_ERROR_CHECK(pc_link_init());
    ESP_LOGI(TAG, ">>> PC Link (UART0) STARTED <<<");

    // 7. 创建 USB 设备任务
    xTaskCreatePinnedToCore(usb_device_task, "usb_dev", 4096, NULL, 5, NULL, 0);
    ESP_LOGI(TAG, ">>> USB Task Created Successfully <<<");

    // 8. 创建 ESP-NOW 接收任务（可选）
    xTaskCreatePinnedToCore(espnow_rx_task, "espnow_rx", 2048, NULL, 4, NULL, 1);
    ESP_LOGI(TAG, ">>> ESP-NOW Task Created Successfully <<<");

    // 9. 设置 LED 为心跳模式，表示系统正常运行
    led_set_mode(LED_HEARTBEAT);
    ESP_LOGI(TAG, ">>> CodeBuddy Dongle Ready <<<");
}

//--------------------------------------------------------------------+
// FreeRTOS Idle Hook (USB-JTAG 控制台配置要求提供此函数)
//--------------------------------------------------------------------+
void vApplicationIdleHook(void)
{
    // 空实现：Idle 任务无需额外操作
}

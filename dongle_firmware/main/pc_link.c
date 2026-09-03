/*
 * pc_link.c - PC↔Dongle UART0 双向转发
 *
 * 控制台已迁到 USB-Serial/JTAG (sdkconfig.defaults), UART0 (GPIO43/44) 由本模块接管。
 * 串口帧格式: [0xA5 0x5A | len | payload(len) | crc8], payload 首字节即 frame_type。
 * 方向:
 *   PC→Dongle: 0x0A 决策请求 → 转发给已配对 K10 (ESP-NOW)
 *   Dongle→PC: 0x0B 决策回执 (由 espnow_receiver 调 pc_link_send_reply_to_pc 写回)
 */
#include "pc_link.h"
#include "espnow_protocol.h"
#include "pairing.h"
#include "esp_log.h"
#include "esp_now.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "pc_link";
#define PC_UART_NUM      UART_NUM_0
#define PC_UART_TX_GPIO  43
#define PC_UART_RX_GPIO  44
#define PC_UART_BAUD     115200
#define PC_MAGIC0        0xA5
#define PC_MAGIC1        0x5A
#define PC_RX_BUFSZ      512

/* 把一帧 (首字节 frame_type) 加魔术头写回 PC:
   magic0 magic1 | len | frame(len) | crc8(frame) */
void pc_link_send_reply_to_pc(const uint8_t *frame, uint16_t len)
{
    uint8_t hdr[3] = { PC_MAGIC0, PC_MAGIC1, (uint8_t)len };
    uint8_t crc = espnow_crc8(frame, len);
    uart_write_bytes(PC_UART_NUM, (const char*)hdr, 3);
    uart_write_bytes(PC_UART_NUM, (const char*)frame, len);
    uart_write_bytes(PC_UART_NUM, (const char*)&crc, 1);
}

/* 收到完整 PC 帧 (frame[0]=frame_type) 后处理 */
static void handle_pc_frame(const uint8_t *frame, uint16_t len)
{
    if (len < 1) return;

    frame_type_t ftype = (frame_type_t)frame[0];

    /* 通用转发逻辑: 0x07 Token / 0x08 Project / 0x09 AI / 0x0A Decision / 0x0C Agent / 0x0D Approval */
    if (ftype == FRAME_TYPE_TOKEN_STATUS || ftype == FRAME_TYPE_PROJECT_STATUS ||
        ftype == FRAME_TYPE_AI_STATE || ftype == FRAME_TYPE_DECISION_REQ ||
        ftype == FRAME_TYPE_AGENT_STATUS || ftype == FRAME_TYPE_AGENT_APPROVAL_REQ)
    {
        /* 长度校验 */
        uint16_t expect_len = 0;
        switch (ftype) {
            case FRAME_TYPE_TOKEN_STATUS:   expect_len = sizeof(token_status_frame_t); break;
            case FRAME_TYPE_PROJECT_STATUS: expect_len = sizeof(project_status_frame_t); break;
            case FRAME_TYPE_AI_STATE:       expect_len = sizeof(ai_state_frame_t); break;
            case FRAME_TYPE_DECISION_REQ:   expect_len = sizeof(decision_request_frame_t); break;
            case FRAME_TYPE_AGENT_STATUS:   expect_len = sizeof(agent_status_frame_t); break;
            case FRAME_TYPE_AGENT_APPROVAL_REQ: expect_len = sizeof(agent_approval_request_frame_t); break;
            default: break;
        }
        if (expect_len && len != expect_len) {
            ESP_LOGW(TAG, "frame 0x%02X len mismatch: got %d, expect %d", ftype, len, expect_len);
            return;
        }

        /* 检查配对 */
        uint8_t peer[6];
        if (pairing_has_pair()) {
            pairing_get_peer_mac(peer);
        } else {
            /* 未配对时使用广播地址 (开发模式) */
            memset(peer, 0xFF, 6);

            /* 确保广播 peer 已注册 (ESP-NOW 要求) */
            static bool broadcast_peer_added = false;
            if (!broadcast_peer_added) {
                esp_now_peer_info_t bcast = {0};
                memset(bcast.peer_addr, 0xFF, 6);
                bcast.channel = 1;  // ESPNOW_CHANNEL from config.h
                bcast.ifidx = WIFI_IF_STA;
                bcast.encrypt = false;
                esp_err_t ret = esp_now_add_peer(&bcast);
                if (ret == ESP_OK || ret == ESP_ERR_ESPNOW_EXIST) {
                    broadcast_peer_added = true;
                    ESP_LOGI(TAG, "Broadcast peer added for dev mode");
                } else {
                    ESP_LOGW(TAG, "Failed to add broadcast peer: %s", esp_err_to_name(ret));
                }
            }
        }

        /* ESP-NOW 转发到配对设备或广播 */
        esp_err_t r = esp_now_send(peer, frame, len);

        const char *type_name =
            (ftype == FRAME_TYPE_TOKEN_STATUS)   ? "Token" :
            (ftype == FRAME_TYPE_PROJECT_STATUS) ? "Project" :
            (ftype == FRAME_TYPE_AI_STATE)       ? "AI" :
            (ftype == FRAME_TYPE_DECISION_REQ)   ? "Decision" :
            (ftype == FRAME_TYPE_AGENT_STATUS)   ? "AgentStatus" :
            (ftype == FRAME_TYPE_AGENT_APPROVAL_REQ) ? "Approval" : "Unknown";

        ESP_LOGI(TAG, "fwd 0x%02X (%s) to device: %s", ftype, type_name, esp_err_to_name(r));
    }
    else {
        ESP_LOGW(TAG, "unknown frame type 0x%02X", ftype);
    }
}

static void pc_rx_task(void *arg)
{
    uint8_t byte;
    enum { S_M0, S_M1, S_LEN, S_BODY, S_CRC } st = S_M0;
    uint8_t buf[160]; uint16_t need = 0, got = 0;
    while (1) {
        int n = uart_read_bytes(PC_UART_NUM, &byte, 1, pdMS_TO_TICKS(100));
        if (n != 1) continue;
        switch (st) {
        case S_M0: if (byte == PC_MAGIC0) st = S_M1; break;
        case S_M1: st = (byte == PC_MAGIC1) ? S_LEN : S_M0; break;
        case S_LEN:
            need = byte;
            if (need == 0 || need > sizeof(buf)) { st = S_M0; break; }
            got = 0; st = S_BODY; break;
        case S_BODY:
            buf[got++] = byte;
            if (got >= need) st = S_CRC;
            break;
        case S_CRC:
            if (espnow_crc8(buf, need) == byte) handle_pc_frame(buf, need);
            else ESP_LOGW(TAG, "pc frame crc err");
            st = S_M0; break;
        }
    }
}

esp_err_t pc_link_init(void)
{
    uart_config_t cfg = {
        .baud_rate = PC_UART_BAUD, .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE, .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(PC_UART_NUM, PC_RX_BUFSZ, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(PC_UART_NUM, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(PC_UART_NUM, PC_UART_TX_GPIO, PC_UART_RX_GPIO,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    xTaskCreatePinnedToCore(pc_rx_task, "pc_rx", 4096, NULL, 4, NULL, 1);
    ESP_LOGI(TAG, "pc_link ready on UART0");
    return ESP_OK;
}

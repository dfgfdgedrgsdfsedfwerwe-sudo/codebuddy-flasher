/*
 * espnow_receiver.c - ESP-NOW 接收与帧分发实现
 */
#include "espnow_receiver.h"
#include "audio_ringbuf.h"
#include "espnow_protocol.h"
#include "config.h"
#include "led_indicator.h"
#include "pairing.h"
#include "pc_link.h"

#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_log.h"
#include "esp_mac.h"
#include <string.h>

static const char *TAG = "espnow_rx";

/* 全局状态 */
static key_frame_handler_t g_key_handler = NULL;
static espnow_stats_t g_stats = {0};
static SemaphoreHandle_t g_stats_mutex = NULL;

/* 音频序列号跟踪 (丢包检测) */
static uint8_t g_last_audio_seq = 0xFF;
static bool g_first_audio_frame = true;

/* FEC 恢复缓存：保存最近 4 个音频帧，配合 FEC 帧恢复丢包 */
#define FEC_HISTORY_SIZE 4
static struct {
    bool valid;
    uint8_t seq;
    uint8_t data[AUDIO_PAYLOAD_BYTES];
} g_fec_history[FEC_HISTORY_SIZE];
static uint8_t g_fec_history_idx = 0;

/* ============================================================
 * FEC 历史管理
 * ============================================================ */
static void fec_history_add(uint8_t seq, const uint8_t *data)
{
    g_fec_history[g_fec_history_idx].valid = true;
    g_fec_history[g_fec_history_idx].seq = seq;
    memcpy(g_fec_history[g_fec_history_idx].data, data, AUDIO_PAYLOAD_BYTES);
    g_fec_history_idx = (g_fec_history_idx + 1) % FEC_HISTORY_SIZE;
}

static void fec_history_clear(void)
{
    memset(g_fec_history, 0, sizeof(g_fec_history));
    g_fec_history_idx = 0;
}

/* ============================================================
 * 按键帧处理
 * ============================================================ */
static void handle_key_frame(const uint8_t *data, uint32_t len)
{
    if (len < sizeof(key_frame_t)) {
        ESP_LOGW(TAG, "Key frame too short: %lu", len);
        return;
    }

    key_frame_t *frame = (key_frame_t *)data;

    /* CRC 校验 */
    uint8_t calc_crc = espnow_crc8(data, sizeof(key_frame_t) - 1);
    if (calc_crc != frame->crc8) {
        xSemaphoreTake(g_stats_mutex, portMAX_DELAY);
        g_stats.crc_errors++;
        xSemaphoreGive(g_stats_mutex);
        ESP_LOGW(TAG, "Key frame CRC error");
        return;
    }

    /* 统计 */
    xSemaphoreTake(g_stats_mutex, portMAX_DELAY);
    g_stats.key_frames++;
    xSemaphoreGive(g_stats_mutex);

    /* LED 指示接收到按键帧 */
    led_indicate_data_activity();

    /* 回调通知主程序 */
    if (g_key_handler) {
        g_key_handler(frame->modifier, frame->keycode);
    }
}

/* ============================================================
 * 音频帧处理
 * ============================================================ */
static void handle_audio_frame(const uint8_t *data, uint32_t len)
{
    if (len < sizeof(audio_frame_t)) {
        ESP_LOGW(TAG, "Audio frame too short: %lu", len);
        return;
    }

    audio_frame_t *frame = (audio_frame_t *)data;

    /* CRC 校验 */
    uint8_t calc_crc = espnow_crc8(data, sizeof(audio_frame_t) - 1);
    if (calc_crc != frame->crc8) {
        xSemaphoreTake(g_stats_mutex, portMAX_DELAY);
        g_stats.crc_errors++;
        xSemaphoreGive(g_stats_mutex);
        return;
    }

    /* 丢包检测 (序列号跳变) */
    if (!g_first_audio_frame) {
        uint8_t expected_seq = (g_last_audio_seq + 1) & 0xFF;
        if (frame->seq_num != expected_seq) {
            uint8_t lost = (frame->seq_num - expected_seq) & 0xFF;
            xSemaphoreTake(g_stats_mutex, portMAX_DELAY);
            g_stats.lost_frames += lost;
            xSemaphoreGive(g_stats_mutex);
            ESP_LOGW(TAG, "Audio gap detected: expected %u, got %u (lost %u)",
                     expected_seq, frame->seq_num, lost);
        }
    }
    g_last_audio_seq = frame->seq_num;
    g_first_audio_frame = false;

    /* 写入环形缓冲区 (全局单例) */
    audio_ringbuf_write(frame->audio_data, AUDIO_PAYLOAD_BYTES);

    /* 保存到 FEC 历史 */
    fec_history_add(frame->seq_num, frame->audio_data);

    /* LED 指示接收到音频帧 */
    led_indicate_data_activity();

    /* 统计 */
    xSemaphoreTake(g_stats_mutex, portMAX_DELAY);
    g_stats.audio_frames++;
    xSemaphoreGive(g_stats_mutex);
}

/* ============================================================
 * FEC 帧处理 (丢包恢复)
 * ============================================================ */
static void handle_fec_frame(const uint8_t *data, uint32_t len)
{
    if (len < sizeof(fec_frame_t)) return;

    fec_frame_t *frame = (fec_frame_t *)data;

    /* CRC 校验 */
    uint8_t calc_crc = espnow_crc8(data, sizeof(fec_frame_t) - 1);
    if (calc_crc != frame->crc8) {
        xSemaphoreTake(g_stats_mutex, portMAX_DELAY);
        g_stats.crc_errors++;
        xSemaphoreGive(g_stats_mutex);
        return;
    }

    xSemaphoreTake(g_stats_mutex, portMAX_DELAY);
    g_stats.fec_frames++;
    xSemaphoreGive(g_stats_mutex);

    /* FEC 恢复逻辑：检查历史中是否缺失某一帧 (seq = fec_seq_base ~ fec_seq_base+3)
     * 若缺失且其余 3 帧都有，则可用 FEC XOR 恢复。
     * 简化实现：暂不做完整恢复，仅统计 FEC 帧数。实际恢复留给后续优化。 */

    /* TODO: 完整 FEC 恢复逻辑 (检测丢包 → XOR 恢复 → 写入缓冲区) */
}

/* ============================================================
 * 决策回执帧处理 (0x0B): K10 → Dongle → PC
 * ============================================================ */
static void handle_decision_reply_frame(const uint8_t *data, uint32_t len)
{
    if (len != sizeof(decision_reply_frame_t)) return;
    const decision_reply_frame_t *rep = (const decision_reply_frame_t *)data;
    if (espnow_crc8(data, sizeof(*rep) - 1) != rep->crc8) {
        xSemaphoreTake(g_stats_mutex, portMAX_DELAY);
        g_stats.crc_errors++;
        xSemaphoreGive(g_stats_mutex);
        return;
    }
    /* 加魔术头经 UART0 写回 PC */
    pc_link_send_reply_to_pc(data, len);
}

/* ============================================================
 * ESP-NOW 接收回调
 * ============================================================ */
static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{
    if (len < 1) return;

    frame_type_t frame_type = (frame_type_t)data[0];
    const uint8_t *src_mac = recv_info->src_addr;

    /* PAIR 帧特殊处理: 无需已配对即可处理 */
    if (frame_type == FRAME_TYPE_PAIR) {
        pairing_handle_pair_frame(data, len, src_mac);
        return;
    }

    /* 其他帧: 检查是否来自已配对设备 (开发模式: 无配对时接受任意设备) */
    if (!pairing_is_paired(src_mac)) {
        if (pairing_has_pair()) {
            /* 有配对记录但 MAC 不匹配 -> 拒绝 */
            ESP_LOGW(TAG, "Frame from unpaired device: %02X:%02X:%02X:%02X:%02X:%02X",
                     src_mac[0], src_mac[1], src_mac[2],
                     src_mac[3], src_mac[4], src_mac[5]);
            return;
        }
        /* 无配对记录 -> 开发模式,接受任意设备。
         * 仅在源 MAC 变化时打印一次，避免每个音频帧都同步写 UART
         * 阻塞接收回调（曾导致 Audio gap / HID not ready）。 */
        static uint8_t last_dev_mac[6] = {0};
        if (memcmp(last_dev_mac, src_mac, 6) != 0) {
            memcpy(last_dev_mac, src_mac, 6);
            ESP_LOGI(TAG, "Dev mode: accepting from %02X:%02X:%02X:%02X:%02X:%02X",
                     src_mac[0], src_mac[1], src_mac[2],
                     src_mac[3], src_mac[4], src_mac[5]);
        }
    }

    switch (frame_type) {
        case FRAME_TYPE_KEY:
            handle_key_frame(data, len);
            break;

        case FRAME_TYPE_AUDIO:
            handle_audio_frame(data, len);
            break;

        case FRAME_TYPE_FEC:
            handle_fec_frame(data, len);
            break;

        case FRAME_TYPE_HEARTBEAT:
            /* 心跳帧: 暂不处理，预留 */
            break;

        case FRAME_TYPE_DECISION_REPLY:
            handle_decision_reply_frame(data, len);
            break;

        case FRAME_TYPE_AGENT_APPROVAL_REPLY:
            if (len == sizeof(agent_approval_reply_frame_t)) {
                pc_link_send_reply_to_pc(data, len);  // 回传 PC
                ESP_LOGI(TAG, "fwd agent approval reply to PC");
            } else {
                ESP_LOGW(TAG, "agent approval reply len mismatch: %d", len);
            }
            break;

        default:
            ESP_LOGW(TAG, "Unknown frame type: 0x%02X", frame_type);
            break;
    }
}

/* ============================================================
 * 初始化
 * ============================================================ */
esp_err_t espnow_receiver_init(key_frame_handler_t key_cb)
{
    g_key_handler = key_cb;
    g_stats_mutex = xSemaphoreCreateMutex();
    if (g_stats_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }
    fec_history_clear();

    /* WiFi 初始化 (STA 模式，ESP-NOW 需要) */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_start());

    /* 设置 WiFi 信道 (与设备端一致) */
    ESP_ERROR_CHECK(esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));

    /* ESP-NOW 初始化 */
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));

    /* 配对设备若已存在，添加为 peer（优化发送延迟，非必须） */
    if (pairing_has_pair()) {
        uint8_t peer_mac[6];
        pairing_get_peer_mac(peer_mac);
        esp_now_peer_info_t peer_info = {0};
        memcpy(peer_info.peer_addr, peer_mac, 6);
        peer_info.channel = ESPNOW_CHANNEL;
        peer_info.ifidx = WIFI_IF_STA;
        peer_info.encrypt = ESPNOW_ENCRYPT;
        esp_err_t ret = esp_now_add_peer(&peer_info);
        if (ret == ESP_OK || ret == ESP_ERR_ESPNOW_EXIST) {
            ESP_LOGI(TAG, "Added paired peer: %02X:%02X:%02X:%02X:%02X:%02X",
                     peer_mac[0], peer_mac[1], peer_mac[2],
                     peer_mac[3], peer_mac[4], peer_mac[5]);
        }
    } else {
        ESP_LOGI(TAG, "No paired device - waiting for PAIR frame");
    }

    /* 打印本机 MAC (用于设备端配置) */
    uint8_t self_mac[6];
    esp_read_mac(self_mac, ESP_MAC_WIFI_STA);
    ESP_LOGI(TAG, "Dongle MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             self_mac[0], self_mac[1], self_mac[2],
             self_mac[3], self_mac[4], self_mac[5]);
    ESP_LOGI(TAG, "ESP-NOW receiver initialized on channel %d", ESPNOW_CHANNEL);

    return ESP_OK;
}

void espnow_receiver_get_stats(espnow_stats_t *out)
{
    xSemaphoreTake(g_stats_mutex, portMAX_DELAY);
    memcpy(out, &g_stats, sizeof(espnow_stats_t));
    xSemaphoreGive(g_stats_mutex);
}

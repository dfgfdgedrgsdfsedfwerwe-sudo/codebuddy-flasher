/*
 * pairing.c - 动态配对模块 (Dongle 端)
 *
 * 替代 config.h 硬编码 PEER_MAC_ADDR + main.c 覆盖 MAC 的方案:
 *   1. 设备端 (K10/ATK BOX) 上电后发送 PAIR 帧 (含自身 MAC)
 *   2. Dongle 收到后写入 NVS 持久化, 并回复 PAIR_ACK (含 Dongle MAC)
 *   3. 设备端收到 ACK 后保存 Dongle MAC 到 Preferences (NVS), 后续单播发送
 *   4. 已配对的设备数据帧正常处理; 未配对设备的 PAIR 帧触发配对流程
 *
 * 配对模式触发:
 *   - Dongle 首次上电 (NVS 无配对数据)
 *   - 设备端连续 N 次发送失败后重新发 PAIR (检测到换 Dongle)
 */
#include "pairing.h"
#include "espnow_protocol.h"
#include "config.h"
#include "esp_log.h"
#include "esp_now.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>

static const char *TAG = "pairing";

#define NVS_NAMESPACE "pairing"
#define NVS_KEY_MAC   "peer_mac"
#define NVS_KEY_VALID "peer_valid"

static uint8_t g_paired_mac[6];
static bool    g_paired = false;

/* 发送 PAIR_ACK 给设备端 */
static void send_pair_ack(const uint8_t *dest_mac)
{
    ctrl_frame_t ack;
    memset(&ack, 0, sizeof(ack));
    ack.frame_type = FRAME_TYPE_PAIR_ACK;
    esp_read_mac(ack.mac, ESP_MAC_WIFI_STA);
    ack.crc8 = espnow_crc8((uint8_t *)&ack, sizeof(ack) - 1);

    /* ACK 单播回源地址 */
    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, dest_mac, 6);
    peer.channel = ESPNOW_CHANNEL;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    esp_now_add_peer(&peer);   /* 已存在返回 ESP_ERR_ESPNOW_EXIST, 忽略 */

    esp_err_t ret = esp_now_send(dest_mac, (uint8_t *)&ack, sizeof(ack));
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "PAIR_ACK sent to %02X:%02X:%02X:%02X:%02X:%02X",
                 dest_mac[0], dest_mac[1], dest_mac[2],
                 dest_mac[3], dest_mac[4], dest_mac[5]);
    } else {
        ESP_LOGW(TAG, "PAIR_ACK send failed: %s", esp_err_to_name(ret));
    }
}

/* 处理 PAIR 帧: 学习 MAC + 存 NVS + 回 ACK */
void pairing_handle_pair_frame(const uint8_t *data, uint32_t len, const uint8_t *src_mac)
{
    if (len < sizeof(ctrl_frame_t)) return;

    ctrl_frame_t *frame = (ctrl_frame_t *)data;
    if (espnow_crc8(data, sizeof(ctrl_frame_t) - 1) != frame->crc8) {
        ESP_LOGW(TAG, "Pair frame CRC error");
        return;
    }

    /* 学习发送者 MAC (以实际来源 src_mac 为准) */
    memcpy(g_paired_mac, src_mac, 6);
    g_paired = true;

    /* NVS 持久化 */
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_blob(h, NVS_KEY_MAC, g_paired_mac, 6);
        nvs_set_u8(h, NVS_KEY_VALID, 1);
        nvs_commit(h);
        nvs_close(h);
    }

    ESP_LOGI(TAG, "Paired with device: %02X:%02X:%02X:%02X:%02X:%02X",
             g_paired_mac[0], g_paired_mac[1], g_paired_mac[2],
             g_paired_mac[3], g_paired_mac[4], g_paired_mac[5]);

    send_pair_ack(src_mac);
}

/* 初始化: 从 NVS 加载已配对 MAC */
esp_err_t pairing_init(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) == ESP_OK) {
        uint8_t valid = 0;
        size_t len = 6;
        if (nvs_get_u8(h, NVS_KEY_VALID, &valid) == ESP_OK && valid) {
            if (nvs_get_blob(h, NVS_KEY_MAC, g_paired_mac, &len) == ESP_OK) {
                g_paired = true;
                ESP_LOGI(TAG, "Loaded paired device: %02X:%02X:%02X:%02X:%02X:%02X",
                         g_paired_mac[0], g_paired_mac[1], g_paired_mac[2],
                         g_paired_mac[3], g_paired_mac[4], g_paired_mac[5]);
            }
        }
        nvs_close(h);
    } else {
        ESP_LOGI(TAG, "No paired device in NVS (first boot?) - waiting for PAIR frame");
    }
    return ESP_OK;
}

/* 查询: 该 MAC 是否已配对 */
bool pairing_is_paired(const uint8_t *mac)
{
    return g_paired && (memcmp(mac, g_paired_mac, 6) == 0);
}

/* 查询: 是否有任意已配对设备 */
bool pairing_has_pair(void)
{
    return g_paired;
}

/* 获取已配对设备的 MAC */
void pairing_get_peer_mac(uint8_t *out)
{
    memcpy(out, g_paired_mac, 6);
}

/* 清除配对 (预留: 长按 Dongle 按钮触发) */
esp_err_t pairing_clear(void)
{
    g_paired = false;
    memset(g_paired_mac, 0, 6);
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        nvs_erase_key(h, NVS_KEY_VALID);
        nvs_commit(h);
        nvs_close(h);
    }
    ESP_LOGI(TAG, "Pairing cleared");
    return ESP_OK;
}

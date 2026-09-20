/*
 * pairing.h - 动态配对模块接口 (Dongle 端)
 *
 * 替代 config.h 硬编码 PEER_MAC_ADDR + main.c 覆盖 MAC 的方案。
 * 详见 pairing.c 头部说明。
 */
#ifndef PAIRING_H
#define PAIRING_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化: 从 NVS 加载已配对设备 MAC (需在 nvs_flash_init 之后调用) */
esp_err_t pairing_init(void);

/* 处理收到的 PAIR 帧: 校验 CRC → 学习 src_mac → 存 NVS → 回 PAIR_ACK */
void pairing_handle_pair_frame(const uint8_t *data, uint32_t len, const uint8_t *src_mac);

/* 查询该 MAC 是否为已配对设备 */
bool pairing_is_paired(const uint8_t *mac);

/* 查询是否有任意已配对设备 */
bool pairing_has_pair(void);

/* 获取已配对设备 MAC (无配对时返回全 0) */
void pairing_get_peer_mac(uint8_t *out);

/* 清除配对 (预留: 按钮/命令触发) */
esp_err_t pairing_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* PAIRING_H */

#ifndef PC_LINK_H
#define PC_LINK_H
#include <stdint.h>
#include "esp_err.h"
#ifdef __cplusplus
extern "C" {
#endif
/* 装 UART0 driver + 起接收任务。须在 pairing_init 之后调用。 */
esp_err_t pc_link_init(void);
/* 把一帧 (首字节为 frame_type) 加 [0xA5 0x5A|len|payload|crc8] 写回 PC。*/
void pc_link_send_reply_to_pc(const uint8_t *frame, uint16_t len);
#ifdef __cplusplus
}
#endif
#endif /* PC_LINK_H */

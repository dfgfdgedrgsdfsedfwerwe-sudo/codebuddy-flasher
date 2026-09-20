/*
 * led_indicator.h - LED 状态指示器接口
 *
 * 功能：
 * - 显示系统状态（初始化、就绪、错误）
 * - 指示数据传输活动（ESP-NOW 接收/USB 发送）
 * - 非阻塞实现，独立 FreeRTOS 任务控制
 */

#ifndef LED_INDICATOR_H
#define LED_INDICATOR_H

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* LED 工作模式 */
typedef enum {
    LED_OFF = 0,        // 关闭
    LED_ON,             // 常亮
    LED_SLOW_BLINK,     // 慢闪 (1Hz) - 初始化中 (蓝色)
    LED_FAST_BLINK,     // 快闪 (5Hz) - 错误状态 (红色)
    LED_HEARTBEAT,      // 心跳 (双闪) - 正常工作 (绿色)
    LED_DATA_BLINK      // 数据活动快闪 (橙色)
} led_mode_t;

/**
 * @brief 初始化 LED 指示器
 *
 * 创建 LED 控制任务，配置 GPIO 输出
 *
 * @return ESP_OK 成功
 *         ESP_FAIL 失败
 */
esp_err_t led_indicator_init(void);

/**
 * @brief 设置 LED 工作模式
 *
 * @param mode LED 模式（LED_OFF, LED_ON, LED_SLOW_BLINK 等）
 */
void led_set_mode(led_mode_t mode);

/**
 * @brief 获取当前 LED 工作模式
 *
 * @return 当前 LED 模式
 */
led_mode_t led_get_mode(void);

/**
 * @brief 指示数据活动（接收或发送）
 *
 * 调用后 LED 临时切换到橙色快闪状态，持续约 500ms 后自动恢复之前模式
 * 适合在 ESP-NOW 接收回调或 USB 发送完成时调用
 */
void led_indicate_data_activity(void);

/**
 * @brief 清理 LED 指示器资源
 *
 * 删除任务、释放 LED Strip 资源
 */
void led_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* LED_INDICATOR_H */

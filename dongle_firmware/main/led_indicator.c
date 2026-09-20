/*
 * led_indicator.c - WS2812 RGB LED 状态指示器实现
 *
 * 硬件: GPIO48 驱动 WS2812 可编址 RGB LED
 *
 * 工作原理：
 * - 使用 ESP-IDF led_strip 组件通过 RMT 驱动 WS2812
 * - 独立 FreeRTOS 任务控制 LED 闪烁模式和颜色
 * - 数据活动时自动切换到快闪，500ms 后恢复基础模式
 * - 非阻塞设计，不影响主任务性能
 */

#include "led_indicator.h"
#include "config.h"
#include "led_strip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"

static const char *TAG = "led";

/* WS2812 LED Strip 句柄 */
static led_strip_handle_t g_led_strip = NULL;

/* 全局状态 */
static led_mode_t g_current_mode = LED_OFF;
static led_mode_t g_base_mode = LED_OFF;  // 数据活动前的基础模式
static SemaphoreHandle_t g_mode_mutex = NULL;
static uint32_t g_data_activity_counter = 0;  // 数据活动计数器

/* LED 任务句柄 */
static TaskHandle_t g_led_task_handle = NULL;

/* ============================================================
 * WS2812 RGB 颜色定义
 * ============================================================ */

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb_color_t;

// 状态颜色定义
static const rgb_color_t COLOR_OFF       = {0, 0, 0};      // 关闭
static const rgb_color_t COLOR_INIT      = {0, 0, 50};     // 初始化：蓝色
static const rgb_color_t COLOR_ERROR     = {50, 0, 0};     // 错误：红色
static const rgb_color_t COLOR_HEARTBEAT = {0, 20, 0};     // 心跳：绿色
static const rgb_color_t COLOR_DATA      = {30, 15, 0};    // 数据传输：橙色

/* ============================================================
 * LED 控制函数
 * ============================================================ */

static void led_set_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    if (g_led_strip) {
        led_strip_set_pixel(g_led_strip, 0, r, g, b);
        led_strip_refresh(g_led_strip);
    }
}

static void led_set_color(const rgb_color_t *color)
{
    led_set_rgb(color->r, color->g, color->b);
}

static void led_off(void)
{
    led_set_color(&COLOR_OFF);
}

/* ============================================================
 * LED 任务 - 根据模式控制闪烁
 * ============================================================ */

static void led_task(void *param)
{
    (void)param;
    uint32_t tick = 0;
    uint8_t heartbeat_phase = 0;  // 心跳阶段：0-3

    while (1) {
        xSemaphoreTake(g_mode_mutex, portMAX_DELAY);
        led_mode_t mode = g_current_mode;

        // 检查数据活动超时（500ms 无活动后恢复基础模式）
        if (g_data_activity_counter > 0) {
            g_data_activity_counter--;
            if (g_data_activity_counter == 0 && mode == LED_DATA_BLINK) {
                g_current_mode = g_base_mode;
                mode = g_base_mode;
            }
        }
        xSemaphoreGive(g_mode_mutex);

        switch (mode) {
            case LED_OFF:
                led_off();
                break;

            case LED_ON:
                led_set_color(&COLOR_HEARTBEAT);
                break;

            case LED_SLOW_BLINK:
                // 慢闪 1Hz (500ms on / 500ms off) - 蓝色
                if (tick % 100 < 50) {
                    led_set_color(&COLOR_INIT);
                } else {
                    led_off();
                }
                break;

            case LED_FAST_BLINK:
                // 快闪 5Hz (100ms on / 100ms off) - 红色
                if (tick % 20 < 10) {
                    led_set_color(&COLOR_ERROR);
                } else {
                    led_off();
                }
                break;

            case LED_HEARTBEAT:
                // 心跳模式 (100ms on / 100ms off / 100ms on / 700ms off) - 绿色
                heartbeat_phase = (tick % 100) / 10;  // 0-9
                if (heartbeat_phase == 0 || heartbeat_phase == 2) {
                    led_set_color(&COLOR_HEARTBEAT);
                } else {
                    led_off();
                }
                break;

            case LED_DATA_BLINK:
                // 数据传输快闪 - 橙色
                if (tick % 10 < 5) {
                    led_set_color(&COLOR_DATA);
                } else {
                    led_off();
                }
                break;

            default:
                led_off();
                break;
        }

        tick++;
        vTaskDelay(pdMS_TO_TICKS(10));  // 10ms 节拍
    }
}

/* ============================================================
 * 公共 API
 * ============================================================ */

esp_err_t led_indicator_init(void)
{
    // 创建互斥锁
    g_mode_mutex = xSemaphoreCreateMutex();
    if (!g_mode_mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_FAIL;
    }

    // 配置 WS2812 LED Strip (RMT 驱动)
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_GPIO,      // GPIO 48
        .max_leds = 1,                   // 只有 1 个 LED
        .led_model = LED_MODEL_WS2812,   // WS2812 型号
        .flags.invert_out = false,
    };

    led_strip_rmt_config_t rmt_config = {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,  // 10MHz
        .flags.with_dma = false,
#else
        .rmt_channel = 0,
#endif
    };

    esp_err_t ret = led_strip_new_rmt_device(&strip_config, &rmt_config, &g_led_strip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create WS2812 LED strip: %s", esp_err_to_name(ret));
        return ret;
    }

    // 清空 LED
    led_strip_clear(g_led_strip);

    ESP_LOGI(TAG, "WS2812 LED strip initialized on GPIO %d", LED_GPIO);

    // 创建 LED 控制任务
    BaseType_t task_ret = xTaskCreatePinnedToCore(
        led_task,
        "led_task",
        2048,
        NULL,
        3,  // 优先级 3 (低于 USB 任务 5)
        &g_led_task_handle,
        0   // 固定在 Core 0
    );

    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create LED task");
        led_strip_del(g_led_strip);
        g_led_strip = NULL;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "LED indicator initialized on GPIO %d", LED_GPIO);
    return ESP_OK;
}

void led_set_mode(led_mode_t mode)
{
    if (g_mode_mutex) {
        xSemaphoreTake(g_mode_mutex, portMAX_DELAY);
        g_current_mode = mode;
        g_base_mode = mode;  // 更新基础模式
        g_data_activity_counter = 0;  // 重置数据活动计数
        xSemaphoreGive(g_mode_mutex);
    }
}

led_mode_t led_get_mode(void)
{
    led_mode_t mode = LED_OFF;
    if (g_mode_mutex) {
        xSemaphoreTake(g_mode_mutex, portMAX_DELAY);
        mode = g_current_mode;
        xSemaphoreGive(g_mode_mutex);
    }
    return mode;
}

void led_indicate_data_activity(void)
{
    if (g_mode_mutex) {
        xSemaphoreTake(g_mode_mutex, portMAX_DELAY);

        // 保存当前基础模式（如果不是数据闪烁模式）
        if (g_current_mode != LED_DATA_BLINK) {
            g_base_mode = g_current_mode;
        }

        // 切换到数据闪烁模式
        g_current_mode = LED_DATA_BLINK;

        // 重置超时计数器 (500ms = 50 * 10ms)
        g_data_activity_counter = 50;

        xSemaphoreGive(g_mode_mutex);
    }
}

void led_deinit(void)
{
    // 删除任务
    if (g_led_task_handle) {
        vTaskDelete(g_led_task_handle);
        g_led_task_handle = NULL;
    }

    // 删除 LED Strip
    if (g_led_strip) {
        led_strip_clear(g_led_strip);
        led_strip_del(g_led_strip);
        g_led_strip = NULL;
    }

    // 删除互斥锁
    if (g_mode_mutex) {
        vSemaphoreDelete(g_mode_mutex);
        g_mode_mutex = NULL;
    }
}

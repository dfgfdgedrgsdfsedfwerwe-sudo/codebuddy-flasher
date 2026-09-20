/*
 * rgb_led.h - K10 WS2812 RGB LED 控制接口
 *
 * 硬件: GPIO46 驱动 3 颗侧发光 WS2812 RGB LED
 *
 * 功能：
 * - 系统状态指示（初始化、就绪、错误）
 * - 录音/语音活动指示
 * - 演示模式视觉反馈
 * - 非阻塞呼吸灯、流水灯效果
 */

#ifndef RGB_LED_H
#define RGB_LED_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

/* K10 WS2812 配置 */
#define RGB_LED_PIN       46        // GPIO46 - WS2812 数据引脚
#define RGB_LED_COUNT     3         // 3 颗侧发光 LED

/* LED 工作模式 */
typedef enum {
    LED_MODE_OFF = 0,               // 关闭
    LED_MODE_SOLID,                 // 单色常亮
    LED_MODE_BREATHING,             // 呼吸灯效果
    LED_MODE_FLOW,                  // 流水灯效果
    LED_MODE_BLINK,                 // 闪烁
    LED_MODE_PULSE                  // 脉冲（短闪后熄灭）
} rgb_led_mode_t;

/* 颜色结构（避免与 LovyanGFX 的 RGBColor 冲突，使用 LEDColor） */
struct LEDColor {
    uint8_t r;
    uint8_t g;
    uint8_t b;

    LEDColor(uint8_t red = 0, uint8_t green = 0, uint8_t blue = 0)
        : r(red), g(green), b(blue) {}

    uint32_t toUint32() const {
        return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    }
};

// 系统状态色
extern const LEDColor COLOR_OFF;
extern const LEDColor COLOR_INIT;       // 蓝色 (0, 50, 255): 初始化中
extern const LEDColor COLOR_READY;      // 绿色 (0, 255, 50): 就绪状态
extern const LEDColor COLOR_ERROR;      // 红色 (255, 0, 0): 错误
extern const LEDColor COLOR_RECORDING;  // 橙色 (255, 100, 0): 录音/语音活动
extern const LEDColor COLOR_DEMO;       // 紫色 (180, 0, 255): 演示模式
extern const LEDColor COLOR_AUDIO_ON;   // 青色 (0, 255, 200): 音频流激活

/*
 * rgb_led_init - 初始化 WS2812 LED
 *
 * 必须在 setup() 中调用，初始化后 LED 处于关闭状态
 */
void rgb_led_init(void);

/*
 * rgb_led_set_color - 设置所有 LED 为同一颜色
 *
 * 参数：
 *   color: 目标颜色
 *
 * 注：立即生效，会中断当前动画
 */
void rgb_led_set_color(const LEDColor &color);

/*
 * rgb_led_set_pixel - 设置单个 LED 颜色
 *
 * 参数：
 *   index: LED 索引 (0-2)
 *   color: 目标颜色
 */
void rgb_led_set_pixel(uint8_t index, const LEDColor &color);

/*
 * rgb_led_set_mode - 设置 LED 工作模式
 *
 * 参数：
 *   mode:  工作模式（OFF/SOLID/BREATHING/FLOW/BLINK/PULSE）
 *   color: 目标颜色
 *   speed: 动画周期（ms），默认 1000ms
 *          - BREATHING: 完整呼吸周期
 *          - FLOW: 流水灯移动间隔
 *          - BLINK: 闪烁周期
 */
void rgb_led_set_mode(rgb_led_mode_t mode, const LEDColor &color, uint16_t speed = 1000);

/*
 * rgb_led_update - 更新 LED 状态（非阻塞）
 *
 * 必须在 loop() 中定期调用以驱动动画效果
 */
void rgb_led_update(void);

/*
 * rgb_led_indicate_recording - 录音指示
 *
 * 橙色脉冲效果，持续 300ms 后恢复之前状态
 */
void rgb_led_indicate_recording(void);

/*
 * rgb_led_indicate_key_press - 按键反馈
 *
 * 参数：
 *   color: 反馈颜色
 *
 * 短时闪烁（150ms），然后恢复之前状态
 */
void rgb_led_indicate_key_press(const LEDColor &color);

#endif // RGB_LED_H

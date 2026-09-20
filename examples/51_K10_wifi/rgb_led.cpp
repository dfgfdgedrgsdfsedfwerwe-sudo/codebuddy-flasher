/*
 * rgb_led.cpp - K10 WS2812 RGB LED 控制实现
 *
 * 硬件: GPIO46 驱动 3 颗侧发光 WS2812 RGB LED
 * 驱动: Adafruit NeoPixel (RMT backend on ESP32-S3)
 *
 * 非阻塞设计：所有动画通过 rgb_led_update() 在 loop() 中驱动
 */

#include "rgb_led.h"

// 预定义颜色（亮度控制在 30 以下，避免刺眼）
const LEDColor COLOR_OFF       = {0,   0,   0};
const LEDColor COLOR_INIT      = {0,   0,   30};   // 蓝色
const LEDColor COLOR_READY     = {0,   20,  0};    // 绿色
const LEDColor COLOR_ERROR     = {30,  0,   0};    // 红色
const LEDColor COLOR_RECORDING = {30,  15,  0};    // 橙色
const LEDColor COLOR_DEMO      = {20,  0,   25};   // 紫色
const LEDColor COLOR_AUDIO_ON  = {0,   20,  20};   // 青色

static Adafruit_NeoPixel strip(RGB_LED_COUNT, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

// 当前动画状态
static rgb_led_mode_t current_mode = LED_MODE_OFF;
static LEDColor current_color = {0, 0, 0};
static uint16_t current_speed = 1000;
static uint32_t last_update_ms = 0;
static uint8_t animation_phase = 0;
static uint8_t brightness_level = 0;
static bool brightness_rising = true;

// 脉冲效果状态
static bool pulse_active = false;
static LEDColor pulse_color = {0, 0, 0};
static uint32_t pulse_start_ms = 0;
static rgb_led_mode_t pre_pulse_mode = LED_MODE_OFF;
static LEDColor pre_pulse_color = {0, 0, 0};
static uint16_t pre_pulse_speed = 1000;

static void apply_all(uint8_t r, uint8_t g, uint8_t b) {
    for (int i = 0; i < RGB_LED_COUNT; i++) {
        strip.setPixelColor(i, strip.Color(r, g, b));
    }
    strip.show();
}

static void apply_color(const LEDColor &c) {
    apply_all(c.r, c.g, c.b);
}

static void apply_color_scaled(const LEDColor &c, uint8_t scale) {
    uint8_t r = (uint16_t)c.r * scale / 255;
    uint8_t g = (uint16_t)c.g * scale / 255;
    uint8_t b = (uint16_t)c.b * scale / 255;
    apply_all(r, g, b);
}

void rgb_led_init(void) {
    strip.begin();
    strip.setBrightness(255);
    apply_all(0, 0, 0);
    Serial.println("RGB LED initialized on GPIO46 (3x WS2812)");
}

void rgb_led_set_color(const LEDColor &color) {
    current_mode = LED_MODE_SOLID;
    current_color = color;
    pulse_active = false;
    apply_color(color);
}

void rgb_led_set_pixel(uint8_t index, const LEDColor &color) {
    if (index >= RGB_LED_COUNT) return;
    strip.setPixelColor(index, strip.Color(color.r, color.g, color.b));
    strip.show();
}

void rgb_led_set_mode(rgb_led_mode_t mode, const LEDColor &color, uint16_t speed) {
    current_mode = mode;
    current_color = color;
    current_speed = speed;
    animation_phase = 0;
    brightness_level = 0;
    brightness_rising = true;
    last_update_ms = millis();
    pulse_active = false;

    if (mode == LED_MODE_OFF) {
        apply_all(0, 0, 0);
    } else if (mode == LED_MODE_SOLID) {
        apply_color(color);
    }
}

void rgb_led_off(void) {
    current_mode = LED_MODE_OFF;
    pulse_active = false;
    apply_all(0, 0, 0);
}

void rgb_led_indicate_recording(void) {
    if (!pulse_active) {
        pre_pulse_mode = current_mode;
        pre_pulse_color = current_color;
        pre_pulse_speed = current_speed;
    }
    pulse_active = true;
    pulse_color = COLOR_RECORDING;
    pulse_start_ms = millis();
    apply_color(COLOR_RECORDING);
}

void rgb_led_indicate_key_press(const LEDColor &color) {
    if (!pulse_active) {
        pre_pulse_mode = current_mode;
        pre_pulse_color = current_color;
        pre_pulse_speed = current_speed;
    }
    pulse_active = true;
    pulse_color = color;
    pulse_start_ms = millis();
    apply_color(color);
}

void rgb_led_update(void) {
    uint32_t now = millis();

    // 脉冲效果处理（优先级最高）
    if (pulse_active) {
        uint32_t elapsed = now - pulse_start_ms;
        if (elapsed < 150) {
            // 亮 150ms
            apply_color(pulse_color);
        } else if (elapsed < 300) {
            // 灭 150ms
            apply_all(0, 0, 0);
        } else if (elapsed < 500) {
            // 再亮 200ms
            apply_color(pulse_color);
        } else {
            // 脉冲结束，恢复之前模式
            pulse_active = false;
            current_mode = pre_pulse_mode;
            current_color = pre_pulse_color;
            current_speed = pre_pulse_speed;
            if (current_mode == LED_MODE_SOLID) {
                apply_color(current_color);
            } else if (current_mode == LED_MODE_OFF) {
                apply_all(0, 0, 0);
            }
        }
        return;
    }

    switch (current_mode) {
        case LED_MODE_OFF:
        case LED_MODE_SOLID:
            break;

        case LED_MODE_BREATHING: {
            // 呼吸灯: 10ms 步进，256 级亮度
            uint16_t step_ms = current_speed / 512;
            if (step_ms < 4) step_ms = 4;

            if (now - last_update_ms >= step_ms) {
                last_update_ms = now;
                if (brightness_rising) {
                    brightness_level += 2;
                    if (brightness_level >= 254) {
                        brightness_level = 255;
                        brightness_rising = false;
                    }
                } else {
                    if (brightness_level <= 2) {
                        brightness_level = 0;
                        brightness_rising = true;
                    } else {
                        brightness_level -= 2;
                    }
                }
                // gamma 校正近似: 让低亮度段变化更明显
                uint8_t gamma = (uint16_t)brightness_level * brightness_level / 255;
                apply_color_scaled(current_color, gamma);
            }
            break;
        }

        case LED_MODE_FLOW: {
            // 流水灯: 每 speed/3 ms 切换到下一个 LED
            uint16_t step_ms = current_speed / 3;
            if (now - last_update_ms >= step_ms) {
                last_update_ms = now;
                for (int i = 0; i < RGB_LED_COUNT; i++) {
                    if (i == animation_phase) {
                        strip.setPixelColor(i, strip.Color(
                            current_color.r, current_color.g, current_color.b));
                    } else {
                        strip.setPixelColor(i, 0);
                    }
                }
                strip.show();
                animation_phase = (animation_phase + 1) % RGB_LED_COUNT;
            }
            break;
        }

        case LED_MODE_BLINK: {
            // 闪烁: 亮 speed/2 ms -> 灭 speed/2 ms
            if (now - last_update_ms >= current_speed / 2) {
                last_update_ms = now;
                animation_phase = !animation_phase;
                if (animation_phase) {
                    apply_color(current_color);
                } else {
                    apply_all(0, 0, 0);
                }
            }
            break;
        }

        case LED_MODE_PULSE:
            break;

        default:
            break;
    }
}

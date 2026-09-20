/**
 * @file 53_esp32-fluidbox.ino
 * @brief FluidBox 3D粒子流体移植到 ATK BOX (240×320 ST7789, 触摸交互)
 *
 * 原项目: https://github.com/V4C38/esp32-fluidbox
 * 移植适配:
 *   - 368×448 → 240×320 分辨率
 *   - QSPI AMOLED → 8080并口 TFT (LovyanGFX)
 *   - QMI8658 IMU → CHSC5432电容触摸屏 (拨动/滑动控制流体)
 *   - ESP-IDF双核任务 → Arduino setup()/loop() + FreeRTOS
 *
 * 交互方式:
 *   - 触摸拖拽: 重力方向指向触摸点 (拨动液体效果)
 *   - 滑动: 产生旋转漩涡 (角速度omega)
 *   - 松手: 2秒平滑衰减回自动演示模式
 *
 * 核心物理引擎(sim.c/render.c)保持不变，仅适配显示和输入层
 */

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

// ATK BOX硬件层
#include "../52_codebuddy_ai_box/AtkBoxTouch.h"
#include "../52_codebuddy_ai_box/AtkBoxXL9555.h"

// FluidBox核心模块
extern "C" {
    #include "config.h"
    #include "sim.h"
    #include "render.h"
    #include "atk_display.h"
    #include "atk_touch_fluid.h"
}

// 实例化全局硬件对象 (定义在 AtkBoxTouch.h / AtkBoxXL9555.h)
AtkTouch touch;
AtkXL9555 xl9555;

// 统计计数器
static volatile uint32_t s_frames = 0;
static volatile uint32_t s_steps = 0;

// 渲染任务 (Core 0: 绘制粒子到LCD)
static void render_task(void *arg) {
    (void)arg;
    for (;;) {
        render_frame();
        s_frames++;
        vTaskDelay(1);  // 让看门狗保持喂养
    }
}

// 触摸扫描任务 (Core 1: 高频率轮询触摸屏)
static void touch_task(void *arg) {
    (void)arg;
    uint16_t touch_x = 0, touch_y = 0;

    for (;;) {
        bool touch_active = touch.scan(&touch_x, &touch_y);
        touch_fluid_update(touch_active, touch_x, touch_y);

        vTaskDelay(pdMS_TO_TICKS(10));  // 100Hz扫描率
    }
}

// 模拟任务 (Core 1: 物理计算 + 触摸驱动力)
static void sim_task(void *arg) {
    (void)arg;

    // 初始重力: 向下
    sim_forces_t forces = {
        .gravity = {0.0f, GRAVITY_GAIN * GRAVITY_MPS2 * PX_PER_METER, 0.0f},
        .omega = {0.0f, 0.0f, 0.0f},
        .alpha = {0.0f, 0.0f, 0.0f},
    };

    int64_t last_us = esp_timer_get_time();

    for (;;) {
        const int64_t now = esp_timer_get_time();
        float dt = (float)(now - last_us) * 1e-6f;
        last_us = now;

        // 限制dt避免大步长
        if (dt > 0.05f) dt = 0.05f;
        else if (dt < 1e-4f) dt = 1e-4f;

        // 触摸交互更新重力/角速度
        touch_fluid_read(dt, &forces);

        sim_step(dt, &forces);
        s_steps++;

        vTaskDelay(1);
    }
}

// 统计日志任务
static void stats_task(void *arg) {
    (void)arg;
    uint32_t last_frames = 0, last_steps = 0;
    int64_t last_us = esp_timer_get_time();

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(2000));

        const int64_t now = esp_timer_get_time();
        const float elapsed = (float)(now - last_us) * 1e-6f;
        last_us = now;

        const uint32_t frames = s_frames;
        const uint32_t steps = s_steps;

        sim_stats_t st;
        sim_stats(&st);

        Serial.printf("%.1f fps | %.1f steps/s | grid %d dens %d relax %d us | "
                      "rho %.2f/%.2f | speed avg %.0f max %.0f | sram %u\n",
                      (float)(frames - last_frames) / elapsed,
                      (float)(steps - last_steps) / elapsed,
                      st.us_grid, st.us_density, st.us_relax,
                      st.mean_density, st.rest_density,
                      st.mean_speed, st.max_speed,
                      (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));

        last_frames = frames;
        last_steps = steps;
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("\n>>> FluidBox for ATK BOX starting <<<");

    // 初始化XL9555 IO扩展 (背光 + 触摸复位)
    // 注意: begin()无参数，内部会初始化Wire(48,45)
    if (!xl9555.begin()) {
        Serial.println("[FATAL] XL9555 init failed");
        while (1) delay(1000);
    }
    // XL9555管脚默认配置即可，无需pinMode (52例从不调用pinMode16)
    xl9555.digitalWrite16(ATK_LCD_BL_IO, 1);  // 开背光

    // XL9555已初始化Wire，无需再调用Wire.begin()

    // 初始化FluidBox模块 (atk_display_init会初始化LCD硬件)
    atk_display_init();
    touch_fluid_init();   // 触摸交互状态机
    sim_init();
    render_init();

    // 初始化触摸屏
    if (!touch.begin(Wire)) {
        Serial.println("[WARN] Touch init failed, auto mode only");
    } else {
        Serial.println("[OK] Touch ready");
    }

    // 创建四个任务
    xTaskCreatePinnedToCore(sim_task, "sim", 4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(render_task, "render", 4096, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(touch_task, "touch", 2048, NULL, 4, NULL, 1);
    xTaskCreatePinnedToCore(stats_task, "stats", 2048, NULL, 3, NULL, 1);

    Serial.println("FluidBox tasks started. Touch to control fluid!");
}

void loop() {
    // 主循环空闲, 所有工作在FreeRTOS任务中
    vTaskDelay(pdMS_TO_TICKS(1000));
}

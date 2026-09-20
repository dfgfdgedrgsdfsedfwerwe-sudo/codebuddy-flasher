/**
 * @file camera.ino
 * @brief The running example will display the camera feed on the screen
 *
 * @copyright   Copyright (c) 2010 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @license     The MIT License (MIT)
 * @author tangjie(jie.tang@dfrobot.com)
 * @version V1.0
 * @date 2024-6-21
 */

// #include "unihiker_k10.h"
#include "main.h"
#include <demos/lv_demos.h>
#include <examples/lv_examples.h>

LV_IMG_DECLARE(output)
SemaphoreHandle_t xGuiSemaphore;

// UNIHIKER_K10 k10;
void setup() {
    Serial.begin(115200);
    pinMode(0, OUTPUT);  // suppress the noise
    digitalWrite(0, LOW);
    /* Initialize the LVGL */
    lvgl_begin();
    xGuiSemaphore = xSemaphoreCreateMutex();
    xSemaphoreGive(xGuiSemaphore);

    // lv_demo_benchmark();
    lv_obj_t *bg = lv_obj_create(lv_scr_act());
    lv_obj_set_size(bg, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_pad_all(bg, 0, 0);
    lv_obj_set_style_bg_color(bg, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(bg, LV_OPA_100, 0);
    lv_obj_clear_flag(bg, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *camImage = lv_gif_create(lv_scr_act());
    // lv_obj_set_style_pad_all(bg, 0, 0);
    lv_obj_set_style_bg_color(camImage, lv_color_black(), 0);
    // lv_obj_set_style_bg_opa(bg, LV_OPA_100, 0);
    lv_obj_clear_flag(camImage, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_gif_set_src(camImage, &output);
    lv_obj_center(camImage);
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(10));
    /* Try to take the semaphore, call lvgl related function on success */
    if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) {
        lv_task_handler();
        xSemaphoreGive(xGuiSemaphore);
    }

    /* Try to take the semaphore, call lvgl related function on success */
}
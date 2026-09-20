
#include "SD_Card.h"
// #include "Audio_PCM5101.h"
#include "LVGL_Music.h"
#include "main.h"
#include "Button_handler.h"
// TFT_eSPI tft = TFT_eSPI();  // Invoke custom library
static const uint16_t screenWidth  = 240;
static const uint16_t screenHeight = 320;
SemaphoreHandle_t xGuiSemaphore;

// ESP_Panel *panel = NULL;
// static TFT_eSPI tft = TFT_eSPI(screenWidth, screenHeight); /* TFT instance */
// static K10_Lcd tft;

SdCard tf;
static void Music_create(lv_obj_t *parent) {
    static lv_obj_t *ctrl;
    //   original_screen_bg_color = lv_obj_get_style_bg_color(parent, 0);
    //   lv_obj_set_style_bg_color(parent, lv_color_hex(0x343247), 0);
    // lv_obj_clear_flag(ctrl,LV_OBJ_FLAG_SCROLLABLE);
    ctrl = _lv_demo_music_main_create(parent);
}

Button *buttonA = new Button(eP5_KeyA);

Button *buttonB = new Button(eP11_KeyB);
Button *buttonUp = new Button(eP9);
Button *buttonDw = new Button(eP15);

Button *buttonAB = new Button(eP5_KeyA, eP11_KeyB);

void KEYB(void)  // Callback function for button B
{
    if (audio.isRunning()) {
        _lv_demo_music_pause();
    } else {
        _lv_demo_music_resume();
    }
}

void KEYA(void)  // Callback function for button A
{
    _lv_demo_music_album_next(false);
}

void KEYAB(void) {
    _lv_demo_music_album_next(true);
}
void KEYUP(void) {
    _lv_demo_music_album_next(true);
}


TimerHandle_t xAutoSwitchTimer = NULL;
void vTimerCallback(TimerHandle_t pxTimer) {
    _lv_demo_music_album_next(true);
}
void setup(void) {
    Serial.begin(115200);
    tf.init();
    lvgl_begin();
    xGuiSemaphore = xSemaphoreCreateMutex();
    xSemaphoreGive(xGuiSemaphore);  // // void SD_Init();
    // tf.init();
    // tf.listDir("/", 0);
    buttonA->setPressedCallback(&KEYA);
    buttonB->setPressedCallback(&KEYB);
    buttonAB->setPressedCallback(&KEYAB);
    // buttonUp->setPressedCallback(&KEYA);
    // buttonDw->setPressedCallback(&KEYB);
    // lv_demo_music(); /* A modern, smartphone-like music player demo.
    // lv_demo_stress();       /* A stress test for LVGL. */
    // lv_demo_benchmark();

    Music_create(lv_scr_act());
    Audio_Init();

    xAutoSwitchTimer =
        xTimerCreate("AutoSwitchTimer",    // 定时器名称
                     pdMS_TO_TICKS(2000),  // 定时器周期，2000ms = 2s
                     pdTRUE,               // 自动重载
                     (void *)0,            // 定时器ID
                     vTimerCallback        // 定时器回调函数
        );

    // 启动定时器
    // if (xAutoSwitchTimer != NULL) {
    //     xTimerStart(xAutoSwitchTimer, 0);
    // }
}

void loop() {
    // Capture microphone data and print it via serial
    vTaskDelay(pdMS_TO_TICKS(10));
    /* Try to take the semaphore, call lvgl related function on success */
    if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) {
        lv_task_handler();
        xSemaphoreGive(xGuiSemaphore);
    }
}
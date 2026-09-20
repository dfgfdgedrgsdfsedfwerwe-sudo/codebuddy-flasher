#ifndef MAIN_H_
#define MAIN_H_

// #include <LGFX_AUTODETECT.hpp>
#include <LovyanGFX.hpp>
// #include <M5GFX.h>
#include "lvgl.h"
#include "Lcd_handler.hpp"
#include "initBoard.h"
// #include "lvgl/examples/lv_examples.h"
// #include "lvgl/demos/lv_demos.h"
#include "examples/lv_examples.h"
#include "demos/lv_demos.h"
#include "lvgl_animation.hpp"
#include "Clock/TimerController.h"
// #include "Weather/WeatherController.h"
#include "Bubble/BubbleController.h"
// #include "Fireworks/FireworksController.h"
// #include "Enviroment/EnviromentController.h"
// #include "Conway/GameController.h"
#include "MpuSensor/MpuController.h"

// #include "ssd1106.hpp"
// static SSD1106 Lcd;
K10_Lcd Lcd;

#define LVGL_HOR_RES (240)
#define LVGL_VER_RES (320)

static uint8_t currentScreen = 2;
static void lvgl_begin(void);
static void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area,
                          lv_color_t *color_p);
// OneButton buttonNext(2, true, true);
static void changeScreen();
static void btnTask(void *param);

// MonitorController monitorController;
TimerController timerController;
// WeatherController weatherController;
BubbleController bubbleController;
// FireworksController fireworksController;
// EnviromentController enviromentController;
// GameController gameController;
MpuController mpuController;
// lv_disp_t *Lcd; /*Descriptor of a display driver*/
static void lvgl_begin(void)
{

    // #define DISP_BUF_SIZE (LVGL_HOR_RES * 100)
    init_board();
    Lcd.init();
    // lcd.setRotation(2);
    // lcd.setBrightness(100);
    lv_init();

    static lv_disp_draw_buf_t draw_buf;
    size_t DISP_BUF_SIZE = sizeof(lv_color_t) * (LVGL_HOR_RES * 20); // best operation.
    // static lv_color_t *buf1 = (lv_color_t *)heap_caps_malloc(
    //     DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    // static lv_color_t *buf2 = (lv_color_t *)heap_caps_malloc(
    //     DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);

    static lv_color_t *buf1 = (lv_color_t *)heap_caps_malloc( // most fast operation.
        DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
    static lv_color_t *buf2 = (lv_color_t *)heap_caps_malloc(
        DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);

    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, DISP_BUF_SIZE); /*Initialize the display buffer*/

    /*-----------------------------------
     * Register the display in LVGL
     *----------------------------------*/

    static lv_disp_drv_t disp_drv; /*Descriptor of a display driver*/
    lv_disp_drv_init(&disp_drv);   /*Basic initialization*/

    /*Set up the functions to access to your display*/
    /*Set the resolution of the display*/
    disp_drv.hor_res = LVGL_HOR_RES;
    disp_drv.ver_res = LVGL_VER_RES;
    /*Used to copy the buffer's content to the display*/
    disp_drv.flush_cb = my_disp_flush;
    /*Set a display buffer*/
    disp_drv.draw_buf = &draw_buf;
    /*Finally register the driver*/
    lv_disp_drv_register(&disp_drv);
}

static void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area,
                          lv_color_t *color_p)
{
    int w = (area->x2 - area->x1 + 1);
    int h = (area->y2 - area->y1 + 1);
    /* Start new TFT transaction */
    Lcd.startWrite();
    /* set the working window */
    Lcd.setAddrWindow(area->x1, area->y1, w, h);
    // Lcd.writePixelsDMA((lgfx::grayscale_t *)&color_p->full, w * h);
    Lcd.writePixelsDMA((lgfx::rgb565_t *)&color_p->full, w * h);
    /* terminate TFT transaction */
    Lcd.endWrite();
    /* tell lvgl that flushing is done */
    lv_disp_flush_ready(disp);
}



static void btnTask(void *param)
{
    while (1)
    {
        // buttonOK.tick();
        // buttonNext.tick();
        delay(10);
    }
    vTaskDelete(NULL);
}

static void changeScreen()
{
    // monitorController.model.speaker.beep();
    switch ((++currentScreen) % 3)
    // switch ((currentScreen) % 2)
    {
    case 0:
        bubbleController.onViewDisappear();
        // fireworksController.onViewDisappear();   
        timerController.onViewLoad();
        break;
    case 1:
        timerController.onViewDisappear();
        mpuController.onViewLoad();
        break;

    case 2:
        mpuController.onViewDisappear();        
        bubbleController.onViewLoad();
        
        break;

    default:
        break;
    }
}



#endif
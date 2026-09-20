#ifndef BUBBLE_VIEW_H_
#define BUBBLE_VIEW_H_

#if defined __has_include
#if __has_include("lvgl.h")
#include "lvgl.h"
#elif __has_include("lvgl/lvgl.h")
#include "lvgl/lvgl.h"
#else
#include "lvgl.h"
#endif
#else
#include "lvgl.h"
#endif
#include "Arduino.h"

// LV_IMG_DECLARE(img_bulb_gif);
typedef struct
{
    uint32_t r;
    uint32_t x;
    uint32_t y;
    uint32_t dx;
    uint32_t dy;
    uint32_t color;
    void move()
    {
        x += dx;
        y += dy;
        if ((x <= 0) | // fix the area for the bouncing ball.
            (x >= LV_HOR_RES - r))
        {
            dx *= -1;
            // color = ; // when it hit the border it will change the color too.
        }
        if ((y <= 0) |
            (y >= LV_VER_RES - r))
        {
            dy *= -1;
            // color = (uint32_t)rand() | 0x808080U;
        }
    }
} bubble_t;

typedef struct
{
    uint32_t r;
    uint32_t x;
    uint32_t y;
    uint32_t dr;
    // uint32_t dy;
    uint32_t color;
    void move()
    {
        r += dr;
        if (r > LV_HOR_RES * 2 / 3)
        {
            x = random(5, LV_HOR_RES);
            y = random(5, LV_VER_RES);
            // r = random(3,50);
            r = 0;
            // dr= random(1,3);
        }
    }
} bb_t;

class BubbleView
{
public:
    struct
    {
        lv_obj_t *root;
        lv_obj_t *game_station;
        lv_obj_t *acc_data;
        lv_obj_t *labelaccX;
        lv_obj_t *labelaccY;
        lv_obj_t *labelaccZ;
        lv_obj_t *labellux;
        lv_obj_t *bulb;
    } ui;

    // static bb_t bbarray[10];
    BubbleView();
    ~BubbleView();
    // static bb_t barray[20];
    void create();
    void load();
    void destroy();
    void delVectb();

private:
    static void game_draw_event_cb(lv_event_t *e);
};

#endif
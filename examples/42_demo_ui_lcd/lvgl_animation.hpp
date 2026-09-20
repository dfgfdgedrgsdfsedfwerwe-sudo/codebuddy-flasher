/*
 * SPDX-FileCopyrightText: 2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once
#include <math.h>
#include "lvgl.h"
#ifndef PI
#define PI (3.14159f)
#endif



// LVGL image declare
LV_IMG_DECLARE(esp_logo)
LV_IMG_DECLARE(esp_text)
LV_IMG_DECLARE(adafruit)
LV_IMG_DECLARE(pandas);
// LV_IMG_DECLARE(pigy2);
static void (*g_boot_anim_end_cb)(void) = NULL;

typedef struct
{
    lv_obj_t *scr;
    int count_val;
} my_timer_context_t;

static lv_obj_t *arc[3];
static lv_obj_t *img_logo;
static lv_obj_t *img_text;

static lv_color_t arc_color[] = {
    LV_COLOR_MAKE(100, 150, 226),
    LV_COLOR_MAKE(126, 87, 236),
    LV_COLOR_MAKE(150, 202, 228),
};

static void anim_timer_cb(lv_timer_t *timer)
{
    my_timer_context_t *timer_ctx = (my_timer_context_t *)timer->user_data;
    int count = timer_ctx->count_val;
    lv_obj_t *scr = timer_ctx->scr;

    // Play arc animation
    if (count < 90)                                                                   // start from -90; there is a period of 0;
    {                                                                                 // 第一阶: 显示弧形慢慢伸长到一个最大长度（为167），然后开始减小，在270角度，开始和结束角度重合，即弧形消失了。
        lv_coord_t arc_start = count > 0 ? (1 - cosf(count / 180.0f * PI)) * 270 : 0; // half of the time the start point is at 0.
        lv_coord_t arc_end = (sinf(count / 180.0f * PI) + 1) * 135;                   // start from  0 to 270 ,meet with the start angle.
        // 显示弧形慢慢伸长到一个最大长度（为167），然后开始减小，在270角度，开始和结束角度重合，即弧形消失了。
        for (size_t i = 0; i < sizeof(arc) / sizeof(arc[0]); i++) // 通过遍历3个弧形，实现同步伸长，但是他们的大小不一。
        {
            lv_arc_set_bg_angles(arc[i], arc_start, arc_end);
            // 要设置背景弧的开始和结束角度，请使用 lv_arc_set_bg_angles(arc, start_angle, end_angle)
            //  函数或 lv_arc_set_bg_start/end_angle(arc, angle)。
            /// 零度位于对象的中间右侧（3 点钟方向），并且度数沿顺时针方向增加。 角度应在 [0;360] 范围内。
            lv_arc_set_rotation(arc[i], (count + 120 * (i + 1)) % 360);
            // 可以使用 lv_arc_set_rotation(arc, deg) 添加到 0 度位置的偏移量。
        }
    }

    // Delete arcs when animation finished
    if (count == 90) // logo初始化阶段。
    {
        for (size_t i = 0; i < sizeof(arc) / sizeof(arc[0]); i++)
        {
            lv_obj_del(arc[i]);
        }

        // Create new image and make it transparent
        // img_text = lv_img_create(scr);
        // lv_img_set_src(img_text, &pigy2);
        // lv_obj_set_style_img_opa(img_text, 0, 0); // 两个logo的透明度均为0即不显示。
        // lv_obj_center(img_text);
        img_logo = lv_img_create(scr);
        lv_img_set_src(img_logo, &pandas);
        lv_obj_set_style_img_opa(img_logo, 0, 0);
        lv_obj_center(img_logo);
        // lv_obj_fade_in(img_logo,500,0); // 显现效果
        // lv_obj_fade_out(img_logo,500,0);
    }

    // Move images when arc animation finished
    if ((count >= 100) && (count <= 180)) // 短暂间隔后，（90 -100），两个图标开始交替显现和fade out.
    {
        // lv_coord_t offset = (sinf((count - 140) * 2.25f / 90.0f) + 1) * 10.0f; // value range from 0 - 20
        lv_coord_t offset = cosf((count - 140) * 2.250f * PI / 180) * 100.0f; // from nothing to something and to nothing.
        // (count - 140) * 2.250f 从 -90 到 90,offset 从0 到 100 再到0；

        lv_obj_set_style_img_opa(img_logo, offset / 100.0f * 255, 0); // show up gradiently
    }
    // if ((count >= 200) && (count <= 280)) // 相同原理，用cos函数来控制数值从0到100到0，下一个logo的显现时间。
    // {
    //     // lv_coord_t offset = (cosf((count - 240) *2.25f * PI/ 90) + 1)/2*100.0f; // value range from 0 - 20
    //     lv_coord_t offset = cosf((count - 240) * 2.250f * PI / 180) * 100.0f; // plus a quite big number is needed,or else it will change very quickly
    //     lv_obj_set_style_img_opa(img_text, offset / 100.0f * 255, 0);         // show up gradiently
    // }
    // Delete timer when all animation finished
    if ((count += 2) == 200) // 最后清场，跳转到下一个场景。
    {
        lv_timer_del(timer);
        lv_obj_del(img_logo);
        // lv_obj_del(img_text);
        if (g_boot_anim_end_cb)
        {
            g_boot_anim_end_cb();
        }
     
    }
    else
    {
        timer_ctx->count_val = count;
    }

}

void example_lvgl_demo_ui(void (*fn)(void))
{
    // Create image
    // img_logo = lv_img_create(scr);
    // lv_img_set_src(img_logo, &esp_logo);
    // lv_obj_center(img_logo);
 

    g_boot_anim_end_cb = fn;
    lv_obj_t *page = lv_obj_create(lv_scr_act());
    lv_obj_set_size(page, lv_obj_get_width(lv_obj_get_parent(page)), lv_obj_get_height(lv_obj_get_parent(page)));
    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(page, 0, LV_PART_MAIN);
    // lv_obj_set_stlye_bg_color(page,lv_color_white(),0);
    lv_obj_set_style_bg_color(page,lv_color_white(),0);
    lv_obj_set_style_bg_opa(page,LV_OPA_0,0);
    // Create arcs
    for (size_t i = 0; i < sizeof(arc) / sizeof(arc[0]); i++)
    {
        arc[i] = lv_arc_create(page);

        // Set arc caption
        lv_obj_set_size(arc[i], 60 - 20 * i, 60 - 20 * i); // the size of the arc are 120,90,60.
        lv_arc_set_bg_angles(arc[i], 120 * i, 10 + 120 * i);
        lv_arc_set_value(arc[i], 0); // must set to 0,else it would show blue indicator.

        // Set arc style
        // lv_obj_remove_style(arc[i], NULL, LV_PART_KNOB);
        lv_obj_set_style_arc_width(arc[i], 6, 0);
        lv_obj_set_style_arc_color(arc[i], arc_color[i], 0); // arc color ,not the value color
        lv_obj_remove_style(arc[i], NULL, LV_PART_KNOB);     /*Be sure the knob is not displayed*/
        lv_obj_clear_flag(arc[i], LV_OBJ_FLAG_CLICKABLE);
        // Make arc center
        lv_obj_center(arc[i]);
    }

    // Create timer for animation
    static my_timer_context_t my_tim_ctx = {
        .count_val = -90,
    };
    my_tim_ctx.scr = page;
    lv_timer_create(anim_timer_cb, 20, &my_tim_ctx);
}

#include "BubbleView.h"
#include <vector>

std::vector<bubble_t> vectb;
std::vector<bb_t> vectbb;

BubbleView::BubbleView()
{
}

BubbleView::~BubbleView()
{
}

void BubbleView::create()
{

    // for (bb_t &b : bbarray)
    // {
    //     b.r = lv_rand(2, 15);
    //     b.x = lv_rand(5, LV_HOR_RES);
    //     b.y = lv_rand(5, LV_VER_RES);
    //     b.dr = 1;
    // }

    /** screen */
    lv_obj_t *root = lv_obj_create(NULL);
    // lv_obj_add_style(ui.root, &style_scr, 0);
    // lv_obj_set_flex_flow(root, LV_FLEX_FLOW_ROW); // arrange as row.
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(root, LV_HOR_RES, LV_VER_RES);
    lv_obj_center(root);
    lv_obj_set_style_pad_all(root, 0, 0);
    ui.root = root;

    // lv_obj_set_flex_grow(ui.city, 1);

    static lv_style_t style;
    lv_style_init(&style);

    /*Make a gradient*/
    lv_style_set_bg_color(&style, lv_color_black());
    // lv_style_set_border_opa(&style, LV_OPA_100);
    lv_style_set_bg_opa(&style, LV_OPA_100);

    lv_obj_t *game_station = lv_obj_create(root);
    // lv_obj_remove_style_all(game_station);
    lv_obj_set_size(game_station, LV_PCT(100), LV_PCT(100));
    lv_obj_center(game_station);
    lv_obj_add_style(game_station, &style, 0);
    lv_obj_add_event_cb(game_station, game_draw_event_cb, LV_EVENT_DRAW_POST, NULL);
    // lv_obj_move_background(game_station);
    lv_obj_clear_flag(game_station, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_pad_all(game_station, 0, 0);
    lv_obj_set_style_border_color(game_station, lv_color_black(), 0);
    lv_obj_set_style_radius(game_station, 0, 0);
    ui.game_station = game_station;

    lv_obj_t *labellux = lv_label_create(game_station);
    lv_obj_set_size(labellux, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_label_set_text_fmt(labellux, "Hello", 0.25);
    lv_obj_align(labellux, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_text_color(labellux, lv_color_white(), 0);
    lv_obj_set_style_text_font(labellux, &lv_font_montserrat_16, 0);
    lv_obj_set_style_bg_opa(labellux,LV_OPA_0,0);
    lv_obj_add_flag(labellux,LV_OBJ_FLAG_HIDDEN);
    ui.labellux = labellux;

    lv_obj_t *bulb = lv_gif_create(game_station);
    // lv_gif_set_src(bulb,&img_bulb_gif);
    lv_obj_align(bulb,LV_ALIGN_TOP_MID,0,0);
    lv_obj_add_flag(bulb,LV_OBJ_FLAG_HIDDEN);
    ui.bulb = bulb;

}

void BubbleView::load()
{
    // lv_disp_load_scr(ui.root);
    lv_scr_load_anim(ui.root, LV_SCR_LOAD_ANIM_FADE_IN, 300, 0, false);
}

void BubbleView::destroy()
{
    if (ui.root != NULL)
    {
        lv_obj_del(ui.root);
        ui.root = NULL;
    }
}

#include <Arduino.h>
void BubbleView::delVectb()
{
    vectb.clear();
    vectbb.clear();
}

// #include <Arduino.h>

void BubbleView::game_draw_event_cb(lv_event_t *e)
{
    // changing the style of the rectangle.
    lv_obj_t *obj = lv_event_get_target(e);
    lv_draw_ctx_t *draw_ctx = lv_event_get_draw_ctx(e); // get the draw context
    lv_draw_rect_dsc_t draw_rect_dsc; // there is many draw dsc
    lv_draw_rect_dsc_init(&draw_rect_dsc);

    draw_rect_dsc.radius = LV_RADIUS_CIRCLE; // change the rectangle into a circle
    draw_rect_dsc.border_width = 2;
    draw_rect_dsc.bg_opa = LV_OPA_0;               // transparent circle
    draw_rect_dsc.border_color = lv_color_white(); //,LV_COLOR_MAKE(100, 150, 226);
    if (vectb.size() < 50)
    {
        bubble_t b = {
            .r = lv_rand(5, 50),
            .x = lv_rand(b.r, LV_HOR_RES - b.r),
            .y = lv_rand(b.r, LV_VER_RES - b.r),
            .dx = (((random(2) == 1) ? -1 : 1) * lv_rand(1, 2)),
            .dy = (((random(2) == 1) ? -1 : 1) * lv_rand(1, 2)),
            // .color = lv_color_white(),
        };
        vectb.push_back(b);
    }

    for (auto &b : vectb)
    {
        // draw_rect_dsc.border_color = lv_color_white();
        lv_area_t gen_rect;      // the area_t is import for the drawing.
        gen_rect.x1 = b.x;       // giving the x1,y1,x2,y2 site for every rectangle
        gen_rect.x2 = b.x + b.r; // but now it is circle here.
        gen_rect.y1 = b.y;
        gen_rect.y2 = b.y + b.r;
        lv_draw_rect(draw_ctx, &draw_rect_dsc, &gen_rect); // the main function works here
        b.move();
    }


}

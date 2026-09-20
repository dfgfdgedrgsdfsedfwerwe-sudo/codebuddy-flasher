#include "MpuView.h"

MpuView::MpuView()
{
}

MpuView::~MpuView()
{
}

void MpuView::create()
{
    /** screen */
    lv_obj_t *root = lv_obj_create(NULL);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(root, LV_HOR_RES, LV_VER_RES);
    lv_obj_center(root);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_set_style_bg_color(root, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_100, 0);
    ui.root = root;

    // lv_obj_set_flex_grow(ui.city, 1);

    static lv_style_t style;
    lv_style_init(&style);

    /*Make a gradient*/
    lv_style_set_bg_color(&style, lv_color_black());
    // lv_style_set_border_opa(&style, LV_OPA_100);
    lv_style_set_bg_opa(&style, LV_OPA_100);

    lv_obj_t *chart = lv_chart_create(root);
    // lv_obj_remove_style_all(game_station);
    lv_obj_set_size(chart, LV_PCT(100), LV_PCT(50));
    lv_obj_align(chart, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_add_style(chart, &style, 0);

    lv_obj_set_style_border_color(chart, lv_color_black(), 0);
    lv_obj_set_style_radius(chart, 0, 0);

    lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_Y, 5, 3, 5, 2, true, 10);
    lv_chart_set_point_count(chart, 100);
    lv_chart_set_div_line_count(chart, 0, 0);

    ui.chart = chart;
    // lv_obj_add_flag(chart, LV_OBJ_FLAG_HIDDEN);

    lv_chart_set_type(chart, LV_CHART_TYPE_LINE); /*Show lines and points too*/
    lv_obj_set_style_size(chart, 0, LV_PART_INDICATOR);
    /*Add two data series*/
    ser1 = lv_chart_add_series(chart, lv_color_hex(0x00FF00), LV_CHART_AXIS_PRIMARY_Y);
    ser2 = lv_chart_add_series(chart, lv_color_hex(0x00FFFF), LV_CHART_AXIS_SECONDARY_Y);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, -1024, 1024);
    lv_chart_set_range(chart, LV_CHART_AXIS_SECONDARY_Y,-1024, 1024);
    lv_obj_t *labelaccX = lv_label_create(root);
    lv_obj_set_size(labelaccX, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_label_set_text_fmt(labelaccX, "X:%.2f,Y:%.2f", 0.25, 0.25);

    lv_obj_align(labelaccX, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_text_color(labelaccX, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_text_font(labelaccX, &lv_font_montserrat_16, 0);
    ui.labelaccX = labelaccX;

    lv_obj_t *arc = lv_arc_create(root);
    lv_obj_set_size(arc, 100, 100);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    // 改变进度条颜色
    // lv_obj_set_style_bg_color(arc, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_BLUE);  // 轨道颜色为蓝色
    // lv_obj_set_style_local_bg_color(arc, LV_OBJ_PART_INDICATOR, LV_STATE_DEFAULT, LV_COLOR_GREEN);  // 进度条颜色为绿色
    // lv_obj_set_style_border_width(arc, 5, 0); // 设置进度条的宽度
    lv_obj_set_style_arc_width(arc, 10, LV_PART_MAIN|LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, lv_color_white(), LV_PART_MAIN|LV_PART_INDICATOR); // arc color ,not the value color
    lv_arc_set_rotation(arc, 270);
    lv_arc_set_bg_angles(arc, 0, 360);
    lv_arc_set_angles(arc, 0, 40); 
    lv_obj_align(arc,LV_ALIGN_TOP_MID,0,40);
    ui.arc = arc;
}

void MpuView::load()
{
    // lv_disp_load_scr(ui.root);
    lv_scr_load_anim(ui.root, LV_SCR_LOAD_ANIM_FADE_IN, 300, 0, false);
}

void MpuView::destroy()
{
}

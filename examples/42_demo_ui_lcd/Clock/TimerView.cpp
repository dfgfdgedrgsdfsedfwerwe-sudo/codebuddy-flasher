#include "TimerView.h"

// #include "font/font_alibaba.h"

// LV_FONT_DECLARE(font_Alibaba);

#define UI_BG_COLOR    lv_color_white()
#define UI_FRAME_COLOR lv_color_white()
#define UI_FONT_COLOR  lv_color_black()
#define UI_PAGE_COUNT  3
LV_FONT_DECLARE(lv_font_chinese_16)
LV_FONT_DECLARE(lv_font_chinese_24)
LV_FONT_DECLARE(lv_font_montserrat_16)
LV_IMG_DECLARE(bg_paper)
// LV_FONT_DECLARE(lv_font_montserrat_24)



static lv_point_t line_points[] = {{-240, 0}, {240, 0}};

TimerView::TimerView() {
}

TimerView::~TimerView() {
}

/// @brief
void TimerView::create() {

    // lv_obj_t *bg =  lv_img_create(NULL);
    // lv_img_set_src(bg,&bg_paper);  
    #if 1 
    ui.root = lv_obj_create(NULL);

    ui.clockDiv = lv_obj_create(ui.root);
    lv_obj_set_size(ui.clockDiv, LV_HOR_RES, LV_VER_RES/2);
    lv_obj_set_style_border_width(ui.clockDiv, 0, 0);
    lv_obj_set_style_radius(ui.clockDiv, 0, 0);
    lv_obj_set_style_bg_color(ui.clockDiv, UI_BG_COLOR, 0);
    lv_obj_clear_flag(ui.clockDiv,
                      LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(ui.clockDiv, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_pad_all(ui.clockDiv, 0, 0);

    lv_obj_t *img2 = lv_img_create(ui.clockDiv);
    lv_img_set_src(img2,
                   LV_SYMBOL_BLUETOOTH LV_SYMBOL_WIFI LV_SYMBOL_BATTERY_FULL);
    lv_obj_align(img2, LV_ALIGN_TOP_RIGHT, -5, 0);

    static lv_obj_t *label_telecom = lv_label_create(ui.clockDiv);

    lv_obj_set_size(label_telecom, 120, 30); /*Fix size*/
    lv_obj_set_style_text_font(label_telecom, &lv_font_chinese_16, 0);
    lv_obj_set_style_text_color(label_telecom,lv_color_hex(0xFF0000), 0);
    lv_label_set_long_mode(label_telecom, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(label_telecom, "新年快乐,万事如意,合家安康!");
    lv_obj_align(label_telecom, LV_ALIGN_TOP_LEFT, 0, 0);

    ui.hourDiv = lv_obj_create(ui.clockDiv);
    lv_obj_set_size(ui.hourDiv, 50, 50);
    lv_obj_align(ui.hourDiv, LV_ALIGN_LEFT_MID, LV_HOR_RES / 6, 0);
    lv_obj_set_style_bg_color(ui.hourDiv, UI_FRAME_COLOR, 0);
    lv_obj_clear_flag(ui.hourDiv, LV_OBJ_FLAG_SCROLLABLE);

    ui.minDiv = lv_obj_create(ui.clockDiv);
    lv_obj_set_size(ui.minDiv, 50, 50);
    lv_obj_align(ui.minDiv, LV_ALIGN_RIGHT_MID, -LV_HOR_RES / 6, 0);
    lv_obj_set_style_bg_color(ui.minDiv, UI_FRAME_COLOR, 0);
    lv_obj_clear_flag(ui.minDiv, LV_OBJ_FLAG_SCROLLABLE);

    ui.segLabel = lv_label_create(ui.clockDiv);
    lv_obj_align(ui.segLabel, LV_ALIGN_CENTER, 0, -6);
    lv_obj_set_style_text_font(ui.segLabel, &lv_font_montserrat_32, 0);
    lv_label_set_text(ui.segLabel, ":");
    lv_obj_set_style_text_color(ui.segLabel, UI_FONT_COLOR, 0);

    ui.hourLabel = lv_label_create(ui.hourDiv);
    lv_obj_center(ui.hourLabel);
    lv_obj_set_style_text_font(ui.hourLabel, &lv_font_montserrat_32, 0);
    lv_label_set_text(ui.hourLabel, "12");
    lv_obj_set_style_text_color(ui.hourLabel, UI_FONT_COLOR, 0);

    ui.minLabel = lv_label_create(ui.minDiv);
    lv_obj_center(ui.minLabel);
    lv_obj_set_style_text_font(ui.minLabel, &lv_font_montserrat_32, 0);
    lv_label_set_text(ui.minLabel, "34");
    lv_obj_set_style_text_color(ui.minLabel, UI_FONT_COLOR, 0);

    lv_obj_t *timelabel = lv_label_create(ui.clockDiv);
    // lv_obj_set_size(ui.city, 240, 30); /*Fix size*/
    lv_obj_align(timelabel, LV_ALIGN_BOTTOM_MID, 0, -10);

    lv_obj_set_style_border_color(timelabel, lv_color_white(), 0);
    lv_obj_set_style_border_opa(timelabel, LV_OPA_0, 0);
    lv_obj_set_style_text_color(timelabel, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_set_style_text_font(timelabel, &lv_font_chinese_24, 0);
    lv_label_set_text_fmt(timelabel, "2025年1月1日 星期三");
    ui.timelabel = timelabel;



    lv_obj_t *sensorStation = lv_obj_create(ui.root);
    lv_obj_set_size(sensorStation, LV_HOR_RES, LV_VER_RES /2);
    lv_obj_set_style_border_width(sensorStation, 0, 0);
    lv_obj_set_style_radius(sensorStation, 0, 0);
    lv_obj_set_style_bg_color(sensorStation, UI_BG_COLOR, 0);
    lv_obj_clear_flag(sensorStation,
                      LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(sensorStation, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_pad_all(sensorStation, 0, 0);
    ui.sensorDiv = sensorStation;

    // 创建一个样式对象
    static lv_style_t label_style;
    lv_style_init(&label_style);
    // 设置边框颜色
    lv_style_set_border_color(&label_style, lv_color_white());
    // 设置边框透明度
    lv_style_set_border_opa(&label_style, LV_OPA_0);
    // 设置文本颜色
    lv_style_set_text_color(&label_style, lv_palette_main(LV_PALETTE_BLUE));
    // 设置文本字体
    lv_style_set_text_font(&label_style, &lv_font_chinese_24);


    lv_obj_t *sensor_label = lv_label_create(ui.sensorDiv);

    // lv_obj_set_style_border_color(sensor_label, lv_color_white(), 0);
    // lv_obj_set_style_border_opa(sensor_label, LV_OPA_0, 0);
    // lv_obj_set_style_text_color(sensor_label, lv_palette_main(LV_PALETTE_BLUE),
    //                             0);
    // lv_obj_set_style_text_font(sensor_label, &lv_font_chinese_24, 0);
        // 将样式应用于 temperature 对象
    lv_obj_add_style(sensor_label, &label_style, 0);
    lv_obj_align(sensor_label, LV_ALIGN_TOP_MID, 0, 0);
    lv_label_set_text_fmt(sensor_label, "室内环境");

    lv_obj_t *temperature = lv_label_create(ui.sensorDiv);
    // lv_obj_set_size(sensor_label, 240, 30); /*Fix size*/      
    lv_obj_add_style(temperature, &label_style, 0);
    lv_obj_align(temperature, LV_ALIGN_LEFT_MID, 5, -20);
    lv_label_set_text_fmt(temperature, "温度");
    ui.temperature = temperature;

    lv_obj_t *humidity = lv_label_create(ui.sensorDiv);
    // lv_obj_set_size(sensor_label, 240, 30); /*Fix size*/        
    lv_obj_add_style(humidity, &label_style, 0);
    lv_obj_align(humidity, LV_ALIGN_LEFT_MID, 5,20);
    lv_label_set_text_fmt(humidity, "湿度");
    ui.humidity = humidity;

    lv_obj_t *visb_light = lv_label_create(ui.sensorDiv);
    // lv_obj_set_size(sensor_label, 240, 30); /*Fix size*/   
    lv_obj_align(visb_light, LV_ALIGN_RIGHT_MID, -5, -20);
    lv_obj_add_style(visb_light, &label_style, 0);
    lv_label_set_text_fmt(visb_light, "可见光");
    ui.visb_light = visb_light;

    lv_obj_t *IR_light = lv_label_create(ui.sensorDiv);
    // lv_obj_set_size(sensor_label, 240, 30); /*Fix size*/  
    lv_obj_align(IR_light, LV_ALIGN_RIGHT_MID, -5, 20);
    lv_obj_add_style(IR_light, &label_style, 0);
    lv_label_set_text_fmt(IR_light, "红外光");
    ui.IR_light = IR_light;

    ui.city = lv_label_create(ui.sensorDiv);
    lv_obj_set_size(ui.city, 240, 30); /*Fix size*/
    lv_obj_align(ui.city, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    lv_obj_set_style_border_color(ui.city, lv_color_white(), 0);
    lv_obj_set_style_border_opa(ui.city, LV_OPA_0, 0);
    lv_obj_set_style_text_color(ui.city, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_set_style_text_font(ui.city, &lv_font_chinese_24, 0);
    lv_label_set_text_fmt(ui.city, "佛山");
    lv_label_set_long_mode(ui.city, LV_LABEL_LONG_SCROLL_CIRCULAR);
    #endif 
}

void TimerView::load() {
    // lv_disp_load_scr(ui.root);
    lv_scr_load_anim(
        ui.root, lv_scr_load_anim_t(rand() & 15) /*LV_SCR_LOAD_ANIM_FADE_IN*/,
        300, 0, false);
}

void TimerView::destroy() {
    if (ui.root != NULL) {
        lv_obj_del(ui.root);
        ui.root = NULL;
    }
}

void TimerView::setSegLabel(bool hidden) {
    if (hidden) {
        lv_obj_add_flag(ui.segLabel, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(ui.segLabel, LV_OBJ_FLAG_HIDDEN);
    }
}

void TimerView::updateHour(int32_t hour) {
    lv_label_set_text_fmt(ui.hourLabel, "%02d", hour);
}

/**
 * @brief update the min value at the view.
 * @
 *
 * @param min
 */
void TimerView::updateMin(int32_t min) {
    lv_label_set_text_fmt(ui.minLabel, "%02d", min);
}

void TimerView::updateCity(String cityName, String weather, String temp,
                           String humd, String wind, String windD) {
    // static long last_value = 0;
    // lv_label_set_text_fmt(ui.city, "%s", cityName.c_str());
    lv_label_set_text_fmt(ui.city, "%s %s %s°C %s%% %s级  %s风",
                          cityName.c_str(), weather.c_str(), temp.c_str(),
                          humd.c_str(), windD.c_str(), wind.c_str());
    // last_value = ND_value;
}


void TimerView::updateSensor(int temp, int humi ,uint16_t visb,uint16_t IR) {
    // static long last_value = 0;
    // lv_label_set_text_fmt(ui.city, "%s", cityName.c_str());
    lv_label_set_text_fmt(ui.temperature, "温度:%d°C",temp);
    lv_label_set_text_fmt(ui.humidity, "湿度:%d%%",humi);   
    lv_label_set_text_fmt(ui.visb_light, "光强:%d",visb);                      
    lv_label_set_text_fmt(ui.IR_light, "红外:%d",IR);                      
                     
    // last_value = ND_value;
}


void TimerView::updateTime(const char *time) {
    // 确保 time 参数不为空
    if (time != nullptr) {
        lv_label_set_text_fmt(ui.timelabel, "%s", time);
    } else {
        // 处理空指针情况，例如设置一个默认值或记录错误日志
        lv_label_set_text_fmt(ui.timelabel, "Invalid Time");
    }
}
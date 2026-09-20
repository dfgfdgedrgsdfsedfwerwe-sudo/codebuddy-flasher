#ifndef MPU_VIEW_H_
#define MPU_VIEW_H_

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
#include "MpuModel.h"

class MpuView
{
public:
 
    struct
    {
        lv_obj_t *root;
        lv_obj_t *acc_data;
        lv_obj_t *labelaccX;
        lv_obj_t *labelaccY;
        lv_obj_t *labelaccZ;
        lv_obj_t *chart;
        lv_obj_t *arc;
    } ui;


    MpuView();
    ~MpuView();

    void create();
    void load();
    void destroy();   
    
    lv_chart_series_t *ser1;
    lv_chart_series_t *ser2;

private:


};

#endif
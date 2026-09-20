#include "MpuController.h"

MpuController::MpuController()
{
}

MpuController::~MpuController()
{
}

void MpuController::init()
{
   model.init();
   view.create();
}

void MpuController::onViewLoad()
{

    // view.create();
    view.load();
    updateTimer = lv_timer_create(
        [](lv_timer_t *timer)
        {
            MpuController *controller = (MpuController *)timer->user_data;
            controller->update(); // get the time
        },
        50,
        this);

    accTimer = lv_timer_create(
        [](lv_timer_t *timer)
        {
            MpuController *controller = (MpuController *)timer->user_data;
            controller->model.updatedata(); // get the time
        },
        20,
        this);
    // lv_timer_set_repeat_count(accTimer, 10);
}

void MpuController::onViewDisappear()
{
    deleteTimer();
    view.destroy();
    //model.deinit();
}

void MpuController::update()
{
    lv_chart_set_next_value(view.ui.chart, view.ser1, model.accX);
    lv_chart_set_next_value(view.ui.chart, view.ser2, model.accY);
    // lv_label_set_text_fmt(view.ui.labelaccX,"angle:%.2f,gx:%.2f", model.angle,model.gyroX);
    lv_label_set_text_fmt(view.ui.labelaccX,"<:%d", model.angle);
    lv_arc_set_angles(view.ui.arc,0,model.angle);

}

void MpuController::accUpdate()
{
    //model.getAlldata();
    // lv_label_set_text_fmt(view.ui.labelaccX, "accX:%0.1f",model.getAccX());
    // lv_label_set_text_fmt(view.ui.labelaccY, "accY:%0.1f",model.getAccY());
    // lv_label_set_text_fmt(view.ui.labelaccZ, "accZ:%0.1f",model.getAccZ());
}

void MpuController::deleteTimer()
{
    if (updateTimer != NULL)
    {
        lv_timer_del(updateTimer);
        updateTimer = NULL;
    }
    if (accTimer != NULL)
    {
        lv_timer_del(accTimer);
        accTimer = NULL;
    }
}


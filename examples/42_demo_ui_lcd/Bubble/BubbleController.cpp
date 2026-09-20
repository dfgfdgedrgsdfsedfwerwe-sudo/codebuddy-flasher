#include "BubbleController.h"

BubbleController::BubbleController()
{
}

BubbleController::~BubbleController()
{
}

void BubbleController::init()
{
    model.init();
    view.create();
}

void BubbleController::onViewLoad()
{

    // view.create();
    view.load();
    updateTimer = lv_timer_create(
        [](lv_timer_t *timer)
        {
            BubbleController *controller = (BubbleController *)timer->user_data;
            controller->update(); // get the time
        },
        100,
        this);

    luxTimer = lv_timer_create(
        [](lv_timer_t *timer)
        {
            BubbleController *controller = (BubbleController *)timer->user_data;
            // controller->luxUpdate(); // get the time
        },
        200,
        this);
}

void BubbleController::onViewDisappear()
{
    deleteTimer();
    view.delVectb();
    // model.speaker.mute();
    // view.destroy();
    // model.deinit();
}

void BubbleController::update()
{
    lv_obj_invalidate(view.ui.game_station);
}

void BubbleController::luxUpdate()
{
    model.getAlldata();
    int lux = model.getLux();
    lv_label_set_text_fmt(view.ui.labellux, "lux:%d", lux);
    lv_obj_add_flag(view.ui.labellux, LV_OBJ_FLAG_FLOATING);
    bool hasFlag;
    if (lux < 400)
    {
        lv_timer_pause(updateTimer);
        // model.speaker.beep();
        hasFlag = lv_obj_has_flag(view.ui.bulb, LV_OBJ_FLAG_HIDDEN);
        if (hasFlag)
        {
            lv_obj_clear_flag(view.ui.bulb, LV_OBJ_FLAG_HIDDEN);
        }
    }
    else
    {
        lv_timer_resume(updateTimer);
        // model.speaker.mute();
        hasFlag = lv_obj_has_flag(view.ui.bulb, LV_OBJ_FLAG_HIDDEN);
        if (!hasFlag)
        {
            lv_obj_add_flag(view.ui.bulb, LV_OBJ_FLAG_HIDDEN);
        }
    }

}

void BubbleController::deleteTimer()
{
    if (updateTimer != NULL)
    {
        lv_timer_del(updateTimer);
        updateTimer = NULL;
    }
    if (luxTimer != NULL)
    {
        lv_timer_del(luxTimer);
        luxTimer = NULL;
    }
}
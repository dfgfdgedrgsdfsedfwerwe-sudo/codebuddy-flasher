#include "TimerController.h"


const char *week_char[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};
// const char *week_char[] = LV_CALENDAR_DEFAULT_DAY_NAMES;
const char *month_char[] = LV_CALENDAR_DEFAULT_MONTH_NAMES;

TimerController::TimerController() {
}


TimerController::~TimerController() {
}


void TimerController::init() {
    model.init();
    view.create();
}


void TimerController::onViewLoad() {
    // model.init();
    // view.create();
    view.load(); 
    view.updateCity(model.getCity(),model.getWeatherState(),model.getTemperature(),model.getHumidity(),model.getwinddirection(),model.getwindpower());
                  // setup the lvgl display enviroment. 
    updateTimer = lv_timer_create(   // start the timers. 
        [](lv_timer_t* timer) {
            TimerController* controller = (TimerController *)timer->user_data;
            controller->update();    // get the time 
        },
        1000,
        this
    );

    tickTimer = lv_timer_create(
        [](lv_timer_t* timer) {
            TimerController* controller = (TimerController *)timer->user_data;
            controller->updateSegLabel();  // show the second flashing effect. 
        },
        1000,
        this
    );

    sensorTimer = lv_timer_create(
        [](lv_timer_t* timer) {
            TimerController* controller = (TimerController *)timer->user_data;
            controller->updateSensor();  // show the second flashing effect. 
        },
        50,
        this
    );

    weatherTimer = lv_timer_create(
        [](lv_timer_t* timer) {
            TimerController* controller = (TimerController *)timer->user_data;
            controller->updateWeather();           
        },
        1000 * 60 *5, // 更新显示数据的时间间隔。 
        this
    );

}


void TimerController::onViewDisappear() {
    deleteTimer();
    // view.destroy();  // ?why  ,increase the speed ?
    model.deinit();
}


void TimerController::update() {
    model.getTime();
    view.updateHour(model.getHour());
    view.updateMin(model.getMin());
    char time[50];
    sprintf(time, "%d年%d月%d日%S", model.year, model.month, model.day, week_char[model.wday]);
    view.updateTime(time);
}


void TimerController::updateSegLabel() {
    static bool status = false;
    view.setSegLabel(status);
    status = !status;
}


void TimerController::updateWeather() {
    model.getWeather();
    view.updateCity(model.getCity(),model.getWeatherState(),model.getTemperature(),model.getHumidity(),model.getwinddirection(),model.getwindpower());

}


void TimerController::updateSensor() {
    view.updateSensor(model.getSensorTemp(),model.getSensorHumi(),model.getVisbLight(),model.getIRlight());    
}


void TimerController::deleteTimer() {
    if (updateTimer != NULL) {
        lv_timer_del(updateTimer);
        updateTimer = NULL;
    }

    if (tickTimer != NULL) {
        lv_timer_del(tickTimer);
        tickTimer = NULL;
    }
}
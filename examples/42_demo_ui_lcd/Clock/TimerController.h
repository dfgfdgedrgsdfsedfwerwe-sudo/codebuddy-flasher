#ifndef TIMER_CONTROLLER_H_
#define TIMER_CONTROLLER_H_

#include "TimerModel.h"
#include "TimerView.h"

class TimerController {
public:
    TimerController(/* args */);
    ~TimerController();

    void init();
    void onViewLoad();
    void onViewDisappear();

private:
    TimerModel model;  // class 
    TimerView view;
    lv_timer_t *updateTimer;  // variables 
    lv_timer_t *weatherTimer;  // variables 
    lv_timer_t *tickTimer;
    lv_timer_t *sensorTimer;


private:
    void update();
    void updateWeather();
    void updateSegLabel();
    void deleteTimer();
    void updateSensor();
};

#endif
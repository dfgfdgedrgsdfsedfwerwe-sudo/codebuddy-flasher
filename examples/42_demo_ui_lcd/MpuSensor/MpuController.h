#ifndef MPU_CONTROLLER_H_
#define MPU_CONTROLLER_H_

#include "MpuModel.h"
#include "MpuView.h"

class MpuController
{
public:
    MpuView view;
    MpuModel model;
    MpuController();
    ~MpuController();

    void init();
    void onViewLoad();
    void onViewDisappear();

private: 

    // BubbleModel model;  // class    
    lv_timer_t *updateTimer;
    lv_timer_t *accTimer;
    

private:
    void update();
    void deleteTimer();
    void accUpdate();

};




#endif
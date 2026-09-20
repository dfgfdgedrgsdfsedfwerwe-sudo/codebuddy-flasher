#ifndef BUBBLE_CONTROLLER_H_
#define BUBBLE_CONTROLLER_H_

#include "BubbleModel.h"
#include "BubbleView.h"

class BubbleController
{
public:
    BubbleController();
    ~BubbleController();

    void init();
    void onViewLoad();
    void onViewDisappear();

private:
  
    BubbleView view;
    BubbleModel model;  // class    
    lv_timer_t *updateTimer;
    lv_timer_t *luxTimer;
    

private:
    void update();
    void luxUpdate();
    void deleteTimer();    

};




#endif
#ifndef BUBBLE_MODEL_H_
#define BUBBLE_MODEL_H_

#include <stdint.h>
#include <time.h>
// #include <M5_DLight.h>
// #include <Speaker.h>
class BubbleModel
{

public:
   BubbleModel(/* args */);
   ~BubbleModel();

   void init();
   void deinit(){};
   // M5_DLight sensor;
   // SPEAKER speaker;   
   void getAlldata();

   uint16_t getLux();

private:
   uint16_t lux;
};

#endif



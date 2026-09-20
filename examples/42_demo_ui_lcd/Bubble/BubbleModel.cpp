
#include <Wire.h>
#include "BubbleModel.h"

BubbleModel::BubbleModel()
{
}

BubbleModel::~BubbleModel()
{
}

void BubbleModel::init()
{
    // sensor.begin();
    // sensor.begin(&Wire, 6, 7);
    // CONTINUOUSLY_H_RESOLUTION_MODE
    // CONTINUOUSLY_H_RESOLUTION_MODE2
    // CONTINUOUSLY_L_RESOLUTION_MODE
    // ONE_TIME_H_RESOLUTION_MODE
    // ONE_TIME_H_RESOLUTION_MODE2
    // ONE_TIME_L_RESOLUTION_MODE
    // sensor.setMode(CONTINUOUSLY_H_RESOLUTION_MODE);
    // speaker.begin(); // confrim the buzzer is working.
}

void BubbleModel::getAlldata()
{
    // this->lux = sensor.getLUX();
}

uint16_t BubbleModel::getLux()
{
    return this->lux;
}

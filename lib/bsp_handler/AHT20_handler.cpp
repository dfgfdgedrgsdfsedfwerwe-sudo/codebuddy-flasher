
#include "AHT20_handler.h"

AHT20::AHT20(TwoWire &wire) : DFRobot_AHT20(wire)
{
    xTaskCreatePinnedToCore(AHT20::taskLoop, "aht20", 4 * 1024, this, 2, NULL, 1);
}

float AHT20::getData(eAHT20Data_t type)
{
    switch (type)
    {
    case eAHT20TempC:
        return getTemperature_C();
    case eAHT20TempF:
        return getTemperature_F();
    case eAHT20HumiRH:
        return getHumidity_RH();
    default:
        break;
    }
    return -1;
}

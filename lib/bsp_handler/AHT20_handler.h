#ifndef  AHT20_HANDLER_
#define AHT20_HANDLER_



#include "../DFRobot_AHT20/DFRobot_AHT20.h"

class AHT20 : public DFRobot_AHT20
{
public:
    enum eAHT20Data_t
    {
        eAHT20TempC,
        eAHT20TempF,
        eAHT20HumiRH,
    };
    /**
     * @fn AHT20
     * @brief AHT20 constructor
     * @param wire Communication properties
     */
    AHT20(TwoWire &wire = Wire);

    /**
     * @fn getData
     * @brief Get AHT20 data
     * @param type Data selection
     */
    float getData(eAHT20Data_t type);

private:
    static void taskLoop(void *param)
    {
        AHT20 *self = (AHT20 *)param;
        uint8_t status;

        while((status = self->begin()) != 0){
            printf("AHT20 sensor initialization failed.\n");
            delay(1000);
            break;
        }
        TickType_t xLastWakeTime;
        const TickType_t xFrequency = pdMS_TO_TICKS(1000); // Specify the task to run every 1000ms
        while (1)
        {
            if(self->startMeasurementReady(true)){
            }
           vTaskDelayUntil(&xLastWakeTime, xFrequency);
        }
    }

};
#endif // ! AHT20_HANDLER_
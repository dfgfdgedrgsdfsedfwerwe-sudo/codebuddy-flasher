/**
 * @file aht20.ino
 * @brief The running example will obtain temperature and humidity
 *
 * @copyright   Copyright (c) 2010 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @license     The MIT License (MIT)
 * @author tangjie(jie.tang@dfrobot.com)
 * @version V1.0
 * @date 2024-6-26
 */

// #include "unihiker_k10.h"
// #include "initBoard.h"
#include "AHT20_handler.h"
// UNIHIKER_K10 k10;
AHT20 aht20;
void setup() {
    Serial.begin(115200);
    pinMode(0, OUTPUT);  // suppress the noise
    digitalWrite(0, LOW);
    // k10.begin();
}

void loop() {
    Serial.print("temperature(-40~85 C): ");
    // Get temp in Celsius (℃), range -40-80℃
    Serial.print(aht20.getData(aht20.eAHT20TempC));
    Serial.print(" ℃, ");
    // Get temp in Fahrenheit (F)
    Serial.print(aht20.getData(aht20.eAHT20TempF));
    Serial.print(" F\t");
    Serial.print("humidity(0~100): ");
    // Get relative humidity (%RH), range 0-100℃
    Serial.print(aht20.getData(aht20.eAHT20HumiRH));
    Serial.println(" %RH");
    delay(1000);
}

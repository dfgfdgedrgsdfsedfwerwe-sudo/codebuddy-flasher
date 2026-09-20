#ifndef TIMER_MODEL_H_
#define TIMER_MODEL_H_

#include <stdint.h>
#include <time.h>

#include <stdint.h>
#include <WString.h>
#include "lvgl.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
// #include "config.h"

class TimerModel {
   public:
    TimerModel(/* args */);
    ~TimerModel();

    void init();
    void deinit() {};
    static void printLocalTime();
    static void timeavailable(struct timeval *t);
    int32_t getHour();
    int32_t getMin();
    void getTime();

    int getSensorTemp();
    int getSensorHumi();
    String getTemperature();
    String getHumidity();
    String getWeatherState();
    String getCity();
    String getwindpower();
    String getwinddirection();
    void getWeather();

    uint16_t getVisbLight();
    uint16_t getIRlight();

    static void updateLightSensor(void *parameter);
    int sensorTemperature;
    int sensorHumidity;
    static uint16_t visb_Light;
    static uint16_t IR_light;


    int min;
    int hour;
    int year;
    int month;
    int day;
    int wday;


   private:
  
    JsonObject live;
    // String temperature;
    // String humidity;
    String cityCode = "440600&";  // 查看你的 城市代码
    String userKey =
        "831f437cxxxxxxxxxxxxxxxXX";  // 要去注册一个高德地图的开发用户
    lv_timer_t *weatherTimer;

    String city;
    String weather;
    String winddirection;
    String windpower;
    String temperature;
    String humidity;
};

#endif
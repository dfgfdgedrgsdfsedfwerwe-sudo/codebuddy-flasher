
#include "TimerModel.h"
#include <sntp.h>
#include <time.h>
#include <Arduino.h>
#include <WiFi.h>
#include "AHT20_handler.h"
// UNIHIKER_K10 k10;
AHT20 aht20;

#include "Adafruit_LTR329_LTR303.h"
Adafruit_LTR303 ltr        = Adafruit_LTR303();

uint16_t TimerModel::visb_Light = 0;  // 初始化为0
uint16_t TimerModel::IR_light = 0;    // 初始化为0

#define NTP_SERVER1          "pool.ntp.org"
#define NTP_SERVER2          "time.nist.gov"
#define GMT_OFFSET_SEC       (3600 * 8)
#define DAY_LIGHT_OFFSET_SEC 0



static const char *ssid     = " ";
static const char *password = "  ";



TimerModel::TimerModel() {
}

TimerModel::~TimerModel() {
}

void TimerModel::init() {
    // WiFi.mode(WIFI_STA);
    // // WiFi.disconnect();
    // //  WiFi.begin(ssid, password);
    // if (WiFi.status() != WL_CONNECTED)
    // {
    //     WiFi.begin(ssid, password);
    //     while (WiFi.status() != WL_CONNECTED)
    //     {
    //         delay(500);
    //         Serial.println("Connecting to WiFi...");
    //     }
    // }
    if (!ltr.begin()) {
        Serial.println("Couldn't find LTR sensor!");
        while (1) delay(10);
    }

    ltr.setGain(LTR3XX_GAIN_4);
    // Set integration time of 50ms (see advanced demo for all options!
    ltr.setIntegrationTime(LTR3XX_INTEGTIME_50);
    // Set measurement rate of 50ms (see advanced demo for all options!
    ltr.setMeasurementRate(LTR3XX_MEASRATE_50);

    xTaskCreatePinnedToCore(updateLightSensor, "updateLightSensor", 4 * 1024,
                            this, 2, NULL, 1);

    getWeather();

    sntp_servermode_dhcp(1);  // (optional)
    configTime(GMT_OFFSET_SEC, DAY_LIGHT_OFFSET_SEC, NTP_SERVER1, NTP_SERVER2);
    printLocalTime();
}

int32_t TimerModel::getHour() {
    // struct tm timeinfo;
    // getLocalTime(&timeinfo, 100); // 100 -> timeout
    // return timeinfo.tm_hour;
    return hour;
}

int32_t TimerModel::getMin() {
    // struct tm timeinfo;
    // getLocalTime(&timeinfo, 100);
    // return timeinfo.tm_min;
    return min;
}

void TimerModel::getTime() {
    struct tm timeinfo;
    getLocalTime(&timeinfo, 100);  // 100 -> timeout
    hour  = timeinfo.tm_hour;
    min   = timeinfo.tm_min;
    year  = timeinfo.tm_year + 1900; ;
    month = timeinfo.tm_mon + 1;
    day = timeinfo.tm_mday;
    wday = timeinfo.tm_wday;
}

void TimerModel::printLocalTime() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("No time available (yet)");
        return;
    }
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}
// Callback function (get's called when time adjusts via NTP)
void TimerModel::timeavailable(struct timeval *t) {
    Serial.println("Got time adjustment from NTP!");
    printLocalTime();
    // WiFi.disconnect();
}

void TimerModel::getWeather() {
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.begin(ssid, password);
        while (WiFi.status() != WL_CONNECTED) {
            delay(500);
            Serial.println("Connecting to WiFi...");
        }
    }
    // 定义一个全局变量作为连接成功的标志
    bool connection_success = false;


    Serial.println("Connected to WiFi");
    HTTPClient http;
    http.begin("https://restapi.amap.com/v3/weather/weatherInfo?city=" +
               cityCode + "key=" + userKey);
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        Serial.println(payload);

        DynamicJsonDocument doc(1024);
        deserializeJson(doc, payload);
        this->live          = doc["lives"][0];
        this->temperature   = live["temperature_float"].as<String>();
        this->humidity      = live["humidity"].as<String>();
        this->city          = live["city"].as<String>();
        this->weather       = live["weather"].as<String>();
        this->winddirection = live["winddirection"].as<String>();
        this->windpower     = live["windpower"].as<String>();

    } else {
        Serial.printf("HTTP request failed with error %d\n", httpCode);
    }
    http.end();
    // WiFi.disconnect();
}

String TimerModel::getTemperature() {
    // temperature = aht20.getData(aht20.eAHT20TempC);
    return temperature;
}

String TimerModel::getHumidity() {
    // this->humidity = aht20.getData(aht20.eAHT20HumiRH);
    return this->humidity;
}
int TimerModel::getSensorTemp() {
    sensorTemperature = aht20.getData(aht20.eAHT20TempC);
    return sensorTemperature;
}

int TimerModel::getSensorHumi() {
    this->sensorHumidity = aht20.getData(aht20.eAHT20HumiRH);
    return this->sensorHumidity;
}

void TimerModel::updateLightSensor(void *parameter) {
    bool valid;

    static uint16_t visible_plus_ir, infrared;

    TickType_t xLastWakeTime;
    const TickType_t xFrequency =
        pdMS_TO_TICKS(50);  // Specify the task to run every 1000ms
    while (1) {
        if (ltr.newDataAvailable()) {
            valid = ltr.readBothChannels(visible_plus_ir, infrared);
            if (valid) {
                visb_Light = visible_plus_ir;
                IR_light   = infrared;
            }
        }
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

uint16_t TimerModel::getVisbLight() {
    return this->visb_Light;
}

uint16_t TimerModel::getIRlight() {
    return this->IR_light;
}

//___________________________________________________________??

String TimerModel::getCity() {
    return this->city;
}

String TimerModel::getWeatherState() {
    return this->weather;
}
String TimerModel::getwinddirection() {
    return this->winddirection;
}
String TimerModel::getwindpower() {
    return this->windpower;
}

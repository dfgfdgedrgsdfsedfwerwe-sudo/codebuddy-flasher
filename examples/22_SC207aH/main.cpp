// #include <Wire.h>      // 导入 Wire 库，支持 I2C 通信
#include <SC7A20H.h>  // 导入 SC7A20H 库
#include "initBoard.h"
#include "Lcd_handler.hpp"

// 创建 SC7A20H 对象，传入 Wire 对象
SC7A20H sensor;
static K10_Lcd tft;

void setup() {
    // 初始化串口通讯
    Serial.begin(115200);
    pinMode(0, OUTPUT);  // suppress the noise
    digitalWrite(0, LOW);

    // 初始化开发板（背光、扩展芯片等）
    init_board();

    // 初始化 LCD
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    // 显示标题
    tft.setCursor(10, 10);
    tft.println("SC7A20H Sensor");
    tft.setCursor(10, 35);
    tft.println("Accelerometer");

    // 初始化 I2C 通信
    Wire.begin(47, 48);

    Wire.beginTransmission(sensor._sc7a20hAddr);
    uint8_t ret = Wire.endTransmission();
    if (ret != 0) {
        Serial.println("SC7A20H Device not found, please check if the device is connected.");
        tft.setCursor(10, 70);
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.println("Sensor Error!");
        while(1) delay(1000);
    }
    // 初始化 SC7A20H 传感器
    sensor.initSC7A20H();

    // 输出初始化成功消息
    Serial.println("SC7A20H Sensor Initialized");
    tft.setCursor(10, 70);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.println("Init OK");
    delay(1000);

    // 清空准备显示数据
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(10, 10);
    tft.println("Accelerometer");
    tft.drawLine(0, 35, 240, 35, TFT_CYAN);
}

void loop() {
    // 获取加速度计的 X, Y, Z 轴数据
    sensor.updateAccelerometerData();
    int x = sensor.getAccelerometerX();
    int y = sensor.getAccelerometerY();
    int z = sensor.getAccelerometerZ();

    // 打印 X, Y, Z 轴数据到串口
    Serial.print("X: ");
    Serial.print(x);
    Serial.print("\tY: ");
    Serial.print(y);
    Serial.print("\tZ: ");
    Serial.println(z);

    // 计算并打印三轴加速度的强度
    int strength = sensor.getStrength();
    Serial.print("Strength: ");
    Serial.println(strength);

    // 显示到 LCD 屏幕
    tft.setCursor(10, 50);
    tft.setTextSize(3);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.printf("X: %5d   ", x);

    tft.setCursor(10, 85);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.printf("Y: %5d   ", y);

    tft.setCursor(10, 120);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.printf("Z: %5d   ", z);

    tft.setCursor(10, 170);
    tft.setTextSize(2);
    tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
    tft.printf("Strength: %5d   ", strength);

    // 显示可视化指示条（基于强度）
    int barWidth = map(strength, 0, 2000, 0, 220);
    tft.fillRect(10, 210, 220, 20, TFT_BLACK);
    tft.fillRect(10, 210, barWidth, 20, TFT_GREEN);
    tft.drawRect(10, 210, 220, 20, TFT_WHITE);

    // 延迟 100ms
    delay(100);
}

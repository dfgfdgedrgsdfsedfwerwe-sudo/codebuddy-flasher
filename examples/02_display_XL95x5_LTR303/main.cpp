/*
*******************************************************************************
* Copyright (c) 2022 by M5Stack
*                  Equipped with M5Core sample source code
*                          配套  M5Core 示例源代码
* Visit for more information: https://docs.m5stack.com/en/unit/dlight
* 获取更多资料请访问: https://docs.m5stack.com/zh_CN/unit/dlight
*
* Describe: Ambient light detection BH1750FVI
* Date: 2023/3/24
*******************************************************************************
  Please connect the sensor to port A (22, 21), the Lux value will be displayed
  on the display after successful initialization
  请将传感器连接至端口A(22、21),成功初始化后显示屏上将显示Lux数值
  hp_BH1750.h Please download the file from the library management office
  hp_BH1750.h 文件请从库管理处下载
*/
#include "Adafruit_LTR329_LTR303.h"
#include "XL95x5_Driver.h"

Adafruit_LTR303 ltr = Adafruit_LTR303();

#include "Lcd_handler.hpp"
K10_Lcd display;
LGFX_Sprite canvas(&display);
XL95x5 ExpandPin(XL95x5_IIC_ADDRESS, XL95x5_SDA, XL95x5_SCL);

char info[20];
void setup() {
    Serial.begin(115200);
    pinMode(0, OUTPUT); // suppress the noise
    digitalWrite(0, LOW);
    while (!Serial) {
        delay(10);  // wait for serial port to open
    }
    Serial.println("Adafruit LTR-303 simple test");
    Wire.begin(47, 48);
    
    ExpandPin.begin();
    ExpandPin.read_all_reg();  // Read all registers
    ExpandPin.pinMode(0,OUTPUT);
    ExpandPin.pinMode(1,OUTPUT);
    delay(10);  
    ExpandPin.digitalWrite(0, HIGH);
    ExpandPin.digitalWrite(1, HIGH);
    ExpandPin.pinMode(2,INPUT);
    ExpandPin.pinMode(12,INPUT);

    if (!ltr.begin()) {
        Serial.println("Couldn't find LTR sensor!");
        while (1) delay(10);
    }
    Serial.println("Found LTR sensor!");

    // Set gain of 1 (see advanced demo for all options!
    ltr.setGain(LTR3XX_GAIN_4);
    // Set integration time of 50ms (see advanced demo for all options!
    ltr.setIntegrationTime(LTR3XX_INTEGTIME_50);
    // Set measurement rate of 50ms (see advanced demo for all options!
    ltr.setMeasurementRate(LTR3XX_MEASRATE_50);
    display.init();
    display.initDMA();
    display.setRotation(1);

    display.setFont(&fonts::Orbitron_Light_24);
    display.setTextSize(0);
    display.setTextColor(TFT_ORANGE);
    display.setTextDatum(MC_DATUM);
    display.fillScreen(TFT_RED);
    canvas.setColorDepth(0);
    // canvas.setFont(&fonts::Orbitron_Light_24);
    canvas.setTextSize(3);
    canvas.setTextWrap(false);
    canvas.createSprite(300, 200);
    canvas.setTextDatum(MC_DATUM);
    // canvas.setPaletteColor(1, TFT_ORANGE);
    Serial.println("Sensor begin.....");
}

void loop() {
    bool valid;
    static uint16_t visible_plus_ir, infrared;
    static uint16_t ls_vs = 0, ls_in = 0;
    if (ltr.newDataAvailable()) {
        valid = ltr.readBothChannels(visible_plus_ir, infrared);
        if (valid) {
        }
    }

    // sprintf(info, "lux:%4d", visible_plus_ir);
#if 1
       canvas.fillSprite(TFT_BLACK);
    // canvas.drawString("Hello World", 100, 50);
    canvas.setCursor(30,50);
    canvas.print("info");

    sprintf(info, "IR: %d", visible_plus_ir);

    canvas.drawString(info, 0, 120);
    canvas.pushSprite(5, 5);
#endif
}

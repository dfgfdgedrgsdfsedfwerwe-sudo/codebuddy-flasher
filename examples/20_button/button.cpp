/**
 * @file button.ino
 * @brief The running example implements button detection
 *
 * @copyright   Copyright (c) 2010 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @license     The MIT License (MIT)
 * @author tangjie(jie.tang@dfrobot.com)
 * @version V1.0
 * @date 2024-6-21
 */

// #include "unihiker_k10.h"
// // #include "initBoard.h"

// UNIHIKER_K10 k10;
#include "Button_handler.h"

Button *buttonA = new Button(eP5_KeyA);

Button *buttonB = new Button(eP11_KeyB);

Button *buttonAB = new Button(eP5_KeyA, eP11_KeyB);

void KEYB(void)  // Callback function for button B
{
    Serial.print("KEY B\n");
}

void KEYA(void)  // Callback function for button A
{
    Serial.print("KEY A\n");
}

void setup() {
    Serial.begin(115200);
    pinMode(0, OUTPUT);  // suppress the noise
    digitalWrite(0, LOW);
    init_board();
    Serial.println("Test Start");

    buttonA->setPressedCallback(&KEYA);
    buttonB->setPressedCallback(&KEYB);
}

void loop() {
    delay(100);
    // Serial.println("Program running");
}
#include <Arduino.h>
#include "main.h"
#include "Button_handler.h"
SemaphoreHandle_t xGuiSemaphore;
LV_IMG_DECLARE(pandas);

Button *buttonA = new Button(eP5_KeyA);

Button *buttonB = new Button(eP11_KeyB);

Button *buttonAB = new Button(eP5_KeyA, eP11_KeyB);

void KEYB(void)  // Callback function for button B
{

}

void KEYA(void)  // Callback function for button A
{
     changeScreen();
}

void setup() {
    /* Initialize the Serial */
    Serial.begin(115200);
    pinMode(0, OUTPUT); // suppress the noise
    digitalWrite(0, LOW);
    lvgl_begin();
    delay(100);
    buttonA->setPressedCallback(&KEYA);
    // buttonB->setPressedCallback(&KEYB);
    xGuiSemaphore = xSemaphoreCreateMutex();
    xSemaphoreGive(xGuiSemaphore);
    example_lvgl_demo_ui(changeScreen);

    timerController.init();
    bubbleController.init();
    mpuController.init();
    xTaskCreatePinnedToCore(btnTask, "buttonTask", 4096, NULL, 6, NULL, 1);
    Serial.println("Setup done");
}

void loop() {

    /* Delay 1 tick (assumes FreeRTOS tick is 10ms) */
    vTaskDelay(pdMS_TO_TICKS(5));
    /* Try to take the semaphore, call lvgl related function on success */
    if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) {
        lv_task_handler();
        xSemaphoreGive(xGuiSemaphore);
    }
}

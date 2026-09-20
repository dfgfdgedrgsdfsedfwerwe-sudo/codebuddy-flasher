#pragma once
#include <Arduino.h>  // 对于 ESP32 标准的数字读写函数
// #include "Audio.h"
#include "../XL95x5/XL95x5_Driver.h"

#define INTR_POSEDGE          0 //上升沿
#define INTR_NEGEDGE          1 //下降沿
#define INTR_ANYEDGE          2//双边沿
#define OPENDRAIN             0
#define PUSHPULL              1

// #define I2S_DOUT 45
// #define I2S_BCLK 0
// #define I2S_LRC  38

// extern Audio audio;

#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief 引脚选择
 */
typedef enum{
    eLCD_BLK,
    eCamera_rst,
    eP11_KeyB,
    eP12,
    eP13,
    eP14,
    eP15,
    eP16,
    eP8 =10,
    eP9,
    eP10,
    eP6,
    eP5_KeyA,
    eP4,
    eP3,
    eAmp_Gain
}ePin_t;


typedef void (*interruptFunc)();//中断处理函数

typedef struct Interrupt{
    uint8_t pin;         // 节点存储的数据
    uint8_t modelInter;      //中断模式
    interruptFunc func;    // 存储函数指针
    struct Interrupt* next;   // 指向下一个节点的指针
} sInterrupt_t;


/**
 * @brief 初始化主板电源芯片
 * 
 * @return 返回初始化状态 
 */
esp_err_t init_board(void);
/**
 * @brief 设置引脚输出电平
 * 
 * @param pin 引脚选择
 * @param state 输出电平
 */
void digital_write(ePin_t pin,uint8_t state);
/**
 * @brief 获取引脚电平
 * 
 * @param pin 引脚选择
 * @return 返回引脚电平
 */
uint8_t digital_read(ePin_t pin);
/**
 * @brief 配置引脚中断
 * 
 * @param pin 引脚号
 * @param function 中断处理函数
 */
//void digitalInterrupt(ePin_t pin,interruptFunc function, uint8_t state);
/**
 * @brief 配置po口输出模式
 * 
 * @param model 模式
 * @return esp_err_t 返回配置状态 
 */
//esp_err_t poart0_model(uint8_t model);

#ifdef __cplusplus
}
#endif


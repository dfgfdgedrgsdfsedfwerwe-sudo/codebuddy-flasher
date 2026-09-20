#include "initBoard.h"
/**
 * @brief 初始化主板电源芯片
 * 
 * @return 返回初始化状态 
 */
// XL95x5 ExpandPin(XL95x5_IIC_ADDRESS, Wire);
// XL95x5 ExpandPin(XL95x5_IIC_ADDRESS, Wire);

// Audio audio;
XL95x5 ExpandPin(XL95x5_IIC_ADDRESS, XL95x5_SDA, XL95x5_SCL);
esp_err_t init_board(void){
    
    ExpandPin.begin();
    ExpandPin.read_all_reg();  // Read all registers

    ExpandPin.pinMode(eLCD_BLK,OUTPUT);
    ExpandPin.pinMode(eCamera_rst,OUTPUT);
    ExpandPin.pinMode(eAmp_Gain,OUTPUT);

    ExpandPin.digitalWrite(eLCD_BLK, LOW);
    // ExpandPin.digitalWrite(eCamera_rst, LOW);
    ExpandPin.digitalWrite(eAmp_Gain,LOW);

    delay(10);
  
    ExpandPin.digitalWrite(eLCD_BLK, HIGH);
    // ExpandPin.digitalWrite(eCamera_rst, HIGH);
    ExpandPin.digitalWrite(eAmp_Gain, HIGH);

    ExpandPin.read_all_reg();
    ExpandPin.digitalWrite(eCamera_rst, HIGH);

    ExpandPin.pinMode(eP5_KeyA,INPUT);
    ExpandPin.pinMode(eP11_KeyB,INPUT);
    ExpandPin.pinMode(eP8,INPUT);
    ExpandPin.pinMode(eP9,INPUT);
    ExpandPin.pinMode(eP13,INPUT);
    ExpandPin.pinMode(eP14,INPUT);
    ExpandPin.pinMode(eP15,INPUT);
    // audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    // audio.setVolume(0);  // 0...21
    return ESP_OK;
};
/**
 * @brief 设置引脚输出电平
 * 
 * @param pin 引脚选择
 * @param state 输出电平
 */
void digital_write(ePin_t pin,uint8_t state){
    ExpandPin.digitalWrite(pin,state);
};
/**
 * @brief 获取引脚电平
 * 
 * @param pin 引脚选择
 * @return 返回引脚电平
 */
uint8_t digital_read(ePin_t pin){
    return ExpandPin.digitalRead(pin);
};
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



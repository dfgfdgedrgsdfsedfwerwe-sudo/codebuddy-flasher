#ifndef SC7A20H_H
#define SC7A20H_H
#include "Arduino.h"
#include "Wire.h"
// #include <stdint.h>
// #include <math.h>

class SC7A20H {
private:
    
    int accX, accY, accZ; // 加速度计的三个轴数据
    TwoWire *_pWire;
    // 私有函数，处理数据读写
    uint8_t  readData(uint8_t addr, uint8_t arg, void *pBuf, size_t size);
    void writeCommand(uint8_t addr, uint8_t reg, uint8_t data);

public:
    SC7A20H(TwoWire &wire= Wire); 

    // 析构函数（如果有资源需要释放，暂时不做处理）
    ~SC7A20H(void);
    uint8_t _sc7a20hAddr = 0x19; // 假设这是传感器的地址
    void initSC7A20H(void);
      // 私有函数，用于更新加速度数据
    void updateAccelerometerData(void);
    int getAccelerometerX(void); // 获取 X 轴加速度
    int getAccelerometerY(void); // 获取 Y 轴加速度
    int getAccelerometerZ(void); // 获取 Z 轴加速度
    int getStrength(void); // 计算三轴加速度的强度
};

#endif // SC7A20H_H

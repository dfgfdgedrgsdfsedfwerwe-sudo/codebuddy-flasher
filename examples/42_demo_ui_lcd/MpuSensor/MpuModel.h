#ifndef MPU_MODEL_H_
#define MPU_MODEL_H_

#include <stdint.h>
#include <time.h>

#include <Wire.h>
// #include <SPI.h>
#include <SPI.h>
#include "MpuView.h"

// #include "MPU6886.h"
// #include <Adafruit_MSA301.h>
// #include <Adafruit_Sensor.h>
#include "SC7A20H.h"

class MpuModel
{

public:
   MpuModel(/* args */);
   ~MpuModel();
   // I2C_MPU6886 imu;
   // Adafruit_MSA301 msa;
   void init();
   void deinit(){};

   void updatedata();

   float accX = 0.0F; // Define variables for storing inertial sensor data
   float accY = 0.0F; // 定义存储惯性传感器相关数据的相关变量
   float accZ = 0.0F;

   float gyroX = 0.0F;
   float gyroY = 0.0F;
   float gyroZ = 0.0F;

   float pitch = 0.0F;
   float roll = 0.0F;
   float yaw = 0.0F;

   float temp = 0.0F;
   int angle = 0;

private:
   // File Counter

};

#endif

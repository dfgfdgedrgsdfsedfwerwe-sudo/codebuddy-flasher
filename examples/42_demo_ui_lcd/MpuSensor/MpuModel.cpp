#include "MpuModel.h"

SC7A20H sensor;
// I2C_MPU6886 imu(I2C_MPU6886_DEFAULT_ADDRESS, Wire);
MpuModel::MpuModel()
{
}

MpuModel::~MpuModel()
{
}

void MpuModel::init()
{
    Wire.begin(47, 48);
    sensor.initSC7A20H();

}

void MpuModel::updatedata()
{
    // imu.getAccel(&accX, &accY, &accZ);
    // imu.getGyro(&gyroX, &gyroY, &gyroX);
    // imu.getAccelData(&accX, &accY, &accZ);
    // imu.getGyroData(&gyroX, &gyroY, &gyroX);

    // sensors_event_t event; 
    // msa.getEvent(&event);
    sensor.updateAccelerometerData();
    accX= sensor.getAccelerometerX();
    accY = sensor.getAccelerometerY();
    accZ = sensor.getAccelerometerZ();

    // gyroX = event.gyro.x;
    // gyroY = event.gyro.y;
    // gyroZ = event.gyro.z;
    angle = atan2(accX, accY) * 180 / PI + 180; 
}
#include "SC7A20H.h"

// 构造函数，初始化传感器地址
SC7A20H::SC7A20H(TwoWire &wire) : _pWire(&wire)
{
    accX = 0;
    accY = 0;
    accZ = 0;
}

// 析构函数
SC7A20H::~SC7A20H(void)
{
    // 如果有需要释放的资源，可以在这里处理
    // 暂时没有动态分配资源，因此这里可以为空
}
// 假设 readData 和 writeCommand 通过某种方式来访问硬件寄存器
uint8_t  SC7A20H::readData(uint8_t addr, uint8_t arg, void *pBuf, size_t size)

// readData(uint8_t addr, uint8_t reg, uint8_t* buf, uint8_t len)
{
    uint8_t ret = 0;
    if (pBuf == NULL)
    {
        printf("pBuf ERROR!! : null pointer");
        return 0;
    }
    uint8_t *_pBuf = (uint8_t *)pBuf;
    _pWire->beginTransmission(addr);
    _pWire->write(arg);
    if (_pWire->endTransmission() != 0)
    {
        return 0;
    }
    ret = _pWire->requestFrom(addr, (uint8_t)size);
    for (uint16_t i = 0; i < size; i++)
    {
        _pBuf[i] = (char)_pWire->read();
    }
    return ret;
}

void SC7A20H::writeCommand(uint8_t addr, uint8_t reg, uint8_t cmd)
{
    _pWire->beginTransmission(addr);
    _pWire->write(reg);
    _pWire->write(cmd);
    _pWire->endTransmission();
}

void SC7A20H::initSC7A20H(void)
{
    uint8_t data = 0;
    uint8_t buf[2];
    readData(_sc7a20hAddr, 0x24, buf, 1);
    data = buf[0];
    data |= 0x08;
    writeCommand(_sc7a20hAddr, 0x24, data);
    // AOI1 CONFIG
    data = 0x00;
    data |= 0x40;
    data |= 0x03;
    data |= 0x0c;
    data |= 0x38;
    writeCommand(_sc7a20hAddr, 0x30, data);

    readData(_sc7a20hAddr, 0x21, buf, 1);
    data = buf[0];
    data |= 0x81;
    writeCommand(_sc7a20hAddr, 0x21, data);

    data = 0x60;
    writeCommand(_sc7a20hAddr, 0x32, data);

    data = 0x02;
    writeCommand(_sc7a20hAddr, 0x33, data);

    readData(_sc7a20hAddr, 0x22, buf, 1);
    data = buf[0];
    data |= 0x40;
    writeCommand(_sc7a20hAddr, 0x21, data);

    readData(_sc7a20hAddr, 0x24, buf, 1);
    data = buf[0];
    data |= 0x02;
    writeCommand(_sc7a20hAddr, 0x24, data);

    readData(_sc7a20hAddr, 0x25, buf, 1);
    data = buf[0];
    data |= 0x02;
    writeCommand(_sc7a20hAddr, 0x25, data);

    data = 0x00;
    data |= 0xc0;
    data |= 0x3f;
    writeCommand(_sc7a20hAddr, 0x34, data);

    readData(_sc7a20hAddr, 0x21, buf, 1);
    data = buf[0];
    data |= 0xfd;
    writeCommand(_sc7a20hAddr, 0x21, data);

    data = 0x18;
    writeCommand(_sc7a20hAddr, 0x36, data);

    data = 0x02;
    writeCommand(_sc7a20hAddr, 0x37, data);

    readData(_sc7a20hAddr, 0x25, buf, 1);
    data = buf[0];
    data |= 0x20;
    writeCommand(_sc7a20hAddr, 0x21, data);
    // fs_config
    readData(_sc7a20hAddr, 0x23, buf, 1);
    data = 0x80;
    data |= 0x08;
    data |= 0x00;
    writeCommand(_sc7a20hAddr, 0x23, data);

    // 电源
    data = 0x27;
    writeCommand(_sc7a20hAddr, 0x20, data);
}


void SC7A20H::updateAccelerometerData() {
    uint8_t buf[6];
    // 读取加速度数据
    readData(_sc7a20hAddr, 0xA8, buf, 6);
    
    // 处理并更新 X、Y、Z 数据
    accX = (int)((buf[1] << 8 | buf[0]) >> 4);
    accY = (int)((buf[3] << 8 | buf[2]) >> 4);
    accZ = (int)((buf[5] << 8 | buf[4]) >> 4);

    // 补码处理
    if ((accX & 0x800) == 0x800) {
        accX -= 4096;
    }
    if ((accY & 0x800) == 0x800) {
        accY -= 4096;
    }
    if ((accZ & 0x800) == 0x800) {
        accZ -= 4096;
    }
}

            // self->readData(0x19, 0xA8, buf, 6);
            // self->accX = (int)((buf[1] << 8 | buf[0]) >> 4);
            // self->accY = (int)((buf[3] << 8 | buf[2]) >> 4);
            // self->accZ = (int)((buf[5] << 8 | buf[4]) >> 4);
            // if ((self->accX & 0x800) == 0x800)
            // {
            //     self->accX -= 4096;
            // }
            // if ((self->accY & 0x800) == 0x800)
            // {
            //     self->accY -= 4096;
            // }
            // if ((self->accZ & 0x800) == 0x800)
            // {
            //     self->accZ -= 4096;
            // }

int SC7A20H::getAccelerometerX(void)
{
    return this->accX;
}

int SC7A20H::getAccelerometerY(void)
{
    return this->accY;
}

int SC7A20H::getAccelerometerZ(void)
{
    return this->accZ;
}

int SC7A20H::getStrength(void)
{
    int x = getAccelerometerX();
    int y = getAccelerometerY();
    int z = getAccelerometerZ();
    return sqrt(x * x + y * y + z * z);
}

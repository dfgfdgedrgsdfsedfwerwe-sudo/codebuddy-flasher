/**
 * @file AtkBoxTouch.h
 * @brief 正点原子 ESP32-S3 BOX CHSC5432 电容触摸屏 Arduino 驱动
 *
 * I2C: 地址 0x2E (7bit), 与 XL9555/ES8311 同总线 (SDA=48 SCL=45)
 * 复位经 XL9555 P0.6 (ATK_CTP_RST_IO)
 *
 * 寄存器协议 (移植自正点原子 chsc5xxx.c):
 *   触摸数据: 0x2000002C 起 28 字节
 *     buf[1] 低 4 位 = 触点数量
 *     触点 i 坐标 (竖屏):
 *       x = ((buf[5+i*5] & 0x0F) << 8) + buf[2+i*5]
 *       y = ((buf[5+i*5] >> 4)  << 8) + buf[3+i*5]
 *   芯片 ID: 0x20000080
 *
 * 屏幕方向校准宏: 若实测 X/Y 反了或镜像, 改下面 3 个宏
 */
#ifndef __ATK_BOX_TOUCH_H__
#define __ATK_BOX_TOUCH_H__

#include <Arduino.h>
#include <Wire.h>
#include "AtkBoxXL9555.h"

// ===== 方向校准宏 (联调时按实测调整) =====
#define TOUCH_SWAP_XY   0   // 0=不交换 (ATK BOX 实测正确)
#define TOUCH_FLIP_X    0   // 1=X 镜像
#define TOUCH_FLIP_Y    0   // 1=Y 镜像

#define CHSC5432_ADDR         0x2E
#define CHSC5432_CTRL_REG     0x2000002CUL
#define CHSC5432_PID_REG      0x20000080UL

class AtkTouch {
public:
    bool begin(TwoWire &wire = Wire) {
        _wire = &wire;

        // 复位触摸芯片 (经 XL9555)
        xl9555.digitalWrite16(ATK_CTP_RST_IO, 0);
        delay(100);
        xl9555.digitalWrite16(ATK_CTP_RST_IO, 1);
        delay(100);

        // 读芯片 ID 验证通信
        uint8_t id[4] = {0};
        if (!readRegs(CHSC5432_PID_REG, id, 4)) {
            Serial.println("[Touch] CHSC5432 read ID failed");
            return false;
        }
        Serial.printf("[Touch] CHSC5432 ID: 0x%02X\n", id[0]);
        _ready = true;
        return true;
    }

    // 扫描触摸状态; 返回 true 表示有触摸点, 坐标写入 x/y
    bool scan(uint16_t *x, uint16_t *y) {
        if (!_ready) return false;

        uint8_t buf[28];
        if (!readRegs(CHSC5432_CTRL_REG, buf, 28)) return false;

        uint8_t points = buf[1] & 0x0F;
        if (points == 0 || points > 5) return false;

        // 第一个触点
        uint16_t tx = ((uint16_t)(buf[5] & 0x0F) << 8) + buf[2];
        uint16_t ty = ((uint16_t)((buf[5] & 0xF0) >> 4) << 8) + buf[3];

        // 方向校准
#if TOUCH_SWAP_XY
        uint16_t t = tx; tx = ty; ty = t;
#endif
#if TOUCH_FLIP_X
        tx = 239 - tx;
#endif
#if TOUCH_FLIP_Y
        ty = 319 - ty;
#endif

        // 边界裁剪
        if (tx > 239) tx = 239;
        if (ty > 319) ty = 319;

        *x = tx;
        *y = ty;
        return true;
    }

private:
    TwoWire *_wire = nullptr;
    bool _ready = false;

    // 32bit 寄存器地址协议: 先写 4 字节地址, 再读数据
    bool readRegs(uint32_t reg, uint8_t *buf, uint8_t len) {
        uint8_t addr[4] = {
            (uint8_t)(reg >> 24), (uint8_t)(reg >> 16),
            (uint8_t)(reg >> 8),  (uint8_t)(reg & 0xFF)
        };
        _wire->beginTransmission(CHSC5432_ADDR);
        _wire->write(addr, 4);
        uint8_t err = _wire->endTransmission(false);
        if (err != 0) {
            static uint32_t lastErr = 0;
            if (millis() - lastErr > 5000) {  // 每 5 秒打印一次
                Serial.printf("[Touch] I2C write reg 0x%08X failed: %d\n", reg, err);
                lastErr = millis();
            }
            return false;
        }
        uint8_t n = _wire->requestFrom((uint8_t)CHSC5432_ADDR, len);
        if (n != len) {
            static uint32_t lastErr = 0;
            if (millis() - lastErr > 5000) {
                Serial.printf("[Touch] I2C read len mismatch: want %d, got %d\n", len, n);
                lastErr = millis();
            }
            return false;
        }
        for (uint8_t i = 0; i < len; i++) buf[i] = _wire->read();
        return true;
    }
};

// 全局实例
extern AtkTouch touch;

#endif // __ATK_BOX_TOUCH_H__

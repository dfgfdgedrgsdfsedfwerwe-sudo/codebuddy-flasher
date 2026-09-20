/**
 * @file AtkBoxXL9555.h
 * @brief 正点原子 ESP32-S3 BOX 板载 XL9555 IO 扩展芯片 Arduino 驱动
 *
 * I2C: 地址 0x20 (7bit), SDA=48, SCL=45, 400kHz
 * XL9555 = 16bit IO 扩展 (两个 8bit 端口), 寄存器:
 *   0/1=输入端口0/1  2/3=输出端口0/1  4/5=极性反转  6/7=方向配置 (0=输出,1=输入)
 *
 * IO 位定义 (来自官方例程 xl9555.h, ATK_DNESP32S3B):
 *   P0.0 AP_INT    P0.1 QMA_INT   P0.2 BEEP     P0.3 KEY1
 *   P0.4 KEY0      P0.5 SPK_CTRL  P0.6 CTP_RST  P0.7 LCD_BL
 *   P1.0 LEDR      P1.1 CTP_INT   P1.2~P1.7 空闲
 *
 * 注意: XL9555 与 K10 的 XL95x5 寄存器布局不同, 不能沿用 K10 的 ePin_t 枚举
 */
#ifndef __ATK_BOX_XL9555_H__
#define __ATK_BOX_XL9555_H__

#include <Arduino.h>
#include <Wire.h>

// XL9555 寄存器地址
#define XL9555_INPUT_PORT0_REG    0
#define XL9555_INPUT_PORT1_REG    1
#define XL9555_OUTPUT_PORT0_REG   2
#define XL9555_OUTPUT_PORT1_REG   3
#define XL9555_INVERSION_PORT0_REG 4
#define XL9555_INVERSION_PORT1_REG 5
#define XL9555_CONFIG_PORT0_REG   6   // 方向: 0=输出 1=输入
#define XL9555_CONFIG_PORT1_REG   7

#define XL9555_I2C_ADDR           0x20

// 16bit IO 位掩码 (低 8bit=P0, 高 8bit=P1)
#define ATK_AP_INT_IO    0x0001  // AP3216 光距中断 (输入)
#define ATK_QMA_INT_IO   0x0002  // QMI8658 中断 (输入)
#define ATK_BEEP_IO      0x0004  // 蜂鸣器
#define ATK_KEY1_IO      0x0008  // KEY1 按键 (输入, 按下=0)
#define ATK_KEY0_IO      0x0010  // KEY0 按键 (输入, 按下=0)
#define ATK_SPK_CTRL_IO  0x0020  // 喇叭功放使能 (NS4150B)
#define ATK_CTP_RST_IO   0x0040  // 触摸芯片复位
#define ATK_LCD_BL_IO    0x0080  // LCD 背光
#define ATK_LEDR_IO      0x0100  // 红色 LED
#define ATK_CTP_INT_IO   0x0200  // 触摸中断 (输入)

class AtkXL9555 {
public:
    bool begin(TwoWire &wire = Wire, uint8_t sda = 48, uint8_t scl = 45) {
        _wire = &wire;
        _addr = XL9555_I2C_ADDR;
        _wire->begin(sda, scl, 400000);

        // 初始输出状态: 蜂鸣器静音(1) + 触摸芯片退出复位(1) + 背光开(1) + 喇叭禁用(0)
        // ATK_BEEP_IO = 0x0004 (低电平响, 所以拉高静音)
        // ATK_CTP_RST_IO = 0x0040 (低电平复位, 拉高退出复位)
        // ATK_LCD_BL_IO = 0x0080 (高电平亮)
        uint16_t init_output = ATK_BEEP_IO | ATK_CTP_RST_IO | ATK_LCD_BL_IO;
        writeReg16(XL9555_OUTPUT_PORT0_REG, init_output);
        writeReg16(XL9555_CONFIG_PORT0_REG,
                   ATK_AP_INT_IO | ATK_QMA_INT_IO | ATK_KEY1_IO |
                   ATK_KEY0_IO | ATK_CTP_INT_IO);   // 这些位=1 → 输入
        _output_shadow = init_output;
        return readReg16(XL9555_INPUT_PORT0_REG, &_last_input);  // 读一次验证通信
    }

    // 读单个 IO (仅对配置为输入的脚有意义)
    int digitalRead16(uint16_t pin) {
        uint16_t val;
        if (!readReg16(XL9555_INPUT_PORT0_REG, &val)) return -1;
        return (val & pin) ? 1 : 0;
    }

    // 写单个 IO (带输出影子寄存器, 避免读改写冲突)
    bool digitalWrite16(uint16_t pin, int val) {
        if (val) _output_shadow |= pin;
        else     _output_shadow &= ~pin;
        return writeReg16(XL9555_OUTPUT_PORT0_REG, _output_shadow);
    }

    // 读当前输出影子 (调试用)
    uint16_t getOutputShadow() const { return _output_shadow; }

    // KEY0/KEY1 便捷读取 (按下返回 true)
    bool key0Pressed() { return digitalRead16(ATK_KEY0_IO) == 0; }
    bool key1Pressed() { return digitalRead16(ATK_KEY1_IO) == 0; }

private:
    TwoWire *_wire = nullptr;
    uint8_t  _addr = XL9555_I2C_ADDR;
    uint16_t _output_shadow = 0;

    bool writeReg16(uint8_t reg, uint16_t data) {
        _wire->beginTransmission(_addr);
        _wire->write(reg);
        _wire->write(data & 0xFF);
        _wire->write((data >> 8) & 0xFF);
        return _wire->endTransmission() == 0;
    }

    bool writeReg8(uint8_t reg, uint8_t data) {
        _wire->beginTransmission(_addr);
        _wire->write(reg);
        _wire->write(data);
        return _wire->endTransmission() == 0;
    }

    bool readReg16(uint8_t reg, uint16_t *data) {
        _wire->beginTransmission(_addr);
        _wire->write(reg);
        if (_wire->endTransmission(false) != 0) return false;
        uint8_t n = _wire->requestFrom((uint8_t)_addr, (uint8_t)2);
        if (n != 2) return false;
        uint8_t lo = _wire->read();
        uint8_t hi = _wire->read();
        *data = lo | (hi << 8);
        return true;
    }

    uint16_t _last_input = 0;
};

// 全局实例 (main 中定义: AtkXL9555 xl9555;)
extern AtkXL9555 xl9555;

#endif // __ATK_BOX_XL9555_H__

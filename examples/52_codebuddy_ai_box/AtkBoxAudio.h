/**
 * @file AtkBoxAudio.h
 * @brief 正点原子 ESP32-S3 BOX ES8311 音频编解码器 Arduino 驱动
 *
 * I2C: 地址 0x18, SDA=48 SCL=45
 * I2S: BCLK=21 WS=13 DIN(ESP收)=47 DOUT(ESP发)=14, MCLK=NC (从模式, SCLK 作时钟源)
 * 喇叭: NS4150B 功放, 使能经 XL9555 P0.5 (ATK_SPK_CTRL_IO)
 *
 * 寄存器初始化序列移植自正点原子 es8311.c (支持录音+播放)
 * 本项目用途: 16kHz 单声道录音 (ESP-NOW 发往 Dongle) + 喇叭播放 (提示音/TTS 预留)
 */
#ifndef __ATK_BOX_AUDIO_H__
#define __ATK_BOX_AUDIO_H__

#include <Arduino.h>
#include <Wire.h>
#include "AtkBoxXL9555.h"

#define ES8311_ADDR  0x18

// ==== 用到的寄存器 (完整定义见正点原子 es8311.h) ====
#define ES8311_RESET_REG00          0x00
#define ES8311_CLK_MANAGER_REG01    0x01
#define ES8311_CLK_MANAGER_REG02    0x02
#define ES8311_CLK_MANAGER_REG03    0x03
#define ES8311_CLK_MANAGER_REG04    0x04
#define ES8311_CLK_MANAGER_REG05    0x05
#define ES8311_CLK_MANAGER_REG06    0x06
#define ES8311_CLK_MANAGER_REG07    0x07
#define ES8311_CLK_MANAGER_REG08    0x08
#define ES8311_SDPIN_REG09          0x09
#define ES8311_SDPOUT_REG0A         0x0A
#define ES8311_SYSTEM_REG0B         0x0B
#define ES8311_SYSTEM_REG0C         0x0C
#define ES8311_SYSTEM_REG0D         0x0D
#define ES8311_SYSTEM_REG0E         0x0E
#define ES8311_SYSTEM_REG10         0x10
#define ES8311_SYSTEM_REG11         0x11
#define ES8311_SYSTEM_REG12         0x12
#define ES8311_SYSTEM_REG13         0x13
#define ES8311_SYSTEM_REG14         0x14
#define ES8311_GP_REG45             0x45
#define ES8311_ADC_REG15            0x15
#define ES8311_ADC_REG16            0x16
#define ES8311_ADC_REG17            0x17
#define ES8311_ADC_REG1B            0x1B
#define ES8311_ADC_REG1C            0x1C
#define ES8311_DAC_REG32            0x32
#define ES8311_DAC_REG37            0x37
#define ES8311_GPIO_REG44           0x44

// 时钟方案: 16kHz 采样, SCLK 作 MCLK 源 (从模式)
// I2S 16kHz 16bit → BCLK = 16000*2*16 = 512kHz (MCLK_DIV_FRE=32 时内部时钟树按此推算)
// 正点原子表: mclk=16kHz*32=512kHz 无此条, 但 SCLK 源模式下 0x2000002C 按下表 16k/2048000 行配置
struct coeff_div_t {
    uint32_t mclk; uint32_t rate;
    uint8_t pre_div, pre_multi, adc_div, dac_div, fs_mode, lrck_h, lrck_l, bclk_div, adc_osr, dac_osr;
};

static const coeff_div_t es8311_coeff_16k[] = {
    // mclk      rate   pre mult adcd dacd fs lrch lrcl bck  osr
    { 12288000, 16000, 0x03, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10 },
    { 16384000, 16000, 0x04, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10 },
    {  8192000, 16000, 0x02, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10 },
    {  4096000, 16000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10 },
    {  2048000, 16000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10 },
};

class AtkES8311 {
public:
    bool begin(TwoWire &wire = Wire, uint32_t sample_rate = 16000) {
        _wire = &wire;
        _sample_rate = sample_rate;

        // ==== 时钟方案选择 (从模式, SCLK=512kHz 作 MCLK) ====
        // 16kHz 16bit 双声道 BCLK = 512kHz; 用 2048000 行 (内部 4 分频后=512kHz)
        const coeff_div_t *cd = &es8311_coeff_16k[4];  // 2048000
        uint8_t ret = 0;

        // ==== 复位与基础配置 (移植自正点原子 es8311_init) ====
        ret |= wr(ES8311_GP_REG45,        0x00);
        ret |= wr(ES8311_CLK_MANAGER_REG01, 0x30);
        ret |= wr(ES8311_CLK_MANAGER_REG02, 0x00);
        ret |= wr(ES8311_CLK_MANAGER_REG03, 0x10);
        ret |= wr(ES8311_ADC_REG16,       0x24);
        ret |= wr(ES8311_CLK_MANAGER_REG04, 0x10);
        ret |= wr(ES8311_CLK_MANAGER_REG05, 0x00);
        ret |= wr(ES8311_SYSTEM_REG0B,    0x00);
        ret |= wr(ES8311_SYSTEM_REG0C,    0x00);
        ret |= wr(ES8311_SYSTEM_REG10,    0x1F);
        ret |= wr(ES8311_SYSTEM_REG11,    0x7F);
        ret |= wr(ES8311_RESET_REG00,     0x80);
        delay(80);

        // 从模式
        uint8_t regv = rd(ES8311_RESET_REG00);
        regv &= 0xBF;
        ret |= wr(ES8311_RESET_REG00, regv);
        ret |= wr(ES8311_SYSTEM_REG0D, 0x01);
        ret |= wr(ES8311_CLK_MANAGER_REG01, 0x3F);

        // 内部 MCLK 时钟源选择
        regv = rd(ES8311_CLK_MANAGER_REG01);
        regv |= 0x80;
        ret |= wr(ES8311_CLK_MANAGER_REG01, regv);

        // ==== 16kHz 时钟分频 ====
        regv = rd(ES8311_CLK_MANAGER_REG02) & 0x07;
        regv |= (cd->pre_div - 1) << 5;
        uint8_t mult = 0;
        switch (cd->pre_multi) {
            case 2:  mult = 1; break;
            case 4:  mult = 2; break;
            case 8:  mult = 3; break;
            default: mult = 0; break;
        }
        regv |= mult << 3;
        ret |= wr(ES8311_CLK_MANAGER_REG02, regv);

        regv = (cd->adc_div - 1) << 4 | (cd->dac_div - 1);
        ret |= wr(ES8311_CLK_MANAGER_REG05, regv);

        regv = rd(ES8311_CLK_MANAGER_REG03) & 0x80;
        regv |= cd->fs_mode << 6 | cd->adc_osr;
        ret |= wr(ES8311_CLK_MANAGER_REG03, regv);

        regv = rd(ES8311_CLK_MANAGER_REG04) & 0x80;
        regv |= cd->dac_osr;
        ret |= wr(ES8311_CLK_MANAGER_REG04, regv);

        regv = rd(ES8311_CLK_MANAGER_REG07) & 0xC0;
        regv |= cd->lrck_h;
        ret |= wr(ES8311_CLK_MANAGER_REG07, regv);

        ret |= wr(ES8311_CLK_MANAGER_REG08, cd->lrck_l);

        delay(80);
        regv = rd(ES8311_CLK_MANAGER_REG06) & 0xE0;
        regv |= (cd->bclk_div - 1);
        ret |= wr(ES8311_CLK_MANAGER_REG06, regv);

        // ==== DAC/ADC 接口: I2S 16bit ====
        uint8_t dac_iface = rd(ES8311_SDPIN_REG09) & 0xC0;
        uint8_t adc_iface = rd(ES8311_SDPOUT_REG0A) & 0xC0;
        dac_iface |= 0x0C;  dac_iface &= 0xFC;
        adc_iface |= 0x0C;  adc_iface &= 0xFC;
        ret |= wr(ES8311_SDPIN_REG09, dac_iface);
        ret |= wr(ES8311_SDPOUT_REG0A, adc_iface);

        // MCLK/SCLK 不翻转
        regv = rd(ES8311_CLK_MANAGER_REG01); regv &= ~0x40;
        ret |= wr(ES8311_CLK_MANAGER_REG01, regv);
        regv = rd(ES8311_CLK_MANAGER_REG06); regv &= ~0x20;
        ret |= wr(ES8311_CLK_MANAGER_REG06, regv);

        ret |= wr(ES8311_SYSTEM_REG14, 0x1A);
        // 模拟麦克风 (非 DMIC)
        regv = rd(ES8311_SYSTEM_REG14); regv &= ~0x40;
        ret |= wr(ES8311_SYSTEM_REG14, regv);

        ret |= wr(ES8311_SYSTEM_REG12, 0x00);
        ret |= wr(ES8311_SYSTEM_REG13, 0x10);
        ret |= wr(ES8311_SYSTEM_REG0E, 0x02);
        ret |= wr(ES8311_ADC_REG15,  0x40);
        ret |= wr(ES8311_ADC_REG1B,  0x0A);
        ret |= wr(ES8311_ADC_REG1C,  0x6A);
        ret |= wr(ES8311_DAC_REG37,  0x48);
        ret |= wr(ES8311_GPIO_REG44, 0x08);
        ret |= wr(ES8311_ADC_REG17,  0xBF);   // ADC 音量
        ret |= wr(ES8311_DAC_REG32,  0xBF);   // DAC 音量

        if (ret != 0) {
            Serial.println("[ES8311] init FAILED (I2C error)");
            return false;
        }
        Serial.printf("[ES8311] init OK @ %lu Hz\n", (unsigned long)_sample_rate);
        return true;
    }

    // DAC 输出音量 (0~100); 正点原子建议喇叭不超过 65
    bool setVolume(int volume) {
        if (volume < 0) volume = 0;
        if (volume > 100) volume = 100;
        // 0xBF = 0dB 参考; 音量映射: 0x00(最小)~0xFF(+24dB?)
        uint8_t vol = (uint8_t)(volume * 255 / 100);
        return wr(ES8311_DAC_REG32, vol);
    }

    // 静音控制: mute=1 静音
    bool setMute(bool mute) {
        uint8_t regv = rd(ES8311_SYSTEM_REG14);
        if (mute) regv |= 0x20; else regv &= ~0x20;
        return wr(ES8311_SYSTEM_REG14, regv);
    }

    // 喇叭功放使能 (XL9555)
    void speakerEnable(bool en) {
        xl9555.digitalWrite16(ATK_SPK_CTRL_IO, en ? 1 : 0);
    }

    // ADC 麦克风增益 (0~8 级, 对应 +4.5dB ~ +30dB 左右)
    bool setMicGain(uint8_t gain) {
        if (gain > 8) gain = 8;
        return wr(ES8311_ADC_REG17, 0xC0 | gain);
    }

private:
    TwoWire *_wire = nullptr;
    uint32_t _sample_rate = 16000;

    uint8_t wr(uint8_t reg, uint8_t val) {
        _wire->beginTransmission(ES8311_ADDR);
        _wire->write(reg);
        _wire->write(val);
        return _wire->endTransmission();
    }

    uint8_t rd(uint8_t reg) {
        _wire->beginTransmission(ES8311_ADDR);
        _wire->write(reg);
        _wire->endTransmission(false);
        _wire->requestFrom((uint8_t)ES8311_ADDR, (uint8_t)1);
        return _wire->read();
    }
};

// 全局实例
extern AtkES8311 es8311;

#endif // __ATK_BOX_AUDIO_H__

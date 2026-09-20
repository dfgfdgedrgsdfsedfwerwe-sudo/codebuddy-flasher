/**
 * @file AtkBoxBoard.h
 * @brief 正点原子 ESP32-S3 BOX 板级初始化 (整合 XL9555 + LCD + 触摸 + ES8311 + LVGL)
 *
 * 替代 K10 的 initBoard.h + main.h 组合。
 * 保留 K10 工程的调用接口形状: init_board() / lvgl_begin() / my_disp_flush() /
 * lv_fs_sd_init() (SD 'D:' 盘符桥接), 使 51_mic_wifi.ino 业务逻辑改动最小。
 *
 * 引脚总表 (ATK DNESP32S3B):
 *   LCD 并口: D0-D7=40/39/38/12/11/10/9/46  WR=42 RD=41 DC=2 CS=1
 *   I2C0:     SDA=48 SCL=45 (XL9555@0x20, ES8311@0x18, CHSC5432@0x2E)
 *   I2S0:     BCLK=21 WS=13 DIN=47 DOUT=14
 *   SD (SPI2): SCLK=7 MOSI=16 MISO=15 CS=17
 *   按键:     KEY1=XL9555 P0.3 → 按键A    KEY0=XL9555 P0.4 → 按键B
 *             BOOT=GPIO0 (仅下载模式, 不做功能键)
 */
#ifndef __ATK_BOX_BOARD_H__
#define __ATK_BOX_BOARD_H__

#include <Arduino.h>
#include <Wire.h>
#include "lvgl.h"
#include <LovyanGFX.hpp>
#include "FS.h"
#include "SD.h"
#include "SPI.h"

#include "AtkBoxGfx.hpp"
#include "AtkBoxXL9555.h"
#include "AtkBoxTouch.h"
#include "AtkBoxAudio.h"

// ======================= 板级实例定义 =======================
AtkXL9555 xl9555;
AtkTouch  touch;
AtkES8311 es8311;
static AtkBoxLcd tft;

// SD 卡引脚 (SPI2)
#define SD_CS    17
#define SD_MOSI  16
#define SD_MISO  15
#define SD_SCLK  7

// I2S 音频引脚 (ES8311)
#define MIC_MCLK   -1   // NC, 从模式用 SCLK 作时钟源
#define MIC_BCLK   21
#define MIC_WS     13
#define MIC_DIN    47   // ES8311 DOUT → ESP (录音)
#define MIC_DOUT   14   // ESP → ES8311 DIN (播放)
#define SAMPLE_RATE 16000

// ======================= LVGL 显示对接 =======================
static lv_disp_t *Lcd_disp;
static void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area,
                          lv_color_t *color_p);
static void atk_touch_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data);

static void lvgl_begin(void) {
#define LVGL_HOR_RES (240)
#define LVGL_VER_RES (320)

    tft.init();
    tft.initDMA();
    tft.setRotation(0);
    lv_init();

    static lv_disp_draw_buf_t draw_buf;
    size_t DISP_BUF_SIZE = sizeof(lv_color_t) * (LVGL_HOR_RES * LVGL_VER_RES);
    static lv_color_t *buf1 = (lv_color_t *)heap_caps_malloc(
        DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    static lv_color_t *buf2 = (lv_color_t *)heap_caps_malloc(
        DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);

    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, DISP_BUF_SIZE);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LVGL_HOR_RES;
    disp_drv.ver_res = LVGL_VER_RES;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.full_refresh = 1;
    Lcd_disp = lv_disp_drv_register(&disp_drv);

    // ---- 触摸输入设备注册 ----
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = atk_touch_read;
    lv_indev_drv_register(&indev_drv);
}

static void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area,
                          lv_color_t *color_p) {
    int w = (area->x2 - area->x1 + 1);
    int h = (area->y2 - area->y1 + 1);
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.writePixelsDMA((lgfx::rgb565_t *)&color_p->full, w * h);
    tft.endWrite();
    lv_disp_flush_ready(disp);
}

// ======================= 触摸 LVGL 回调 =======================
// LVGL 周期调用 (默认 30ms); 与正点原子 lvgl_demo.c 的 touchpad_read 同构
static void atk_touch_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
    static lv_coord_t last_x = 0;
    static lv_coord_t last_y = 0;

    uint16_t x, y;
    if (touch.scan(&x, &y)) {
        last_x = x;
        last_y = y;
        data->state = LV_INDEV_STATE_PR;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
    data->point.x = last_x;
    data->point.y = last_y;
}

// ======================= LVGL 文件系统桥接 SD 卡 =======================
// 与 K10 main.h 完全一致的 'D:' 盘符桥接 (避开 LVGL 内置 FATFS 的 'S')
static void *fs_sd_open_cb(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode) {
    (void)drv;
    const char *flags = (mode == LV_FS_MODE_WR) ? FILE_WRITE : FILE_READ;
    File *f = new File(SD.open(path, flags));
    return (f && *f) ? (void *)f : NULL;
}

static lv_fs_res_t fs_sd_close_cb(lv_fs_drv_t *drv, void *file_p) {
    (void)drv;
    File *f = (File *)file_p;
    f->close();
    delete f;
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_sd_read_cb(lv_fs_drv_t *drv, void *file_p, void *buf, uint32_t btr, uint32_t *br) {
    (void)drv;
    File *f = (File *)file_p;
    *br = f->read((uint8_t *)buf, btr);
    return (*br > 0) ? LV_FS_RES_OK : LV_FS_RES_UNKNOWN;
}

static lv_fs_res_t fs_sd_seek_cb(lv_fs_drv_t *drv, void *file_p, uint32_t pos, lv_fs_whence_t whence) {
    (void)drv;
    File *f = (File *)file_p;
    SeekMode sm = (whence == LV_FS_SEEK_SET) ? SeekSet : (whence == LV_FS_SEEK_CUR) ? SeekCur : SeekEnd;
    return f->seek(pos, sm) ? LV_FS_RES_OK : LV_FS_RES_UNKNOWN;
}

static lv_fs_res_t fs_sd_tell_cb(lv_fs_drv_t *drv, void *file_p, uint32_t *pos_p) {
    (void)drv;
    File *f = (File *)file_p;
    *pos_p = f->position();
    return LV_FS_RES_OK;
}

static void lv_fs_sd_init(void) {
    static lv_fs_drv_t fs_drv;
    lv_fs_drv_init(&fs_drv);
    fs_drv.letter = 'D';
    fs_drv.open_cb = fs_sd_open_cb;
    fs_drv.close_cb = fs_sd_close_cb;
    fs_drv.read_cb = fs_sd_read_cb;
    fs_drv.seek_cb = fs_sd_seek_cb;
    fs_drv.tell_cb = fs_sd_tell_cb;
    lv_fs_drv_register(&fs_drv);
}

// ======================= 板级初始化 (替代 K10 init_board) =======================
// K10 兼容层: 主程序里 digital_read(eP5_KeyA)==0 / digital_read(eP11_KeyB)==0 的调用
// 映射到新板 KEY1(A) / KEY0(B), 按下=0 逻辑保持一致
static esp_err_t init_board(void) {
    Serial.println("\n>>> ATK ESP32-S3 BOX board init <<<");

    // 1. XL9555 (先于其他 I2C 设备: 背光/触摸复位/功放都挂它上面)
    if (!xl9555.begin()) {
        Serial.println("[XL9555] init FAILED!");
    } else {
        Serial.println("[XL9555] init OK");
    }

    // 2. LCD 背光 (P0.7 拉高)
    xl9555.digitalWrite16(ATK_LCD_BL_IO, 1);
    Serial.println("[LCD] backlight ON");

    // 3. 触摸芯片
    if (!touch.begin()) {
        Serial.println("[Touch] init FAILED (UI 无触摸, 按键仍可用)");
    }

    // 4. ES8311 音频编解码器
    if (!es8311.begin(Wire, SAMPLE_RATE)) {
        Serial.println("[ES8311] init FAILED (音频不可用)");
    } else {
        es8311.setMute(true);           // 开机静音，避免 I2S TX 空跑时放大底噪
        es8311.setVolume(60);           // 建议不超过 65
        es8311.speakerEnable(false);    // NS4150B 功放关闭（本项目只用麦克风录音）
        Serial.println("[ES8311] init OK (speaker muted, mic ready)");
    }

    return ESP_OK;
}

#endif // __ATK_BOX_BOARD_H__

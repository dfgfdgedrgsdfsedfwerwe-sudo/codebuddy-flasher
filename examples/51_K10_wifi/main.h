#ifndef _MAIN_H_
#define _MAIN_H_
#include "Lcd_handler.hpp"
#include <LovyanGFX.hpp>
#include "lvgl.h"
#include "FS.h"
#include "SD.h"

#include "XL95x5_Driver.h"

// XL95x5 IO 扩展芯片实例在 initBoard.cpp 中定义，这里仅声明引用
extern XL95x5 ExpandPin;
static K10_Lcd tft;
static void lvgl_begin(void);
static void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area,
                          lv_color_t *color_p);
lv_disp_t *Lcd_disp;

static void lvgl_begin(void) {
#define LVGL_HOR_RES (240)
#define LVGL_VER_RES (320)

    // 背光/扩展芯片已由 init_board() 初始化，这里只初始化屏幕
    tft.init();
    tft.initDMA();
    tft.setRotation(0);
    lv_init();

    static lv_disp_draw_buf_t draw_buf;
    size_t DISP_BUF_SIZE =
        sizeof(lv_color_t) * (LVGL_HOR_RES * LVGL_VER_RES);
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
    lv_disp_drv_register(&disp_drv);
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

// ======================= LVGL 文件系统桥接 SD 卡 =======================
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

// 初始化 LVGL SD 卡驱动（挂载为 D: 盘符）
// 注意: 不能用 'S'，LVGL 内置 FATFS 驱动 (LV_USE_FS_FATFS) 已占用 'S' 盘符
static void lv_fs_sd_init(void) {
    static lv_fs_drv_t fs_drv;
    lv_fs_drv_init(&fs_drv);
    fs_drv.letter = 'D';  // SD 卡挂载为 D: 盘符 (避开内置 FATFS 的 'S')
    fs_drv.open_cb = fs_sd_open_cb;
    fs_drv.close_cb = fs_sd_close_cb;
    fs_drv.read_cb = fs_sd_read_cb;
    fs_drv.seek_cb = fs_sd_seek_cb;
    fs_drv.tell_cb = fs_sd_tell_cb;
    lv_fs_drv_register(&fs_drv);
}

#endif

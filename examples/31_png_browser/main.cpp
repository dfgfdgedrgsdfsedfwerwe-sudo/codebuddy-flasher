
/**
 * @file camera.ino
 * @brief The running example will display the camera feed on the screen
 *
 * @copyright   Copyright (c) 2010 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @license     The MIT License (MIT)
 * @author tangjie(jie.tang@dfrobot.com)
 * @version V1.0
 * @date 2024-6-21
 */

#include "main.h"
#include <demos/lv_demos.h>
#include <examples/lv_examples.h>
#include "SD_Card.h"
#include "Button_handler.h"

SdCard tf;

Button *buttonA = new Button(eP5_KeyA);

Button *buttonB = new Button(eP11_KeyB);

Button *buttonAB = new Button(eP5_KeyA, eP11_KeyB);

LV_IMG_DECLARE(output)
SemaphoreHandle_t xGuiSemaphore;

#define MAX_FILES    100
#define MAX_PATH_LEN 256

lv_obj_t *img;
lv_obj_t *btn_prev;
lv_obj_t *btn_next;

#define MAX_PNG_FILES   100  // 假设最多存储 1000 个 PNG 文件路径
#define MAX_PATH_LENGTH 256  // 假设路径长度不超过 255 个字符

// 全局变量
char img_paths[MAX_PNG_FILES][MAX_PATH_LENGTH];  // 存储 PNG 文件路径的数组
int png_count = 0;                               // 记录找到的 PNG 文件数量

// char *img_paths[MAX_FILES];
// int img_count = 0;
int current_img_index = 0;

void load_img(int index);
void prev_img_event_cb(lv_event_t *e);
void next_img_event_cb(lv_event_t *e);
void create_img_browser(void);
int my_strcasecmp(const char *s1, const char *s2);
void find_png_files_recursive(const char *base_path);
void KEYB(void);
void KEYA(void);

TimerHandle_t xAutoSwitchTimer = NULL;

void vTimerCallback(TimerHandle_t pxTimer) {
    current_img_index++;
    if (current_img_index >= png_count) {
        current_img_index = 0;
    }
    load_img(current_img_index);
}

void setup() {
    Serial.begin(115200);
    //   Wire.begin(47,48);
    pinMode(0, OUTPUT);  // suppress the noise
    digitalWrite(0, LOW);
    Wire.begin(47, 48);
    pinMode(45, OUTPUT);
    digitalWrite(45, HIGH);
    lvgl_begin();
    xGuiSemaphore = xSemaphoreCreateMutex();
    xSemaphoreGive(xGuiSemaphore);
    tf.init();
    lv_fs_fatfs_init();

    find_png_files_recursive("S:");

    printf("Found %d PNG files:\n", png_count);
    for (int i = 0; i < png_count; i++) {
        printf("Stored PNG file: %s\n", img_paths[i]);
    }
    create_img_browser();
    buttonA->setPressedCallback(&KEYA);
    buttonB->setPressedCallback(&KEYB);

    if (png_count > 0) {
        // 加载第一张图片
        load_img(0);
    } else {
        printf("No .jpg files found!\n");
    }

    xAutoSwitchTimer =
        xTimerCreate("AutoSwitchTimer",     // 定时器名称
                     pdMS_TO_TICKS(10000),  // 定时器周期，2000ms = 2s
                     pdTRUE,                // 自动重载
                     (void *)0,             // 定时器ID
                     vTimerCallback         // 定时器回调函数
        );

    // 启动定时器
    // if (xAutoSwitchTimer != NULL) {
    //     xTimerStart(xAutoSwitchTimer, 0);
    // }
}

void loop() {
    /* Try to take the semaphore, call lvgl related function on success */
    vTaskDelay(pdMS_TO_TICKS(10));
    /* Try to take the semaphore, call lvgl related function on success */
    if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) {
        lv_task_handler();
        xSemaphoreGive(xGuiSemaphore);
    }
}

void load_img(int index) {
    if (index < 0 || index >= png_count) {
        return;  // 超出范围
    }
    lv_img_set_src(img, img_paths[index]);
}

void prev_img_event_cb(lv_event_t *e) {
    current_img_index--;
    if (current_img_index < 0) {
        current_img_index = png_count - 1;
    }
    load_img(current_img_index);
}

void next_img_event_cb(lv_event_t *e) {
    current_img_index++;
    if (current_img_index >= png_count) {
        current_img_index = 0;
    }
    load_img(current_img_index);
}

void create_img_browser(void) {
    // 创建一个样式对象
    static lv_style_t btn_style;
    lv_style_init(&btn_style);
    // 设置按钮的背景颜色
    lv_style_set_bg_color(&btn_style, lv_palette_main(LV_PALETTE_CYAN));
    // 设置按钮的背景透明度
    lv_style_set_bg_opa(&btn_style, 40);
    // 设置按钮的文本颜色
    lv_style_set_text_color(&btn_style, lv_color_white());

    img = lv_img_create(lv_scr_act());
    lv_obj_align(img, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_clear_flag(img, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    btn_prev = lv_btn_create(lv_scr_act());
    lv_obj_add_style(btn_prev, &btn_style, 0);  // 将样式应用到按钮上
    lv_obj_align(btn_prev, LV_ALIGN_BOTTOM_LEFT, 10, -10);
    lv_obj_t *label_prev = lv_label_create(btn_prev);
    lv_label_set_text(label_prev, "Prev");

    btn_next = lv_btn_create(lv_scr_act());
    lv_obj_add_style(btn_next, &btn_style, 0);  // 将样式应用到按钮上
    lv_obj_align(btn_next, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    lv_obj_t *label_next = lv_label_create(btn_next);
    lv_label_set_text(label_next, "Next");

    lv_obj_add_event_cb(btn_prev, prev_img_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(btn_next, next_img_event_cb, LV_EVENT_CLICKED, NULL);
}

// void lvgl_init(void) {
//     lv_init();
//     lv_fs_if_init();  // 初始化文件系统接口
//     // 其他初始化代码，如显示驱动、输入设备等
// }

// 自定义不区分大小写的字符串比较函数
int my_strcasecmp(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        int diff = tolower(*s1) - tolower(*s2);
        if (diff != 0) {
            return diff;
        }
        s1++;
        s2++;
    }
    return tolower(*s1) - tolower(*s2);
}

// 递归查找 PNG 文件并将路径存储到全局数组中
void find_png_files_recursive(const char *base_path) {
    lv_fs_dir_t dir;
    lv_fs_res_t res;
    char fn[MAX_PATH_LENGTH];         // 用于存储文件名或目录名
    char full_path[MAX_PATH_LENGTH];  // 用于存储完整路径

    // 打开当前目录
    res = lv_fs_dir_open(&dir, base_path);
    if (res != LV_FS_RES_OK) {
        printf("Failed to open directory: %s\n", base_path);
        return;  // 返回表示没有找到文件
    }

    // 遍历目录中的文件和子目录
    while (png_count < MAX_PNG_FILES) {
        res = lv_fs_dir_read(&dir, fn);
        if (res != LV_FS_RES_OK || fn[0] == '\0') {
            break;  // 读取结束或出错
        }

        // 构建完整路径
        snprintf(full_path, MAX_PATH_LENGTH, "%s/%s", base_path, fn);
        printf("Checking: %s\n", full_path);  // 调试：打印正在检查的路径

        // 检查是否为目录
        lv_fs_file_t file;
        if (lv_fs_open(&file, full_path, LV_FS_MODE_RD) == LV_FS_RES_OK) {
            lv_fs_close(&file);  // 关闭文件
        } else {
            // 如果是目录，递归查找
            find_png_files_recursive(full_path);
        }

        // 检查文件扩展名是否为 ".png"
        const char *filename = lv_fs_get_last(fn);       // 提取文件名
        const char *ext      = lv_fs_get_ext(filename);  // 获取扩展名
        if (strcmp(ext, "png") == 0) {
            // 将文件路径存储到全局数组中
            snprintf(img_paths[png_count], MAX_PATH_LENGTH, "%s", full_path);
            printf("Found PNG file: %s\n", img_paths[png_count]);
            png_count++;
        }
    }

    // 关闭目录
    lv_fs_dir_close(&dir);
}

void KEYB(void)  // Callback function for button B
{
    current_img_index++;
    if (current_img_index >= png_count) {
        current_img_index = 0;
    }
    load_img(current_img_index);
}

void KEYA(void)  // Callback function for button A
{
    current_img_index--;
    if (current_img_index < 0) {
        current_img_index = png_count - 1;
    }
    load_img(current_img_index);
}

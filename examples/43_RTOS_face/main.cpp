#include "Lcd_handler.hpp"
#include <Wire.h>
#include <initBoard.h>
#include <esp_camera.h>
#include "who_camera.hpp"
#include "human_face_detect_msr01.hpp"
#include "human_face_detect_mnp01.hpp"
#include "dl_image.hpp"

#define LCD_WIDTH (320)
#define LCD_HEIGHT (240)

K10_Lcd lcd;
LGFX_Sprite canvas(&lcd);

int xf = 160, yf = 120;

#define FACE_COLOR_WHITE 0x00FFFFFF

TaskHandle_t camera_task_handle;
TaskHandle_t ai_task_handle;
QueueHandle_t xQueueCamer = NULL;
QueueHandle_t xQueueAIFrameO = NULL;

/**
 * @brief       摄像头图像数据获取任务
 * @param       arg：未使用
 * @retval      无
 */


static void face_coordinate(camera_fb_t *fb, std::list<dl::detect::result_t> *results, int face_id)
{
    int x, y, w, h;

    int i = 0;


    for (const auto &prediction : *results)
    {
        x = (int)prediction.box[0];
        y = (int)prediction.box[1];

        if (y < 0)
            y = 0;

        w = (int)prediction.box[2] - x + 1;
        h = (int)prediction.box[3] - y + 1;

        if ((x + w) > fb->width)
            w = fb->width - x;
        if ((y + h) > fb->height)
            h = fb->height - y;

        xf = x + w / 2;
        yf = y + h / 2;

        canvas.drawRect(x, y, w, h, TFT_GREEN);
    }
}

static void ai_process_handler(void *arg)
{
    arg = arg;
    camera_fb_t *face_ai_frameI = NULL;
    HumanFaceDetectMSR01 detector(0.1F, 0.5F, 10, 0.2F);
    HumanFaceDetectMNP01 detector2(0.4F, 0.3F, 10);
    int face_id = 0;
    while (1)
    {
        /* 以队列的形式获取摄像头图像数据 */
        if (xQueueReceive(xQueueCamer, &face_ai_frameI, portMAX_DELAY))
        {
            /* 判断图像是否出现人脸 */
            std::list<dl::detect::result_t> &detect_candidates = detector.infer((uint16_t *)face_ai_frameI->buf, {(int)face_ai_frameI->height, (int)face_ai_frameI->width, 3});
            // std::list<dl::detect::result_t> &detect_results = detector2.infer((uint16_t *)face_ai_frameI->buf, {(int)face_ai_frameI->height, (int)face_ai_frameI->width, 3}, detect_candidates);

            canvas.pushImage(0, 0, face_ai_frameI->width, face_ai_frameI->height, (uint16_t *)face_ai_frameI->buf); // Draw camera frame

            if (detect_candidates.size() > 0)
            {

                face_coordinate((camera_fb_t *)face_ai_frameI->buf, &detect_candidates, face_id);
            }

            lcd.startWrite();
            canvas.pushSprite(0, 0);
           lcd.endWrite();

            esp_camera_fb_return(face_ai_frameI);
        }

        /* 以队列的形式发送AI处理的图像 */
        // xQueueSend(xQueueAIFrameO, &face_ai_frameI, portMAX_DELAY);
    }
}



// static void face_coordinate(fb_data_t *fb, std::list<dl::detect::result_t> *results, int face_id) {



uint8_t esp_face_detection_ai_strat(void)
{
    /* 创建队列及任务 */
    xQueueCamer = xQueueCreate(5, sizeof(camera_fb_t *));
    xQueueAIFrameO = xQueueCreate(5, sizeof(camera_fb_t *));
    // xTaskCreatePinnedToCore(camera_process_handler, "camera_process_handler", 4 * 1024, NULL, 5, &camera_task_handle, 1);
    register_camera(PIXFORMAT_RGB565, FRAMESIZE_HVGA, 3, xQueueCamer);  
    xTaskCreatePinnedToCore(ai_process_handler, "ai_process_handler", 6 * 1024, NULL, 5, &ai_task_handle, 1);

    if (xQueueCamer != NULL || xQueueAIFrameO != NULL || camera_task_handle != NULL || ai_task_handle != NULL)
    {
        return 0;
    }

    return 1;
}

void esp_face_detection_ai_deinit(void)
{
    if (xQueueCamer != NULL)
    {
        vQueueDelete(xQueueCamer);
    }

    if (xQueueAIFrameO != NULL)
    {
        vQueueDelete(xQueueAIFrameO);
    }

    if (camera_task_handle != NULL)
    {
        vTaskDelete(camera_task_handle);
    }

    if (ai_task_handle != NULL)
    {
        vTaskDelete(ai_task_handle);
    }
}

void setup()
{
    Serial.begin(115200);
    pinMode(0, OUTPUT); // suppress the noise
    digitalWrite(0, LOW);
    init_board();
    lcd.init();
    lcd.setRotation(0);

    // camera_init();
    canvas.setPsram(true);
    canvas.createSprite(240, 320);
    esp_face_detection_ai_strat();
}

void loop()
{
    // camera_capture_and_face_detect();
    // delay(200);
}
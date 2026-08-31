/**
 * @file 51_mic_wifi.ino
 * @brief CodeBuddy P1 Enhanced: I2S Mic + ESP-NOW + Status Display
 *
 * 功能:
 * - K10 板载麦克风 -> ESP-NOW 音频流发送到 Dongle
 * - 接收 Dongle 回传的 Token/Project 状态数据
 * - 六个 LVGL 界面:
 *   界面 0: ESP-NOW 连接状态
 *   界面 1: Token Usage (AI 服务配额使用)
 *   界面 2: Coding Status (项目进度列表)
 *   界面 3: Product Inspo (产品灵感卡片)
 *   界面 4: User Profile (用户信息)
 *   界面 5: AI Status (AI 工作状态动画)
 *
 * 按键交互 (ATK BOX, UX 重定义 2026-08-28):
 * - 按键 A (KEY1): 短按=切换音频流+发送F2, 长按=发送Esc(取消)
 * - 按键 B (KEY0): 短按=发送Enter(确认), 长按=连续发送Backspace
 * - 按键 C (BOOT): 短按=返回主界面(AI Status), 长按=跳转触摸测试界面
 * - RST: 硬件复位 (不可作功能键)
 * - A+B 同时 2 秒: 触发演示模式
 * - 触摸: 左右滑动切换界面 + 界面内单击/长按交互
 *
 * @date 2026-08-28 (ATK BOX 交互重定义版)
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Wire.h>
#include "driver/i2s.h"
#include "AtkBoxBoard.h"      // ATK ESP32-S3 BOX 板级支持 (替代 initBoard.h + main.h)
#include "espnow_protocol.h"

// ======================= ESP-NOW 配置 =======================
static uint8_t dongle_mac[6] = {0xe0, 0x72, 0xa1, 0xd4, 0x8f, 0xe0};
static const int ESPNOW_CHANNEL = 1;

// ======================= I2S 麦克风引脚 (ES8311, 已移至 AtkBoxBoard.h) =======================
#define MIC_CHANNEL_OFFSET  0

// ======================= 按键 K10 兼容层 =======================
// K10: 按键 A/B 在 XL95x5 上; ATK BOX: KEY1(P0.3)=A, KEY0(P0.4)=B, BOOT(GPIO0)=C
// 按键 A/B 均为按下=0; BOOT 按键按下=0 (内部上拉)
#define digital_read_key_a()   (xl9555.key1Pressed() ? 0 : 1)
#define digital_read_key_b()   (xl9555.key0Pressed() ? 0 : 1)
#define digital_read_key_boot()  digitalRead(0)  // BOOT 按键在 GPIO0

// ======================= 全局变量 =======================
static int16_t stereoBuf[AUDIO_SAMPLES_PER_FRAME * 2];
static int16_t monoBuf[AUDIO_SAMPLES_PER_FRAME];

static bool     streaming      = false;
static bool     prev_a         = false;
static bool     prev_b         = false;
static bool     prev_boot      = false;
static uint32_t press_time_a   = 0;
static uint32_t press_time_b   = 0;
static uint32_t press_time_boot = 0;
static uint32_t last_repeat_b  = 0;
static uint32_t packetCount    = 0;
static uint32_t sendFailCount  = 0;
static const uint32_t LONG_PRESS_MS = 600;
static const uint32_t REPEAT_INTERVAL_MS = 100;
static const uint32_t DEMO_TRIGGER_MS = 2000;  // A+B 同时按 2 秒触发演示
static uint8_t  audio_seq_num  = 0;
static bool     espnow_ready   = false;

// AI 情绪状态机 (无真实数据时自动循环切换; 后续可由上位机驱动)
typedef enum {
    AI_STATE_THINKING = 0,  // 思考: 挑眉 + 闭嘴 + "Thinking..."
    AI_STATE_CODING   = 1,  // 编码: 皱眉 + 说话动嘴 + "Coding..."
    AI_STATE_DONE     = 2,  // 完成: 平眉 + 微笑 + "Done!"
    AI_STATE_COUNT    = 3,
} ai_state_t;
static ai_state_t ai_state = AI_STATE_THINKING;
static const uint32_t AI_STATE_DURATION_MS = 4000;  // 每个状态持续 4 秒

// AI 情绪外部覆盖 (收到上位机 AI_STATE 帧后停用本地自动循环)
static bool     ai_external_override = false;   // true=由上位机驱动, 停止自动循环
static uint32_t ai_last_ext_frame   = 0;        // 上次收到外部帧的时间
static const uint32_t AI_OVERRIDE_TIMEOUT_MS = 15000; // 15秒无外部帧则恢复自动循环
static char     ai_custom_text[20]  = {0};      // 上位机自定义文字 (空则用默认)
static bool     ai_state_changed    = false;    // 标记需要应用新情绪

// 界面管理
// 界面ID: 0=Status, 1=Token, 2=Project, 3=Inspo, 4=Profile, 5=AI, 6=TouchTest
static uint8_t  current_screen = 5;  // 开机默认 AI Status
static bool     screen_dirty   = true;
// 界面切换循环顺序: 5(AI) -> 2(Project) -> 1(Token) -> 3(Inspo) -> 4(Profile) -> 6(TouchTest) -> 0(Status) -> 回到5
static const uint8_t SCREEN_ORDER[7] = {5, 2, 1, 3, 4, 6, 0};
static uint8_t  order_index = 0;     // 当前在 SCREEN_ORDER 中的位置

// Token 状态数据 (从 Dongle 接收)
static token_status_frame_t token_data;
static bool token_data_valid = false;

// Project 状态数据 (从 Dongle 接收)
static project_status_frame_t project_data;
static bool project_data_valid = false;

// 决策状态 (Claude Code ⇄ K10 双向触摸决策)
static decision_request_frame_t g_decision_req;
static volatile bool g_decision_pending = false;  // 收到新请求待切界面
static volatile bool g_decision_active  = false;  // 决策界面正显示中
static uint32_t g_decision_deadline_ms = 0;       // 超时时刻
static int g_decision_sel = -1;                   // 当前高亮选项 (-1=未选)

// 演示模式状态
static bool demo_mode_active = false;

// SD 卡状态
static bool sd_card_ready = false;
static SPIClass *sd_spi = nullptr;

// SD 卡图片路径（SD 卡根目录，D: 盘符对应自定义 SD 驱动）
static const char *profile_image_path = "D:/user.png";  // 界面 4 用户照片 (Clay Portrait)
static const char *ai_status_image_path = "D:/ai.png";  // 界面 5 AI 状态图 (云朵)

// LVGL 对象指针
static lv_obj_t *screen_status = NULL;
static lv_obj_t *screen_token = NULL;
static lv_obj_t *screen_project = NULL;
static lv_obj_t *screen_inspo = NULL;
static lv_obj_t *screen_profile = NULL;
static lv_obj_t *screen_ai_status = NULL;
static lv_obj_t *screen_touch_test = NULL;  // 触摸测试界面
static lv_obj_t *detail_card_bg = NULL;     // 详情卡片遮罩层 (NULL=未打开)
static uint32_t last_swipe_ms = 0;          // 上次左右滑动切屏时间戳 (供屏幕级点击回调过滤滑动)

// FreeRTOS 互斥锁
static SemaphoreHandle_t xGuiSemaphore;

// ======================= 交互状态变量 =======================
// 界面 1 Token Usage 交互: 高亮选择 + 重置
static uint8_t token_selected = 0;   // 当前高亮的模型行索引

// 界面 3 Product Inspo 打字动画
static const char *inspo_full_text = nullptr;    // 完整文本 (指向 inject_demo_data 里的字面量)
static bool     inspo_typing_active = false;     // 是否正在打字
static uint16_t inspo_typing_index = 0;          // 已显示字符数
static uint32_t inspo_last_type_ms = 0;          // 上次显示字符时间戳
static bool     inspo_cursor_visible = false;    // 光标当前显示状态
static uint32_t inspo_last_cursor_ms = 0;        // 光标闪烁计时
static const uint32_t TYPING_INTERVAL_MS = 40;   // 每字符间隔
static const uint32_t CURSOR_BLINK_MS = 500;     // 光标闪烁间隔

// 界面 4 User Profile: SD 卡多头像切换
#define MAX_PROFILE_IMAGES 10
static char    profile_paths[MAX_PROFILE_IMAGES][16];  // LVGL 路径 "D:/userN.png"
static uint8_t profile_count = 0;                      // 扫描到的图片数
static uint8_t profile_index = 0;                      // 当前显示索引

// ======================= 前向声明 =======================
static void create_screen_status();
static void create_screen_token();
static void create_screen_project();
static void create_screen_inspo();
static void create_screen_profile();
static void create_screen_ai_status();
static void update_screen_status();
static void update_screen_token();
static void update_screen_project();
static void update_screen_inspo();
static void update_screen_profile();
static void update_screen_ai_status();
static void switch_screen(uint8_t screen_num);
static void create_screen_decision();
static void update_screen_decision();
static void send_decision_reply(uint16_t id, uint8_t index);
static void show_detail_card(const char *title, const char *body);
static void close_detail_card();
static bool detail_card_open();
static void detail_card_bg_event(lv_event_t *e);
static void start_typing_animation();
static void inspo_title_event(lv_event_t *e);
static void inspo_body_event(lv_event_t *e);
static void profile_img_event(lv_event_t *e);
static void ai_face_event(lv_event_t *e);
static void token_row_event(lv_event_t *e);

// ======================= ESP-NOW 回调 =======================
static void espnow_send_cb(const uint8_t *mac, esp_now_send_status_t status) {
    if (status != ESP_NOW_SEND_SUCCESS) {
        sendFailCount++;
    }
}

static void espnow_recv_cb(const uint8_t *mac, const uint8_t *data, int len) {
    if (len < 1) return;

    uint8_t frame_type = data[0];

    // Token 状态帧
    if (frame_type == FRAME_TYPE_TOKEN_STATUS && len == sizeof(token_status_frame_t)) {
        token_status_frame_t *frame = (token_status_frame_t *)data;
        uint8_t crc = espnow_crc8(data, len - 1);
        if (crc == frame->crc8) {
            memcpy(&token_data, frame, sizeof(token_data));
            token_data_valid = true;
            if (current_screen == 1) screen_dirty = true;
            Serial.printf("Token data received: %d items\n", frame->count);
        }
    }

    // Project 状态帧
    else if (frame_type == FRAME_TYPE_PROJECT_STATUS && len == sizeof(project_status_frame_t)) {
        project_status_frame_t *frame = (project_status_frame_t *)data;
        uint8_t crc = espnow_crc8(data, len - 1);
        if (crc == frame->crc8) {
            memcpy(&project_data, frame, sizeof(project_data));
            project_data_valid = true;
            if (current_screen == 2) screen_dirty = true;
            Serial.printf("Project data received: %d items\n", frame->count);
        }
    }

    // AI 情绪状态帧 (驱动云朵表情, 覆盖本地自动循环)
    else if (frame_type == FRAME_TYPE_AI_STATE && len == sizeof(ai_state_frame_t)) {
        ai_state_frame_t *frame = (ai_state_frame_t *)data;
        uint8_t crc = espnow_crc8(data, len - 1);
        if (crc == frame->crc8 && frame->emotion < AI_STATE_COUNT) {
            ai_state = (ai_state_t)frame->emotion;
            // 自定义文字 (确保以 \0 结尾)
            memcpy(ai_custom_text, frame->text, sizeof(ai_custom_text) - 1);
            ai_custom_text[sizeof(ai_custom_text) - 1] = '\0';
            ai_external_override = true;
            ai_last_ext_frame = millis();
            ai_state_changed = true;  // 由 update_screen_ai_status 应用
            Serial.printf("AI state received: emotion=%d text='%s'\n", frame->emotion, ai_custom_text);
        }
    }

    // 决策请求帧 (PC→Dongle→K10)
    else if (frame_type == FRAME_TYPE_DECISION_REQ && len == sizeof(decision_request_frame_t)) {
        const decision_request_frame_t *req = (const decision_request_frame_t *)data;
        if (espnow_crc8(data, sizeof(*req) - 1) == req->crc8) {
            memcpy(&g_decision_req, req, sizeof(g_decision_req));
            g_decision_pending = true;   // 主循环据此切到决策界面
            Serial.printf("Decision req id=%u kind=%u opts=%u\n",
                          req->decision_id, req->kind, req->opt_count);
        }
    }
}

// ======================= I2C / ES7243E =======================
static void i2c_scan(int sda, int scl) {
    Wire.begin(sda, scl);
    Serial.printf("\n=== I2C Scan on SDA=%d SCL=%d ===\n", sda, scl);
    int count = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("  Found device at 0x%02X\n", addr);
            count++;
        }
    }
    Serial.printf("Total: %d device(s)\n", count);
}

// ======================= I2S / ESP-NOW 初始化 =======================
// 注: ES7243E 初始化序列已删除 — 新板用 ES8311, 由 AtkBoxBoard.h 的 init_board() 完成
static void mic_i2s_init() {
    i2s_config_t cfg = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 3,
        .dma_buf_len = 300,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0,
        .mclk_multiple = I2S_MCLK_MULTIPLE_DEFAULT,
        .bits_per_chan = I2S_BITS_PER_CHAN_16BIT,
    };
    i2s_pin_config_t pins = {
        .mck_io_num   = MIC_MCLK,
        .bck_io_num   = MIC_BCLK,
        .ws_io_num    = MIC_WS,
        .data_out_num = MIC_DOUT,
        .data_in_num  = MIC_DIN,
    };
    i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pins);
    i2s_zero_dma_buffer(I2S_NUM_0);
}

static bool espnow_init() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
    Serial.printf("WiFi STA mode, Channel: %d\n", ESPNOW_CHANNEL);
    Serial.printf("K10 MAC: %s\n", WiFi.macAddress().c_str());

    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW init FAILED");
        return false;
    }
    Serial.println("ESP-NOW init OK");

    esp_now_register_send_cb(espnow_send_cb);
    esp_now_register_recv_cb(espnow_recv_cb);

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, dongle_mac, 6);
    peerInfo.channel = ESPNOW_CHANNEL;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Add peer FAILED");
        return false;
    }
    Serial.printf("Peer added: %02X:%02X:%02X:%02X:%02X:%02X\n",
        dongle_mac[0], dongle_mac[1], dongle_mac[2],
        dongle_mac[3], dongle_mac[4], dongle_mac[5]);
    return true;
}

// ======================= LVGL 界面创建 =======================
// 界面 0: ESP-NOW 连接状态
static lv_obj_t *label_title_0, *label_espnow, *label_channel, *label_peer;
static lv_obj_t *label_streaming, *label_tx, *label_fail, *label_hint;

static void create_screen_status() {
    screen_status = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_status, lv_color_hex(0x1F2421), 0);

    label_title_0 = lv_label_create(screen_status);
    lv_label_set_text(label_title_0, "CodeBuddy P1");
    lv_obj_set_style_text_font(label_title_0, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(label_title_0, lv_color_hex(0xF7F4EF), 0);
    lv_obj_align(label_title_0, LV_ALIGN_TOP_LEFT, 10, 10);

    label_espnow = lv_label_create(screen_status);
    lv_obj_set_style_text_color(label_espnow, lv_color_hex(0x00FF00), 0);
    lv_obj_align(label_espnow, LV_ALIGN_TOP_LEFT, 10, 45);

    label_channel = lv_label_create(screen_status);
    lv_obj_set_style_text_color(label_channel, lv_color_hex(0xF7F4EF), 0);
    lv_obj_align(label_channel, LV_ALIGN_TOP_LEFT, 10, 65);

    label_peer = lv_label_create(screen_status);
    lv_obj_set_style_text_color(label_peer, lv_color_hex(0xF7F4EF), 0);
    lv_obj_align(label_peer, LV_ALIGN_TOP_LEFT, 10, 85);

    label_streaming = lv_label_create(screen_status);
    lv_obj_set_style_text_font(label_streaming, &lv_font_montserrat_28, 0);
    lv_obj_align(label_streaming, LV_ALIGN_CENTER, 0, -20);

    label_tx = lv_label_create(screen_status);
    lv_obj_set_style_text_font(label_tx, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(label_tx, lv_color_hex(0xF7F4EF), 0);
    lv_obj_align(label_tx, LV_ALIGN_CENTER, 0, 35);

    label_fail = lv_label_create(screen_status);
    lv_obj_set_style_text_font(label_fail, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(label_fail, lv_color_hex(0xFFAA00), 0);
    lv_obj_align(label_fail, LV_ALIGN_CENTER, 0, 65);

    label_hint = lv_label_create(screen_status);
    lv_label_set_text(label_hint, "A:F2/Enter  B:switch");
    lv_obj_set_style_text_color(label_hint, lv_color_hex(0x5C635D), 0);
    lv_obj_align(label_hint, LV_ALIGN_BOTTOM_LEFT, 10, -10);
}

static void update_screen_status() {
    if (espnow_ready) {
        lv_label_set_text(label_espnow, "ESP-NOW OK");
        lv_obj_set_style_text_color(label_espnow, lv_color_hex(0x00FF00), 0);
    } else {
        lv_label_set_text(label_espnow, "ESP-NOW FAILED");
        lv_obj_set_style_text_color(label_espnow, lv_color_hex(0xFF0000), 0);
    }

    lv_label_set_text_fmt(label_channel, "CH:%d", ESPNOW_CHANNEL);
    lv_label_set_text_fmt(label_peer, "->%02X:%02X:%02X:%02X:%02X:%02X",
        dongle_mac[0], dongle_mac[1], dongle_mac[2],
        dongle_mac[3], dongle_mac[4], dongle_mac[5]);

    if (streaming) {
        lv_label_set_text(label_streaming, "REC *");
        lv_obj_set_style_text_color(label_streaming, lv_color_hex(0xFF0000), 0);
    } else {
        lv_label_set_text(label_streaming, "IDLE");
        lv_obj_set_style_text_color(label_streaming, lv_color_hex(0x00FFFF), 0);
    }

    lv_label_set_text_fmt(label_tx, "TX:%u", packetCount);
    lv_label_set_text_fmt(label_fail, "FAIL:%u", sendFailCount);
}

// 界面 1: Token Usage
// 每个 Token 项包含: 名称标签 + 百分比标签 + 进度条 + 数量标签
typedef struct {
    lv_obj_t *name_label;
    lv_obj_t *pct_label;
    lv_obj_t *bar;
    lv_obj_t *amount_label;
} token_row_t;

static lv_obj_t *label_title_1;
static token_row_t token_rows[TOKEN_MAX_ITEMS];

// 点击服务名 -> 弹用量详情卡片
static void token_row_event(lv_event_t *e) {
    uint8_t i = (uint8_t)(intptr_t)lv_event_get_user_data(e);
    if (!token_data_valid || i >= token_data.count) return;
    uint16_t pct = token_data.items[i].percent_x10;
    static char body[96];
    snprintf(body, sizeof(body), "Usage: %u.%u%%\nUsed: %lu\nTotal: %lu",
             pct / 10, pct % 10,
             (unsigned long)token_data.items[i].used,
             (unsigned long)token_data.items[i].total);
    show_detail_card(token_data.items[i].name, body);
}

static void create_screen_token() {
    screen_token = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_token, lv_color_hex(0x0D0D0D), 0);
    lv_obj_set_style_pad_all(screen_token, 0, 0);
    lv_obj_clear_flag(screen_token, LV_OBJ_FLAG_SCROLLABLE);

    // 标题
    label_title_1 = lv_label_create(screen_token);
    lv_label_set_text(label_title_1, "Token Usage");
    lv_obj_set_style_text_font(label_title_1, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(label_title_1, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(label_title_1, LV_ALIGN_TOP_LEFT, 12, 12);

    // 创建 5 行 Token 项 (每行高度约 58px, 从 y=50 开始)
    const int row_start_y = 50;
    const int row_height = 54;
    for (int i = 0; i < TOKEN_MAX_ITEMS; i++) {
        int y = row_start_y + i * row_height;

        // 服务名称 (左侧)
        token_rows[i].name_label = lv_label_create(screen_token);
        lv_obj_set_style_text_font(token_rows[i].name_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(token_rows[i].name_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(token_rows[i].name_label, LV_ALIGN_TOP_LEFT, 12, y);

        // 百分比标签 (右侧, 带背景色块)
        token_rows[i].pct_label = lv_label_create(screen_token);
        lv_obj_set_style_text_font(token_rows[i].pct_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(token_rows[i].pct_label, lv_color_hex(0x00FF88), 0);
        lv_obj_set_style_bg_color(token_rows[i].pct_label, lv_color_hex(0x0A3D2E), 0);
        lv_obj_set_style_bg_opa(token_rows[i].pct_label, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_hor(token_rows[i].pct_label, 6, 0);
        lv_obj_set_style_pad_ver(token_rows[i].pct_label, 2, 0);
        lv_obj_set_style_radius(token_rows[i].pct_label, 4, 0);
        lv_obj_align(token_rows[i].pct_label, LV_ALIGN_TOP_RIGHT, -12, y);

        // 进度条
        token_rows[i].bar = lv_bar_create(screen_token);
        lv_obj_set_size(token_rows[i].bar, 216, 8);
        lv_obj_align(token_rows[i].bar, LV_ALIGN_TOP_LEFT, 12, y + 22);
        lv_obj_set_style_bg_color(token_rows[i].bar, lv_color_hex(0x2A2A2A), LV_PART_MAIN);
        lv_obj_set_style_bg_color(token_rows[i].bar, lv_color_hex(0x00FF88), LV_PART_INDICATOR);
        lv_obj_set_style_radius(token_rows[i].bar, 4, LV_PART_MAIN);
        lv_obj_set_style_radius(token_rows[i].bar, 4, LV_PART_INDICATOR);
        lv_bar_set_range(token_rows[i].bar, 0, 1000);

        // 数量标签 (已用/总量)
        token_rows[i].amount_label = lv_label_create(screen_token);
        lv_obj_set_style_text_font(token_rows[i].amount_label, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(token_rows[i].amount_label, lv_color_hex(0x888888), 0);
        lv_obj_align(token_rows[i].amount_label, LV_ALIGN_TOP_LEFT, 12, y + 34);

        // 点击服务名 -> 弹用量详情卡片
        lv_obj_add_flag(token_rows[i].name_label, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(token_rows[i].name_label, token_row_event,
                            LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }
}

// 格式化大数字 (加千位逗号): 847200 -> "847,200"
static void format_number(uint32_t num, char *buf, size_t buf_size) {
    char tmp[16];
    snprintf(tmp, sizeof(tmp), "%u", num);
    int len = strlen(tmp);
    int commas = (len - 1) / 3;
    int out_len = len + commas;
    if (out_len >= (int)buf_size) { snprintf(buf, buf_size, "%u", num); return; }

    buf[out_len] = '\0';
    int j = out_len - 1;
    int cnt = 0;
    for (int i = len - 1; i >= 0; i--) {
        buf[j--] = tmp[i];
        if (++cnt % 3 == 0 && i > 0) buf[j--] = ',';
    }
}

// 刷新 Token 界面的高亮 (仅演示模式)
static void update_token_highlight() {
    if (!demo_mode_active) return;
    uint8_t count = token_data_valid ? token_data.count : 0;
    if (count > TOKEN_MAX_ITEMS) count = TOKEN_MAX_ITEMS;
    for (uint8_t i = 0; i < count; i++) {
        uint32_t color = (i == token_selected) ? 0xFBBF24 : 0xFFFFFF;
        lv_obj_set_style_text_color(token_rows[i].name_label, lv_color_hex(color), 0);
    }
}

// 重置指定 Token 模型的用量 (演示: 模拟清空配额)
static void reset_token_item(uint8_t idx) {
    if (!token_data_valid || idx >= token_data.count) return;
    token_data.items[idx].used = 0;
    token_data.items[idx].percent_x10 = 0;
    Serial.printf("Token: reset %s\n", token_data.items[idx].name);
    update_screen_token();  // 重绘进度条/百分比/数量
}

static void update_screen_token() {
    uint8_t count = token_data_valid ? token_data.count : 0;
    if (count > TOKEN_MAX_ITEMS) count = TOKEN_MAX_ITEMS;

    for (int i = 0; i < TOKEN_MAX_ITEMS; i++) {
        if (i < count) {
            token_item_t *item = &token_data.items[i];
            lv_obj_clear_flag(token_rows[i].name_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(token_rows[i].pct_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(token_rows[i].bar, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(token_rows[i].amount_label, LV_OBJ_FLAG_HIDDEN);

            // 名称
            lv_label_set_text(token_rows[i].name_label, item->name);

            // 百分比 (percent_x10: 847 -> "84.7%")
            uint16_t pct = item->percent_x10;
            lv_label_set_text_fmt(token_rows[i].pct_label, "%u.%u%%", pct / 10, pct % 10);

            // 根据百分比高低设置颜色 (>=80% 红色警告, 否则绿色)
            uint32_t bar_color, txt_color, bg_color;
            if (pct >= 800) {
                bar_color = 0xFF4444; txt_color = 0xFF6666; bg_color = 0x3D0A0A;
            } else {
                bar_color = 0x00FF88; txt_color = 0x00FF88; bg_color = 0x0A3D2E;
            }
            lv_obj_set_style_text_color(token_rows[i].pct_label, lv_color_hex(txt_color), 0);
            lv_obj_set_style_bg_color(token_rows[i].pct_label, lv_color_hex(bg_color), 0);
            lv_obj_set_style_bg_color(token_rows[i].bar, lv_color_hex(bar_color), LV_PART_INDICATOR);

            // 进度条 (0~1000)
            lv_bar_set_value(token_rows[i].bar, pct > 1000 ? 1000 : pct, LV_ANIM_OFF);

            // 数量 (已用/总量, 带千位逗号)
            char used_str[16], total_str[16];
            format_number(item->used, used_str, sizeof(used_str));
            format_number(item->total, total_str, sizeof(total_str));
            lv_label_set_text_fmt(token_rows[i].amount_label, "%s / %s", used_str, total_str);
        } else {
            lv_obj_add_flag(token_rows[i].name_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(token_rows[i].pct_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(token_rows[i].bar, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(token_rows[i].amount_label, LV_OBJ_FLAG_HIDDEN);
        }
    }

    // 无数据时显示提示
    if (count == 0) {
        lv_label_set_text(token_rows[0].name_label, "Waiting for data...");
        lv_obj_clear_flag(token_rows[0].name_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_color(token_rows[0].name_label, lv_color_hex(0x888888), 0);
    }

    // 演示模式下应用高亮
    update_token_highlight();
}

// 界面 2: Coding Status (项目列表)
// 每个项目: 彩色圆点 + 项目名 + 状态文字
typedef struct {
    lv_obj_t *dot;
    lv_obj_t *name_label;
    lv_obj_t *status_label;
} project_row_t;

static lv_obj_t *label_title_2;
static project_row_t project_rows[PROJECT_MAX_ITEMS];

// 界面 2 选择题交互区 (演示 Claude 需要用户选择的场景)
static lv_obj_t *choice_container = NULL;    // 选择题容器
static lv_obj_t *choice_question = NULL;     // 问题文本
static lv_obj_t *choice_option_a = NULL;     // 选项 A
static lv_obj_t *choice_option_b = NULL;     // 选项 B
static uint8_t   choice_selected = 0;        // 当前选中 0=A, 1=B
static bool      choice_confirmed = false;   // 是否已确认

// 状态码 -> 文字 (供触摸回调复用; update_screen_project 内另有带颜色的完整表)
static const char *project_status_text(uint8_t code) {
    static const char *names[] = {"Planning", "Coding", "Review Needed",
                                  "Completed", "Error", "Idle"};
    return (code < 6) ? names[code] : "Unknown";
}

// 点击项目名 -> 弹详情卡片
static void project_row_event(lv_event_t *e) {
    uint8_t i = (uint8_t)(intptr_t)lv_event_get_user_data(e);
    if (!project_data_valid || i >= project_data.count) return;
    static char body[96];
    snprintf(body, sizeof(body), "Status: %s\nProject #%d of %d",
             project_status_text(project_data.items[i].status_code),
             i + 1, project_data.count);
    show_detail_card(project_data.items[i].name, body);
}

// 点击圆点 -> 循环切状态: Planning->Coding->Review->Completed->Idle->Planning
static void project_dot_event(lv_event_t *e) {
    uint8_t i = (uint8_t)(intptr_t)lv_event_get_user_data(e);
    if (!project_data_valid || i >= project_data.count) return;
    uint8_t c = project_data.items[i].status_code;
    switch (c) {
        case 0: c = 1; break;
        case 1: c = 2; break;
        case 2: c = 3; break;
        case 3: c = 5; break;
        default: c = 0; break;
    }
    project_data.items[i].status_code = c;
    Serial.printf("Project %d status -> %d\n", i, c);
    screen_dirty = true;   // 主循环 update_screen_project() 会重绘圆点与文字
}

static void create_screen_project() {
    screen_project = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_project, lv_color_hex(0x0D0D0D), 0);
    lv_obj_set_style_pad_all(screen_project, 0, 0);
    lv_obj_clear_flag(screen_project, LV_OBJ_FLAG_SCROLLABLE);

    // 标题
    label_title_2 = lv_label_create(screen_project);
    lv_label_set_text(label_title_2, "Coding Status");
    lv_obj_set_style_text_font(label_title_2, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(label_title_2, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(label_title_2, LV_ALIGN_TOP_LEFT, 12, 12);

    // 创建 6 行项目条目 (每行高度约 42px, 从 y=50 开始)
    const int row_start_y = 50;
    const int row_height = 42;
    for (int i = 0; i < PROJECT_MAX_ITEMS; i++) {
        int y = row_start_y + i * row_height;

        // 状态圆点 (左侧, 直径10px)
        project_rows[i].dot = lv_obj_create(screen_project);
        lv_obj_set_size(project_rows[i].dot, 10, 10);
        lv_obj_align(project_rows[i].dot, LV_ALIGN_TOP_LEFT, 12, y + 5);
        lv_obj_set_style_radius(project_rows[i].dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(project_rows[i].dot, 0, 0);
        lv_obj_clear_flag(project_rows[i].dot, LV_OBJ_FLAG_SCROLLABLE);

        // 项目名称 (左侧, 圆点右边)
        project_rows[i].name_label = lv_label_create(screen_project);
        lv_obj_set_style_text_font(project_rows[i].name_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(project_rows[i].name_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(project_rows[i].name_label, LV_ALIGN_TOP_LEFT, 28, y);
        lv_label_set_long_mode(project_rows[i].name_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_width(project_rows[i].name_label, 140);

        // 状态文字 (右侧)
        project_rows[i].status_label = lv_label_create(screen_project);
        lv_obj_set_style_text_font(project_rows[i].status_label, &lv_font_montserrat_12, 0);
        lv_obj_align(project_rows[i].status_label, LV_ALIGN_TOP_LEFT, 28, y + 18);

        // 圆点命中区放大 (视觉仍 10px)，点击切状态
        lv_obj_set_ext_click_area(project_rows[i].dot, 15);
        lv_obj_add_flag(project_rows[i].dot, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(project_rows[i].dot, project_dot_event,
                            LV_EVENT_CLICKED, (void*)(intptr_t)i);
        // 点击项目名弹详情
        lv_obj_add_flag(project_rows[i].name_label, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(project_rows[i].name_label, project_row_event,
                            LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }

    // 选择题交互区 (底部, 演示模式下显示 Claude 提问)
    choice_container = lv_obj_create(screen_project);
    lv_obj_set_size(choice_container, 216, 90);
    lv_obj_align(choice_container, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_set_style_bg_color(choice_container, lv_color_hex(0x1F2937), 0);
    lv_obj_set_style_border_color(choice_container, lv_color_hex(0x374151), 0);
    lv_obj_set_style_border_width(choice_container, 1, 0);
    lv_obj_set_style_radius(choice_container, 8, 0);
    lv_obj_set_style_pad_all(choice_container, 8, 0);
    lv_obj_clear_flag(choice_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(choice_container, LV_OBJ_FLAG_HIDDEN);  // 默认隐藏,演示模式显示

    // 问题文本
    choice_question = lv_label_create(choice_container);
    lv_label_set_text(choice_question, "Choose database:");
    lv_obj_set_style_text_font(choice_question, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(choice_question, lv_color_hex(0xF3F4F6), 0);
    lv_obj_align(choice_question, LV_ALIGN_TOP_LEFT, 0, 0);

    // 选项 A
    choice_option_a = lv_label_create(choice_container);
    lv_label_set_text(choice_option_a, "A: PostgreSQL");
    lv_obj_set_style_text_font(choice_option_a, &lv_font_montserrat_14, 0);
    lv_obj_align(choice_option_a, LV_ALIGN_TOP_LEFT, 0, 28);

    // 选项 B
    choice_option_b = lv_label_create(choice_container);
    lv_label_set_text(choice_option_b, "B: MongoDB");
    lv_obj_set_style_text_font(choice_option_b, &lv_font_montserrat_14, 0);
    lv_obj_align(choice_option_b, LV_ALIGN_TOP_LEFT, 0, 52);
}

// 刷新选择题的高亮状态
static void update_choice_highlight() {
    if (!demo_mode_active || choice_option_a == NULL) return;

    // 选中的亮黄色，未选中的灰色
    lv_color_t color_selected = lv_color_hex(0xFBBF24);  // 亮黄
    lv_color_t color_normal = lv_color_hex(0x9CA3AF);    // 灰色

    if (choice_confirmed) {
        // 已确认：两个都变绿色，选中的加粗
        lv_color_t color_confirmed = lv_color_hex(0x10B981);
        lv_obj_set_style_text_color(choice_option_a, color_confirmed, 0);
        lv_obj_set_style_text_color(choice_option_b, color_confirmed, 0);
    } else {
        // 未确认：选中的高亮，未选中的灰色
        lv_obj_set_style_text_color(choice_option_a,
            (choice_selected == 0) ? color_selected : color_normal, 0);
        lv_obj_set_style_text_color(choice_option_b,
            (choice_selected == 1) ? color_selected : color_normal, 0);
    }
}

static void update_screen_project() {
    uint8_t count = project_data_valid ? project_data.count : 0;
    if (count > PROJECT_MAX_ITEMS) count = PROJECT_MAX_ITEMS;

    // 状态码 -> 文字 + 颜色映射
    const struct {
        const char *text;
        uint32_t dot_color;
        uint32_t text_color;
    } status_map[] = {
        {"Planning",       0x3B82F6, 0x60A5FA}, // 蓝色
        {"Coding",         0x8B5CF6, 0xA78BFA}, // 紫色
        {"Review Needed",  0xF97316, 0xFB923C}, // 橙色
        {"Completed",      0x10B981, 0x34D399}, // 绿色
        {"Error",          0xEF4444, 0xF87171}, // 红色
        {"Idle",           0x6B7280, 0x9CA3AF}, // 灰色
    };

    for (int i = 0; i < PROJECT_MAX_ITEMS; i++) {
        if (i < count) {
            project_item_t *item = &project_data.items[i];
            lv_obj_clear_flag(project_rows[i].dot, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(project_rows[i].name_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(project_rows[i].status_label, LV_OBJ_FLAG_HIDDEN);

            // 项目名称
            lv_label_set_text(project_rows[i].name_label, item->name);

            // 状态文字 + 颜色
            uint8_t code = item->status_code;
            if (code < sizeof(status_map) / sizeof(status_map[0])) {
                lv_obj_set_style_bg_color(project_rows[i].dot, lv_color_hex(status_map[code].dot_color), 0);
                lv_label_set_text(project_rows[i].status_label, status_map[code].text);
                lv_obj_set_style_text_color(project_rows[i].status_label, lv_color_hex(status_map[code].text_color), 0);
            } else {
                lv_obj_set_style_bg_color(project_rows[i].dot, lv_color_hex(0x6B7280), 0);
                lv_label_set_text(project_rows[i].status_label, "Unknown");
                lv_obj_set_style_text_color(project_rows[i].status_label, lv_color_hex(0x9CA3AF), 0);
            }
        } else {
            lv_obj_add_flag(project_rows[i].dot, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(project_rows[i].name_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(project_rows[i].status_label, LV_OBJ_FLAG_HIDDEN);
        }
    }

    // 无数据时显示提示
    if (count == 0) {
        lv_label_set_text(project_rows[0].name_label, "Waiting for data...");
        lv_obj_clear_flag(project_rows[0].name_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_color(project_rows[0].name_label, lv_color_hex(0x888888), 0);
        lv_obj_add_flag(project_rows[0].dot, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(project_rows[0].status_label, LV_OBJ_FLAG_HIDDEN);
    }

    // 刷新选择题高亮
    update_choice_highlight();
}

// ==================== 界面 3: Product Inspo ====================
static lv_obj_t *label_inspo_title;
static lv_obj_t *label_inspo_date;
static lv_obj_t *label_inspo_content;

static void create_screen_inspo() {
    screen_inspo = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_inspo, lv_color_hex(0x0D0D0D), 0);
    lv_obj_set_style_pad_all(screen_inspo, 0, 0);
    lv_obj_clear_flag(screen_inspo, LV_OBJ_FLAG_SCROLLABLE);

    // 标题
    label_inspo_title = lv_label_create(screen_inspo);
    lv_label_set_text(label_inspo_title, "Product Inspo");
    lv_obj_set_style_text_font(label_inspo_title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(label_inspo_title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(label_inspo_title, LV_ALIGN_TOP_LEFT, 12, 12);

    // 日期
    label_inspo_date = lv_label_create(screen_inspo);
    lv_obj_set_style_text_font(label_inspo_date, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label_inspo_date, lv_color_hex(0x888888), 0);
    lv_obj_align(label_inspo_date, LV_ALIGN_TOP_LEFT, 12, 45);

    // 内容文本
    label_inspo_content = lv_label_create(screen_inspo);
    lv_label_set_long_mode(label_inspo_content, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label_inspo_content, 216);
    lv_obj_set_style_text_font(label_inspo_content, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_inspo_content, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_text_line_space(label_inspo_content, 4, 0);
    lv_obj_align(label_inspo_content, LV_ALIGN_TOP_LEFT, 12, 70);

    // 触摸: 单击标题重启打字机, 单击正文暂停/继续
    lv_obj_add_flag(label_inspo_title, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(label_inspo_title, inspo_title_event, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(label_inspo_content, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(label_inspo_content, inspo_body_event, LV_EVENT_CLICKED, NULL);
}

// 单击标题 -> 重启打字机动画
static void inspo_title_event(lv_event_t *e) {
    (void)e;
    start_typing_animation();
    Serial.println("Inspo: typing restarted (tap title)");
}

// 单击正文 -> 暂停/继续打字机
static void inspo_body_event(lv_event_t *e) {
    (void)e;
    inspo_typing_active = !inspo_typing_active;
    Serial.printf("Inspo: typing %s (tap body)\n", inspo_typing_active ? "resumed" : "paused");
}

// 启动打字动画
static void start_typing_animation() {
    if (!inspo_full_text) return;
    inspo_typing_active = true;
    inspo_typing_index = 0;
    inspo_cursor_visible = true;
    inspo_last_type_ms = millis();
    inspo_last_cursor_ms = millis();
    lv_label_set_text(label_inspo_content, "");
    Serial.println("Typing animation started");
}

// 推进打字动画（每次调用: 按间隔追加字符 + 光标闪烁）
// buf 512 字节容纳全文(~280)+光标+结束符
static void advance_typing() {
    if (!inspo_full_text) return;
    uint32_t now = millis();
    uint16_t full_len = strlen(inspo_full_text);

    // 推进字符
    if (inspo_typing_active && (now - inspo_last_type_ms >= TYPING_INTERVAL_MS)) {
        inspo_last_type_ms = now;
        if (inspo_typing_index < full_len) {
            inspo_typing_index++;
        } else {
            inspo_typing_active = false;  // 打完, 光标继续闪
            Serial.println("Typing animation complete");
        }
    }

    // 光标闪烁
    if (now - inspo_last_cursor_ms >= CURSOR_BLINK_MS) {
        inspo_last_cursor_ms = now;
        inspo_cursor_visible = !inspo_cursor_visible;
    }

    // 组装显示文本 = 已打字符 + 光标
    static char buf[512];
    uint16_t n = inspo_typing_index;
    if (n > full_len) n = full_len;
    if (n > sizeof(buf) - 2) n = sizeof(buf) - 2;
    memcpy(buf, inspo_full_text, n);
    buf[n] = inspo_cursor_visible ? '_' : ' ';
    buf[n + 1] = '\0';
    lv_label_set_text(label_inspo_content, buf);
}

static void update_screen_inspo() {
    // 演示数据将在 inject_demo_data() 中填充
}

// ==================== 界面 4: User Profile ====================
static lv_obj_t *profile_image;     // SD 卡加载的图片
static lv_obj_t *profile_gradient;  // 降级方案：渐变色块
static lv_obj_t *label_profile_name;

// 扫描 SD 卡根目录 /user1.png ~ /user10.png, 存入 profile_paths (LVGL 格式)
static void scan_profile_images() {
    profile_count = 0;
    if (!sd_card_ready) {
        Serial.println("Profile: SD not ready, no scan");
        return;
    }
    for (int i = 1; i <= 10 && profile_count < MAX_PROFILE_IMAGES; i++) {
        char sd_path[16];
        snprintf(sd_path, sizeof(sd_path), "/user%d.png", i);
        if (SD.exists(sd_path)) {
            snprintf(profile_paths[profile_count], 16, "D:/user%d.png", i);
            profile_count++;
        }
    }
    Serial.printf("Profile: found %d images\n", profile_count);
}

static void create_screen_profile() {
    screen_profile = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_profile, lv_color_hex(0x0D0D0D), 0);
    lv_obj_set_style_pad_all(screen_profile, 0, 0);
    lv_obj_clear_flag(screen_profile, LV_OBJ_FLAG_SCROLLABLE);

    // 尝试从 SD 卡加载图片
    if (sd_card_ready) {
        profile_image = lv_img_create(screen_profile);
        lv_img_set_src(profile_image, profile_image_path);
        lv_obj_align(profile_image, LV_ALIGN_CENTER, 0, -20);
        lv_obj_add_flag(profile_image, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(profile_image, profile_img_event, LV_EVENT_CLICKED, NULL);
        Serial.printf("Profile: Loading image from %s\n", profile_image_path);
    } else {
        // 降级方案：渐变色块 (代替照片, 200x240 居中)
        profile_gradient = lv_obj_create(screen_profile);
        lv_obj_set_size(profile_gradient, 200, 240);
        lv_obj_set_style_bg_color(profile_gradient, lv_color_hex(0x3B82F6), 0);
        lv_obj_set_style_bg_grad_color(profile_gradient, lv_color_hex(0x8B5CF6), 0);
        lv_obj_set_style_bg_grad_dir(profile_gradient, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_radius(profile_gradient, 12, 0);
        lv_obj_set_style_border_width(profile_gradient, 0, 0);
        lv_obj_align(profile_gradient, LV_ALIGN_CENTER, 0, -20);
        lv_obj_add_flag(profile_gradient, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(profile_gradient, profile_img_event, LV_EVENT_CLICKED, NULL);
        Serial.println("Profile: Using gradient fallback (SD card not ready)");
    }

    // 用户名 (叠加在图片/渐变块底部)
    label_profile_name = lv_label_create(screen_profile);
    lv_obj_set_style_text_font(label_profile_name, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(label_profile_name, lv_color_hex(0xFFFF00), 0);
    lv_obj_align(label_profile_name, LV_ALIGN_CENTER, 0, 80);
}

// 单击头像 -> 切换下一张 (回调在 lv_task_handler 持锁上下文, 可直接调 update)
static void profile_img_event(lv_event_t *e) {
    (void)e;
    if (profile_count > 1) {
        profile_index = (profile_index + 1) % profile_count;
        Serial.printf("Profile: switched to user %d (tap)\n", profile_index + 1);
        update_screen_profile();
    }
}

static void update_screen_profile() {
    if (profile_count > 0 && profile_image != NULL) {
        // 有扫描到的头像 → 加载当前索引
        // 仅在图片索引变化时才重设源 (lv_img_set_src 会触发 PNG 从 SD 重解码, 每次切屏都调会卡顿)
        static int8_t loaded_profile_index = -1;
        if (loaded_profile_index != (int8_t)profile_index) {
            loaded_profile_index = (int8_t)profile_index;
            lv_img_set_src(profile_image, profile_paths[profile_index]);
        }
        lv_label_set_text_fmt(label_profile_name, "User %d", profile_index + 1);
    } else {
        // 演示数据将在 inject_demo_data() 中填充 (演示模式备用)
    }
}

// ==================== 界面 5: AI Status ====================
static lv_obj_t *ai_status_image = NULL;   // SD 卡加载的图片
static lv_obj_t *ai_thinking_arc = NULL;   // 降级方案：旋转弧形动画
static lv_obj_t *label_ai_status_text = NULL;

// 表情动画叠加层 (云朵脸部)
static lv_obj_t *face_brow_left  = NULL;   // 左眉
static lv_obj_t *face_brow_right = NULL;   // 右眉
static lv_obj_t *face_eye_left  = NULL;    // 左眼
static lv_obj_t *face_eye_right = NULL;    // 右眼
static lv_obj_t *face_mouth     = NULL;    // 嘴巴

// 表情五官位置 —— 依据 ai-1.png 实测像素坐标换算
// 图片 240x320, 以 LV_ALIGN_CENTER 上移 30px 显示 (offset_y = py - 160 - 30)
// 眉(y~118) 左眼(100,136) 右眼(131,136) 眼高~16; 嘴中心(114,158)
#define FACE_BROW_Y       (-72)  // 眉毛 Y 偏移 (118-160-30)
#define FACE_BROW_LEFT_X  (-20)  // 左眉 X (跟左眼对齐)
#define FACE_BROW_RIGHT_X (11)   // 右眉 X (跟右眼对齐)
#define FACE_BROW_W       (16)   // 眉毛宽
#define FACE_BROW_H       (4)    // 眉毛粗细
#define FACE_EYE_LEFT_X   (-20)  // 左眼 X 偏移 (100-120)
#define FACE_EYE_RIGHT_X  (11)   // 右眼 X 偏移 (131-120)
#define FACE_EYE_Y        (-54)  // 眼睛 Y 偏移 (136-160-30)
#define FACE_EYE_W        (16)   // 眼睛宽
#define FACE_EYE_H        (16)   // 眼睛正常高度 (眨眼时压扁)
#define FACE_MOUTH_X      (-6)   // 嘴巴 X 偏移 (114-120)
#define FACE_MOUTH_Y      (-32)  // 嘴巴 Y 偏移 (158-160-30)
#define FACE_MOUTH_W      (18)   // 嘴巴宽
#define FACE_COLOR        0x333333  // 五官颜色 (深灰, 接近参考图)

static void create_screen_ai_status() {
    screen_ai_status = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_ai_status, lv_color_hex(0x0D0D0D), 0);
    lv_obj_set_style_pad_all(screen_ai_status, 0, 0);
    lv_obj_clear_flag(screen_ai_status, LV_OBJ_FLAG_SCROLLABLE);

    // 尝试从 SD 卡加载图片
    if (sd_card_ready) {
        ai_status_image = lv_img_create(screen_ai_status);
        lv_img_set_src(ai_status_image, ai_status_image_path);
        lv_obj_align(ai_status_image, LV_ALIGN_CENTER, 0, -30);
        Serial.printf("AI Status: Loading image from %s\n", ai_status_image_path);
    } else {
        // 降级方案：思考中的圆弧动画 (用 arc 控件模拟云朵思考效果)
        ai_thinking_arc = lv_arc_create(screen_ai_status);
        lv_obj_set_size(ai_thinking_arc, 140, 140);
        lv_arc_set_rotation(ai_thinking_arc, 0);
        lv_arc_set_bg_angles(ai_thinking_arc, 0, 360);
        lv_arc_set_angles(ai_thinking_arc, 0, 120);
        lv_obj_set_style_arc_width(ai_thinking_arc, 10, LV_PART_MAIN);
        lv_obj_set_style_arc_color(ai_thinking_arc, lv_color_hex(0x1F2937), LV_PART_MAIN);
        lv_obj_set_style_arc_width(ai_thinking_arc, 12, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(ai_thinking_arc, lv_color_hex(0x10B981), LV_PART_INDICATOR);
        lv_obj_remove_style(ai_thinking_arc, NULL, LV_PART_KNOB);
        lv_obj_align(ai_thinking_arc, LV_ALIGN_CENTER, 0, -30);
        Serial.println("AI Status: Using arc animation fallback (SD card not ready)");
    }

    // 状态文字
    label_ai_status_text = lv_label_create(screen_ai_status);
    lv_obj_set_style_text_font(label_ai_status_text, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(label_ai_status_text, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(label_ai_status_text, "Thinking...");  // 初始文字, 后续由状态机管理
    lv_obj_align(label_ai_status_text, LV_ALIGN_CENTER, 0, 80);

    // 表情动画叠加层: 眉毛 + 眼睛 + 嘴巴 (盖在无脸云朵上, 由 update 驱动眨眼/动嘴)
    // 仅在图片模式下叠加 (降级模式无云朵图, 不叠加五官)
    if (sd_card_ready) {
        // 左眉 (稍微挑起的弧形)
        face_brow_left = lv_obj_create(screen_ai_status);
        lv_obj_set_size(face_brow_left, FACE_BROW_W, FACE_BROW_H);
        lv_obj_set_style_radius(face_brow_left, 2, 0);
        lv_obj_set_style_bg_color(face_brow_left, lv_color_hex(FACE_COLOR), 0);
        lv_obj_set_style_border_width(face_brow_left, 0, 0);
        lv_obj_clear_flag(face_brow_left, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align(face_brow_left, LV_ALIGN_CENTER, FACE_BROW_LEFT_X, FACE_BROW_Y);

        // 右眉
        face_brow_right = lv_obj_create(screen_ai_status);
        lv_obj_set_size(face_brow_right, FACE_BROW_W, FACE_BROW_H);
        lv_obj_set_style_radius(face_brow_right, 2, 0);
        lv_obj_set_style_bg_color(face_brow_right, lv_color_hex(FACE_COLOR), 0);
        lv_obj_set_style_border_width(face_brow_right, 0, 0);
        lv_obj_clear_flag(face_brow_right, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align(face_brow_right, LV_ALIGN_CENTER, FACE_BROW_RIGHT_X, FACE_BROW_Y);

        // 左眼
        face_eye_left = lv_obj_create(screen_ai_status);
        lv_obj_set_size(face_eye_left, FACE_EYE_W, FACE_EYE_H);
        lv_obj_set_style_radius(face_eye_left, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(face_eye_left, lv_color_hex(FACE_COLOR), 0);
        lv_obj_set_style_border_width(face_eye_left, 0, 0);
        lv_obj_clear_flag(face_eye_left, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align(face_eye_left, LV_ALIGN_CENTER, FACE_EYE_LEFT_X, FACE_EYE_Y);

        // 右眼
        face_eye_right = lv_obj_create(screen_ai_status);
        lv_obj_set_size(face_eye_right, FACE_EYE_W, FACE_EYE_H);
        lv_obj_set_style_radius(face_eye_right, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(face_eye_right, lv_color_hex(FACE_COLOR), 0);
        lv_obj_set_style_border_width(face_eye_right, 0, 0);
        lv_obj_clear_flag(face_eye_right, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align(face_eye_right, LV_ALIGN_CENTER, FACE_EYE_RIGHT_X, FACE_EYE_Y);

        // 嘴巴 (小圆点, 通过改变高度模拟开合)
        face_mouth = lv_obj_create(screen_ai_status);
        lv_obj_set_size(face_mouth, FACE_MOUTH_W, 6);
        lv_obj_set_style_radius(face_mouth, 4, 0);
        lv_obj_set_style_bg_color(face_mouth, lv_color_hex(FACE_COLOR), 0);
        lv_obj_set_style_border_width(face_mouth, 0, 0);
        lv_obj_clear_flag(face_mouth, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align(face_mouth, LV_ALIGN_CENTER, FACE_MOUTH_X, FACE_MOUTH_Y);
    }

    // 触摸: 单击屏幕任意处切换 AI 情绪 (挂在屏幕上, 覆盖图片/五官叠层)
    lv_obj_add_flag(screen_ai_status, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(screen_ai_status, ai_face_event, LV_EVENT_CLICKED, NULL);
}

// 单击 -> 切换 AI 情绪 (Thinking->Coding->Done->循环), 暂停自动循环 15s
static void ai_face_event(lv_event_t *e) {
    (void)e;
    // 过滤滑动: 刚发生过左右滑动切屏时, 屏幕级 CLICKED 不当作点击
    if (millis() - last_swipe_ms < 400) return;
    ai_state = (ai_state_t)(((int)ai_state + 1) % AI_STATE_COUNT);
    ai_external_override = true;      // 暂停自动循环
    ai_last_ext_frame = millis();     // 15s 后恢复自动
    ai_state_changed = true;          // 触发表情应用
    Serial.printf("AI emotion -> %d (tap)\n", (int)ai_state);
}

// 应用某个 AI 情绪状态: 设置眉毛位置/嘴型/文字 (在状态切换时调用一次)
// 若 ai_external_override 且 ai_custom_text 非空, 文字用自定义, 否则用默认
static void apply_ai_state(ai_state_t st) {
    if (face_brow_left == NULL) return;

    bool use_custom = ai_external_override && ai_custom_text[0] != '\0';

    switch (st) {
        case AI_STATE_THINKING:
            // 挑眉: 眉毛抬高 (远离眼睛)
            lv_obj_align(face_brow_left,  LV_ALIGN_CENTER, FACE_BROW_LEFT_X,  FACE_BROW_Y - 4);
            lv_obj_align(face_brow_right, LV_ALIGN_CENTER, FACE_BROW_RIGHT_X, FACE_BROW_Y - 4);
            // 小嘴闭合
            lv_obj_set_size(face_mouth, 10, 6);
            lv_obj_set_style_radius(face_mouth, 3, 0);
            lv_obj_align(face_mouth, LV_ALIGN_CENTER, FACE_MOUTH_X, FACE_MOUTH_Y);
            lv_label_set_text(label_ai_status_text, use_custom ? ai_custom_text : "Thinking...");
            break;
        case AI_STATE_CODING:
            // 皱眉: 眉毛压低 (靠近眼睛), 内侧下压
            lv_obj_align(face_brow_left,  LV_ALIGN_CENTER, FACE_BROW_LEFT_X,  FACE_BROW_Y + 5);
            lv_obj_align(face_brow_right, LV_ALIGN_CENTER, FACE_BROW_RIGHT_X, FACE_BROW_Y + 5);
            // 嘴巴由动画驱动开合, 这里给个初始
            lv_obj_set_size(face_mouth, FACE_MOUTH_W, 6);
            lv_obj_set_style_radius(face_mouth, 4, 0);
            lv_obj_align(face_mouth, LV_ALIGN_CENTER, FACE_MOUTH_X, FACE_MOUTH_Y);
            lv_label_set_text(label_ai_status_text, use_custom ? ai_custom_text : "Coding...");
            break;
        case AI_STATE_DONE:
            // 平眉: 恢复正常位置
            lv_obj_align(face_brow_left,  LV_ALIGN_CENTER, FACE_BROW_LEFT_X,  FACE_BROW_Y);
            lv_obj_align(face_brow_right, LV_ALIGN_CENTER, FACE_BROW_RIGHT_X, FACE_BROW_Y);
            // 微笑: 宽扁大嘴, 大圆角
            lv_obj_set_size(face_mouth, 26, 14);
            lv_obj_set_style_radius(face_mouth, 7, 0);
            lv_obj_align(face_mouth, LV_ALIGN_CENTER, FACE_MOUTH_X, FACE_MOUTH_Y + 2);
            lv_label_set_text(label_ai_status_text, use_custom ? ai_custom_text : "Done!");
            break;
        default: break;
    }
}

static void update_screen_ai_status() {
    // 降级模式(无 SD 图片)时更新旋转圆弧动画
    if (ai_thinking_arc != NULL) {
        static int rotation = 0;
        rotation = (rotation + 10) % 360;
        lv_arc_set_rotation(ai_thinking_arc, rotation);
    }

    // 表情动画: 状态机 + 眨眼 + 动嘴 (仅在叠加层已创建时)
    if (face_eye_left == NULL) return;

    uint32_t now = millis();

    // --- 状态机: 外部覆盖优先, 否则本地自动循环 ---
    static uint32_t last_state_change = 0;
    static bool state_inited = false;
    if (!state_inited) {
        state_inited = true;
        last_state_change = now;
        apply_ai_state(ai_state);
    }

    if (ai_external_override) {
        // 上位机驱动模式: 收到新帧时应用, 超时无帧则恢复自动循环
        if (ai_state_changed) {
            ai_state_changed = false;
            apply_ai_state(ai_state);
        }
        if (now - ai_last_ext_frame >= AI_OVERRIDE_TIMEOUT_MS) {
            ai_external_override = false;
            ai_custom_text[0] = '\0';
            last_state_change = now;  // 平滑接回自动循环
        }
    } else {
        // 本地自动循环: 每 AI_STATE_DURATION_MS 切换情绪
        if (now - last_state_change >= AI_STATE_DURATION_MS) {
            last_state_change = now;
            ai_state = (ai_state_t)((ai_state + 1) % AI_STATE_COUNT);
            apply_ai_state(ai_state);
        }
    }

    // --- 眨眼: 每 ~3 秒眨一次, 闭眼持续 ~150ms ---
    static const uint32_t BLINK_PERIOD = 3000;
    uint32_t phase = now % BLINK_PERIOD;
    bool blinking = (phase >= (BLINK_PERIOD - 150));

    static bool last_blink = false;
    if (blinking != last_blink) {
        last_blink = blinking;
        if (blinking) {
            lv_obj_set_size(face_eye_left,  FACE_EYE_W, 3);
            lv_obj_set_size(face_eye_right, FACE_EYE_W, 3);
        } else {
            lv_obj_set_size(face_eye_left,  FACE_EYE_W, FACE_EYE_H);
            lv_obj_set_size(face_eye_right, FACE_EYE_W, FACE_EYE_H);
        }
        lv_obj_align(face_eye_left,  LV_ALIGN_CENTER, FACE_EYE_LEFT_X,  FACE_EYE_Y);
        lv_obj_align(face_eye_right, LV_ALIGN_CENTER, FACE_EYE_RIGHT_X, FACE_EYE_Y);
    }

    // --- 动嘴: 仅 Coding 状态下嘴巴周期性开合 (每 ~250ms 切换) ---
    if (ai_state == AI_STATE_CODING) {
        static uint32_t last_mouth = 0;
        static bool mouth_open = false;
        if (now - last_mouth >= 250) {
            last_mouth = now;
            mouth_open = !mouth_open;
            if (mouth_open) {
                lv_obj_set_size(face_mouth, FACE_MOUTH_W, 14);
                lv_obj_set_style_radius(face_mouth, 7, 0);
            } else {
                lv_obj_set_size(face_mouth, FACE_MOUTH_W, 6);
                lv_obj_set_style_radius(face_mouth, 4, 0);
            }
            lv_obj_align(face_mouth, LV_ALIGN_CENTER, FACE_MOUTH_X, FACE_MOUTH_Y);
        }
    }
}

// ========================== 界面 6: 触摸测试 ==========================
static lv_obj_t *touch_coord_label = NULL;
static lv_obj_t *touch_status_label = NULL;
static lv_obj_t *touch_dot = NULL;

// ========================== 界面 7: 决策界面 ==========================
static lv_obj_t *screen_decision = NULL;
static lv_obj_t *dec_title_label = NULL;
static lv_obj_t *dec_opt_btn[DECISION_MAX_OPTS] = {NULL};
static lv_obj_t *dec_opt_label[DECISION_MAX_OPTS] = {NULL};
static lv_obj_t *dec_confirm_btn = NULL;
// 触摸命中矩形 (屏幕坐标 240x320): 选项纵向排列, 确认键在底部
#define DEC_OPT_X       10
#define DEC_OPT_W       220
#define DEC_OPT_H       40
#define DEC_OPT_Y0      70
#define DEC_OPT_GAP     48
#define DEC_CONFIRM_Y   280
#define DEC_CONFIRM_H   35

static void create_screen_touch_test() {
    screen_touch_test = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_touch_test, lv_color_hex(0x000000), 0);

    // 标题
    lv_obj_t *title = lv_label_create(screen_touch_test);
    lv_label_set_text(title, "Touch Test");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // 坐标显示
    touch_coord_label = lv_label_create(screen_touch_test);
    lv_label_set_text(touch_coord_label, "X: --- Y: ---");
    lv_obj_set_style_text_color(touch_coord_label, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_text_font(touch_coord_label, &lv_font_montserrat_18, 0);
    lv_obj_align(touch_coord_label, LV_ALIGN_TOP_MID, 0, 40);

    // 状态显示
    touch_status_label = lv_label_create(screen_touch_test);
    lv_label_set_text(touch_status_label, "Waiting...");
    lv_obj_set_style_text_color(touch_status_label, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(touch_status_label, &lv_font_montserrat_14, 0);
    lv_obj_align(touch_status_label, LV_ALIGN_TOP_MID, 0, 70);

    // 触摸指示点
    touch_dot = lv_obj_create(screen_touch_test);
    lv_obj_set_size(touch_dot, 20, 20);
    lv_obj_set_style_radius(touch_dot, 10, 0);
    lv_obj_set_style_bg_color(touch_dot, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_border_width(touch_dot, 2, 0);
    lv_obj_set_style_border_color(touch_dot, lv_color_hex(0xFFFFFF), 0);
    lv_obj_add_flag(touch_dot, LV_OBJ_FLAG_HIDDEN);

    // 提示
    lv_obj_t *hint = lv_label_create(screen_touch_test);
    lv_label_set_text(hint, "Touch anywhere\nGreen dot follows");
    lv_obj_set_style_text_color(hint, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(hint, LV_ALIGN_CENTER, 0, 0);
}

static void update_screen_touch_test() {
    if (!touch_coord_label || !touch_status_label || !touch_dot) return;

    uint16_t x, y;
    if (touch.scan(&x, &y)) {
        lv_label_set_text_fmt(touch_coord_label, "X: %d  Y: %d", x, y);
        lv_label_set_text(touch_status_label, "TOUCHED");
        lv_obj_set_style_text_color(touch_status_label, lv_color_hex(0x00FF00), 0);
        lv_obj_clear_flag(touch_dot, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(touch_dot, x - 10, y - 10);
    } else {
        lv_label_set_text(touch_coord_label, "X: --- Y: ---");
        lv_label_set_text(touch_status_label, "Released");
        lv_obj_set_style_text_color(touch_status_label, lv_color_hex(0x888888), 0);
        lv_obj_add_flag(touch_dot, LV_OBJ_FLAG_HIDDEN);
    }
}

// ========================== 界面 7: 决策界面 (创建 + 刷新) ==========================
static void create_screen_decision() {
    screen_decision = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_decision, lv_color_hex(0x101828), 0);

    dec_title_label = lv_label_create(screen_decision);
    lv_label_set_long_mode(dec_title_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(dec_title_label, 220);
    lv_obj_set_style_text_color(dec_title_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(dec_title_label, &lv_font_montserrat_18, 0);
    lv_obj_align(dec_title_label, LV_ALIGN_TOP_MID, 0, 15);
    lv_label_set_text(dec_title_label, "");

    for (int i = 0; i < DECISION_MAX_OPTS; i++) {
        dec_opt_btn[i] = lv_obj_create(screen_decision);
        lv_obj_set_size(dec_opt_btn[i], DEC_OPT_W, DEC_OPT_H);
        lv_obj_set_pos(dec_opt_btn[i], DEC_OPT_X, DEC_OPT_Y0 + i * DEC_OPT_GAP);
        lv_obj_set_style_radius(dec_opt_btn[i], 8, 0);
        lv_obj_set_style_bg_color(dec_opt_btn[i], lv_color_hex(0x2A3441), 0);
        lv_obj_clear_flag(dec_opt_btn[i], LV_OBJ_FLAG_SCROLLABLE);
        dec_opt_label[i] = lv_label_create(dec_opt_btn[i]);
        lv_obj_set_style_text_color(dec_opt_label[i], lv_color_hex(0xE5E7EB), 0);
        lv_obj_center(dec_opt_label[i]);
        lv_label_set_text(dec_opt_label[i], "");
    }

    dec_confirm_btn = lv_obj_create(screen_decision);
    lv_obj_set_size(dec_confirm_btn, DEC_OPT_W, DEC_CONFIRM_H);
    lv_obj_set_pos(dec_confirm_btn, DEC_OPT_X, DEC_CONFIRM_Y);
    lv_obj_set_style_radius(dec_confirm_btn, 8, 0);
    lv_obj_set_style_bg_color(dec_confirm_btn, lv_color_hex(0x2563EB), 0);
    lv_obj_clear_flag(dec_confirm_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *cl = lv_label_create(dec_confirm_btn);
    lv_label_set_text(cl, "confirm");
    lv_obj_set_style_text_color(cl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(cl);
}

static void update_screen_decision() {
    if (!dec_title_label) return;
    lv_label_set_text(dec_title_label, g_decision_req.title);
    for (int i = 0; i < DECISION_MAX_OPTS; i++) {
        if (!dec_opt_btn[i]) continue;
        if (i < g_decision_req.opt_count) {
            lv_obj_clear_flag(dec_opt_btn[i], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(dec_opt_label[i], g_decision_req.opts[i]);
            uint32_t bg = (i == g_decision_sel) ? 0x2563EB : 0x2A3441;  // 选中变蓝
            lv_obj_set_style_bg_color(dec_opt_btn[i], lv_color_hex(bg), 0);
        } else {
            lv_obj_add_flag(dec_opt_btn[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

// 切换界面 (操作 LVGL 前必须持有 xGuiSemaphore, 避免与动画刷新/lv_task_handler 竞态卡死)
// ======================= 通用详情卡片 (界面 1/2 复用) =======================
static bool detail_card_open() { return detail_card_bg != NULL; }

static void close_detail_card() {
    if (detail_card_bg) {
        lv_obj_del(detail_card_bg);   // 删遮罩会连带删子对象(卡片)
        detail_card_bg = NULL;
    }
}

// 点击遮罩空白处关闭；点击卡片本体不关闭
static void detail_card_bg_event(lv_event_t *e) {
    lv_obj_t *target = lv_event_get_target(e);
    if (target == detail_card_bg) {
        close_detail_card();
    }
}

static void show_detail_card(const char *title, const char *body) {
    close_detail_card();  // 先关旧的，避免叠加占用 PSRAM

    // 半透明遮罩 (覆盖当前活动屏幕)
    detail_card_bg = lv_obj_create(lv_scr_act());
    lv_obj_set_size(detail_card_bg, 240, 320);
    lv_obj_align(detail_card_bg, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(detail_card_bg, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(detail_card_bg, LV_OPA_60, 0);
    lv_obj_set_style_border_width(detail_card_bg, 0, 0);
    lv_obj_set_style_radius(detail_card_bg, 0, 0);
    lv_obj_clear_flag(detail_card_bg, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(detail_card_bg, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(detail_card_bg, detail_card_bg_event, LV_EVENT_CLICKED, NULL);

    // 卡片本体
    lv_obj_t *card = lv_obj_create(detail_card_bg);
    lv_obj_set_size(card, 210, 240);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1F2937), 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x374151), 0);

    lv_obj_t *lbl_title = lv_label_create(card);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_label_set_text(lbl_title, title);

    lv_obj_t *lbl_body = lv_label_create(card);
    lv_obj_set_style_text_font(lbl_body, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_body, lv_color_hex(0xD1D5DB), 0);
    lv_label_set_long_mode(lbl_body, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl_body, 186);
    lv_obj_align(lbl_body, LV_ALIGN_TOP_LEFT, 0, 34);
    lv_label_set_text(lbl_body, body);
}

static void switch_screen(uint8_t screen_num) {
    uint8_t old_screen = current_screen;
    current_screen = screen_num % 8;  // 8 个界面 (0-5 + 6触摸测试 + 7决策)
    screen_dirty = true;

    // 加锁保护 LVGL 操作
    bool locked = (xGuiSemaphore != NULL) &&
                  (xSemaphoreTake(xGuiSemaphore, portMAX_DELAY) == pdTRUE);

    if (current_screen == 0) {
        lv_scr_load(screen_status);
        update_screen_status();
    } else if (current_screen == 1) {
        lv_scr_load(screen_token);
        update_screen_token();
    } else if (current_screen == 2) {
        lv_scr_load(screen_project);
        update_screen_project();
    } else if (current_screen == 3) {
        lv_scr_load(screen_inspo);
        update_screen_inspo();
    } else if (current_screen == 4) {
        lv_scr_load(screen_profile);
        update_screen_profile();
    } else if (current_screen == 6) {
        lv_scr_load(screen_touch_test);
        update_screen_touch_test();
    } else if (current_screen == 7) {
        if (!screen_decision) create_screen_decision();
        update_screen_decision();
        lv_scr_load(screen_decision);
    } else {
        lv_scr_load(screen_ai_status);
        update_screen_ai_status();
    }

    if (locked) xSemaphoreGive(xGuiSemaphore);
    Serial.printf("Switched: %d -> %d (screens: 0=Status, 1=Token, 2=Project, 3=Inspo, 4=Profile, 5=AI, 6=TouchTest)\n",
                  old_screen, current_screen);
}

// ======================= 演示模式：注入模拟数据 =======================
static void inject_demo_data() {
    // Token 演示数据 (模拟图片中的示例)
    memset(&token_data, 0, sizeof(token_data));
    token_data.frame_type = FRAME_TYPE_TOKEN_STATUS;
    token_data.count = 4;
    token_data.seq_num = 0;

    // OpenAI Codex: 84.7% (847,200 / 1,000,000)
    strncpy(token_data.items[0].name, "OpenAI Codex", TOKEN_NAME_LEN - 1);
    token_data.items[0].used = 847200;
    token_data.items[0].total = 1000000;
    token_data.items[0].percent_x10 = 847;

    // Claude Code Opus: 62.5% (312,500 / 500,000)
    strncpy(token_data.items[1].name, "Claude Code Opus", TOKEN_NAME_LEN - 1);
    token_data.items[1].used = 312500;
    token_data.items[1].total = 500000;
    token_data.items[1].percent_x10 = 625;

    // Claude Code Sonnet: 62.3% (1,245,000 / 2,000,000)
    strncpy(token_data.items[2].name, "Claude Code Sonnet", TOKEN_NAME_LEN - 1);
    token_data.items[2].used = 1245000;
    token_data.items[2].total = 2000000;
    token_data.items[2].percent_x10 = 623;

    // Cursor GPT-4: 39.2%
    strncpy(token_data.items[3].name, "Cursor GPT-4", TOKEN_NAME_LEN - 1);
    token_data.items[3].used = 392000;
    token_data.items[3].total = 1000000;
    token_data.items[3].percent_x10 = 392;

    token_data.crc8 = espnow_crc8((uint8_t*)&token_data, sizeof(token_data) - 1);
    token_data_valid = true;

    // Project 演示数据
    memset(&project_data, 0, sizeof(project_data));
    project_data.frame_type = FRAME_TYPE_PROJECT_STATUS;
    project_data.count = 5;
    project_data.seq_num = 0;

    strncpy(project_data.items[0].name, "Smart Todo App", PROJECT_NAME_LEN - 1);
    project_data.items[0].status_code = PROJ_STATUS_REVIEW;

    strncpy(project_data.items[1].name, "AI Chat Assistant", PROJECT_NAME_LEN - 1);
    project_data.items[1].status_code = PROJ_STATUS_PLANNING;

    strncpy(project_data.items[2].name, "Voice Keyboard", PROJECT_NAME_LEN - 1);
    project_data.items[2].status_code = PROJ_STATUS_CODING;

    strncpy(project_data.items[3].name, "Weather Dashboard", PROJECT_NAME_LEN - 1);
    project_data.items[3].status_code = PROJ_STATUS_COMPLETED;

    strncpy(project_data.items[4].name, "Desktop Pet", PROJECT_NAME_LEN - 1);
    project_data.items[4].status_code = PROJ_STATUS_COMPLETED;

    project_data.crc8 = espnow_crc8((uint8_t*)&project_data, sizeof(project_data) - 1);
    project_data_valid = true;

    // 界面 3: Product Inspo 演示数据
    static const char *INSPO_TEXT =
        "Lenovo CodeBuddy is more than a tool for coding - it is a new "
        "creative interface for the AI era. Designed to turn ideas into "
        "action, CodeBuddy enables users to vibe code through natural "
        "voice interaction, capture inspiration instantly, and stay "
        "continuously connected with AI assistants throughout the day.";
    inspo_full_text = INSPO_TEXT;
    lv_label_set_text(label_inspo_date, "July 29, 2026");
    lv_label_set_text(label_inspo_content, INSPO_TEXT);

    // 界面 4: User Profile 演示数据
    lv_label_set_text(label_profile_name, "Suo Ya");

    // 界面 5: AI Status 文字由状态机(apply_ai_state)自动管理, 无需在此设置

    demo_mode_active = true;
    Serial.println("\n*** DEMO MODE ACTIVATED ***");
    Serial.println("All demo data injected (6 screens)!");
    Serial.println("Press B to cycle: Status/Token/Project/Inspo/Profile/AI");

    // 演示模式下显示选择题容器
    if (choice_container != NULL) {
        lv_obj_clear_flag(choice_container, LV_OBJ_FLAG_HIDDEN);
    }
    choice_selected = 0;  // 默认选中 A
    choice_confirmed = false;

    // 演示模式注入后停在 AI Status (SCREEN_ORDER[0]=5), 与开机界面一致
    order_index = 0;
    switch_screen(SCREEN_ORDER[order_index]);
}

// ======================= 发送按键帧辅助函数 =======================
static void send_key(uint8_t keycode, const char *name) {
    key_frame_t kf;
    kf.frame_type = FRAME_TYPE_KEY;
    kf.modifier = 0;
    memset(kf.keycode, 0, 6);
    kf.timestamp = (uint16_t)(millis() & 0xFFFF);
    kf.seq_num = 0;

    // 按下
    kf.action = 1;
    kf.keycode[0] = keycode;
    kf.crc8 = espnow_crc8((uint8_t*)&kf, sizeof(kf) - 1);
    esp_now_send(dongle_mac, (uint8_t*)&kf, sizeof(kf));
    delay(20);
    // 释放
    kf.action = 0;
    kf.keycode[0] = 0;
    kf.crc8 = espnow_crc8((uint8_t*)&kf, sizeof(kf) - 1);
    esp_now_send(dongle_mac, (uint8_t*)&kf, sizeof(kf));

    Serial.printf("Key sent: %s (0x%02X)\n", name, keycode);
}

// 组 0x0B 决策回执帧, 经 ESP-NOW 回传 Dongle
static void send_decision_reply(uint16_t id, uint8_t index) {
    decision_reply_frame_t rep;
    memset(&rep, 0, sizeof(rep));
    rep.frame_type = FRAME_TYPE_DECISION_REPLY;
    rep.decision_id = id;
    rep.chosen_index = index;
    rep.crc8 = espnow_crc8((uint8_t*)&rep, sizeof(rep) - 1);
    esp_now_send(dongle_mac, (uint8_t*)&rep, sizeof(rep));
    Serial.printf("Decision reply id=%u index=%u sent\n", id, index);
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n\n=== CodeBuddy P1 Enhanced: Mic + Status Display ===");

    init_board();

    // BOOT 按键初始化 (GPIO0, 内部上拉, 按下=0)
    pinMode(0, INPUT_PULLUP);

    // 初始化 LVGL + 显示
    lvgl_begin();
    xGuiSemaphore = xSemaphoreCreateMutex();
    xSemaphoreGive(xGuiSemaphore);

    // SD 卡初始化 (必须在创建界面之前，界面 4/5 依赖 sd_card_ready 决定加载图片还是降级)
    Serial.println("\n>>> SD Card Init <<<");
    Serial.printf("SD Pins: CS=%d MOSI=%d MISO=%d SCLK=%d\n", SD_CS, SD_MOSI, SD_MISO, SD_SCLK);

    // 硬件检测：测试 MISO 引脚是否能读取
    pinMode(SD_MISO, INPUT_PULLUP);
    delay(10);
    int miso_state = digitalRead(SD_MISO);
    Serial.printf("MISO pin state (pullup): %d (should be 1 if card present)\n", miso_state);

    // 配置 CS 为输出并拉高（确保 SD 卡不干扰 SPI 总线）
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);
    delay(10);

    // ATK BOX: SD 卡走 SPI2 (FSPI), 与 LCD 并口无总线冲突
    sd_spi = new SPIClass(FSPI);
    sd_spi->begin(SD_SCLK, SD_MISO, SD_MOSI);

    Serial.println("SPI bus initialized, attempting card detection...");

    // 尝试多个速度，从低到高重试
    uint32_t speeds[] = {400000, 1000000, 4000000};
    for (int i = 0; i < 3 && !sd_card_ready; i++) {
        Serial.printf("Trying SD init at %d Hz...\n", speeds[i]);
        if (SD.begin(SD_CS, *sd_spi, speeds[i])) {
            Serial.printf("✓ SD card initialized at %d Hz\n", speeds[i]);
            uint64_t cardSize = SD.cardSize() / (1024 * 1024);
            Serial.printf("  Card Size: %llu MB\n", cardSize);
            Serial.printf("  Card Type: ");
            uint8_t cardType = SD.cardType();
            if (cardType == CARD_MMC) Serial.println("MMC");
            else if (cardType == CARD_SD) Serial.println("SD");
            else if (cardType == CARD_SDHC) Serial.println("SDHC");
            else Serial.println("UNKNOWN");

            lv_fs_sd_init();  // 注册 LVGL SD 文件系统 (D: 盘符)
            sd_card_ready = true;
            break;
        }
        delay(200);
    }

    if (!sd_card_ready) {
        Serial.println("SD card init FAILED - will use fallback images");
    }

    // 扫描 SD 卡用户头像 (界面 4 依赖 profile_count 决定切换行为)
    scan_profile_images();

    // 创建六个界面 + 触摸测试界面 (此时 sd_card_ready 已确定)
    create_screen_status();
    create_screen_token();
    create_screen_project();
    create_screen_inspo();
    create_screen_profile();
    create_screen_ai_status();
    create_screen_touch_test();

    // I2C 扫描 (ES8311/触摸已在 init_board() 初始化, 这里仅诊断)
    Serial.println("\n>>> I2C scan <<<");
    i2c_scan(48, 45);

    // ESP-NOW
    espnow_ready = espnow_init();

    // I2S 麦克风
    mic_i2s_init();

    // 加载初始界面: 直接进入触摸测试界面 (临时调试)
    switch_screen(6);

    Serial.println("Setup complete. Swipe left/right or press B to cycle screens");
}

void loop() {
    uint32_t now = millis();

    // --- 决策请求: 收到 0x0A 后切到决策界面 (screen 7) ---
    if (g_decision_pending) {
        g_decision_pending = false;
        g_decision_active = true;
        g_decision_sel = -1;
        g_decision_deadline_ms = millis() + 15000;   // 15s 超时
        switch_screen(7);
    }

    // --- 决策界面激活: 触摸命中 + 超时; 暂停常规按键/轮换, 但 LVGL 刷新照常 ---
    if (g_decision_active) {
        // 超时: 发 0xFF 回执, 退出决策界面
        if ((int32_t)(millis() - g_decision_deadline_ms) >= 0) {
            send_decision_reply(g_decision_req.decision_id, 0xFF);
            g_decision_active = false;
            switch_screen(SCREEN_ORDER[order_index]);   // 回到轮换界面
        } else {
            uint16_t tx, ty;
            static uint32_t last_touch_ms = 0;
            if (touch.scan(&tx, &ty) && (millis() - last_touch_ms > 250)) {
                last_touch_ms = millis();
                // 命中选项?
                bool hit = false;
                for (int i = 0; i < g_decision_req.opt_count; i++) {
                    int y0 = DEC_OPT_Y0 + i * DEC_OPT_GAP;
                    if (tx >= DEC_OPT_X && tx <= DEC_OPT_X + DEC_OPT_W &&
                        ty >= y0 && ty <= y0 + DEC_OPT_H) {
                        g_decision_sel = i;
                        hit = true;
                        if (xGuiSemaphore && xSemaphoreTake(xGuiSemaphore, portMAX_DELAY) == pdTRUE) {
                            update_screen_decision();
                            xSemaphoreGive(xGuiSemaphore);
                        }
                        break;
                    }
                }
                // 命中确认键且已选?
                if (!hit && g_decision_sel >= 0 &&
                    tx >= DEC_OPT_X && tx <= DEC_OPT_X + DEC_OPT_W &&
                    ty >= DEC_CONFIRM_Y && ty <= DEC_CONFIRM_Y + DEC_CONFIRM_H) {
                    send_decision_reply(g_decision_req.decision_id, (uint8_t)g_decision_sel);
                    g_decision_active = false;
                    switch_screen(SCREEN_ORDER[order_index]);
                }
            }
        }
        // 决策界面激活时跳过常规按键/轮换/音频, 但 lv_task_handler 仍需跑 (界面刷新)
        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) {
            lv_task_handler();
            xSemaphoreGive(xGuiSemaphore);
        }
        delay(2);
        return;
    }

    // --- A+B 同时长按 2 秒: 触发演示模式 ---
    static uint32_t both_press_start = 0;
    static bool both_pressed_before = false;
    static bool both_hold_consumed = false;
    {
        bool a_now = (digital_read_key_a() == 0);
        bool b_now = (digital_read_key_b() == 0);
        bool both_now = a_now && b_now;

        if (both_now && !both_pressed_before) {
            both_press_start = now;
            both_hold_consumed = false;
            Serial.println("A+B pressed, hold 2s for DEMO...");
        }
        if (both_now && !both_hold_consumed && (now - both_press_start >= DEMO_TRIGGER_MS)) {
            inject_demo_data();
            both_hold_consumed = true;
            // 等待释放，避免松手时误触发单键逻辑
            while (digital_read_key_a() == 0 || digital_read_key_b() == 0) {
                delay(20);
            }
            prev_a = false;
            prev_b = false;
            both_pressed_before = false;
            return;  // 本轮结束
        }
        both_pressed_before = both_now;

        // A+B 同时按住时，跳过单键逻辑
        if (both_now) {
            prev_a = a_now;
            prev_b = b_now;
            if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) {
                lv_task_handler();
                xSemaphoreGive(xGuiSemaphore);
            }
            delay(2);
            return;
        }
    }

    // --- 按键 A: 短按=F2+切换音频流, 长按=Enter确认 (演示模式下操作选择题) ---
    static uint32_t lastBtn = 0;
    if (now - lastBtn >= 10) {
        lastBtn = now;
        bool a = (digital_read_key_a() == 0);

        if (a && !prev_a) {
            press_time_a = now;
        } else if (!a && prev_a) {
            uint32_t duration = now - press_time_a;

            // 界面 1 Token Usage: 短按 A 高亮下一个, 长按 A 重置当前
            if (demo_mode_active && current_screen == 1) {
                uint8_t count = token_data_valid ? token_data.count : 0;
                if (count > 0) {
                    if (duration < LONG_PRESS_MS) {
                        token_selected = (token_selected + 1) % count;
                        Serial.printf("Token: highlight %d %s\n", token_selected,
                                      token_data.items[token_selected].name);
                        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) {
                            update_token_highlight();
                            xSemaphoreGive(xGuiSemaphore);
                        }
                    } else {
                        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) {
                            reset_token_item(token_selected);
                            xSemaphoreGive(xGuiSemaphore);
                        }
                    }
                }
            }
            // 演示模式下，在界面 2 时操作选择题:
            //   短按 A = 在 A/B 选项间切换; 长按 A = 确认当前选择
            else if (demo_mode_active && current_screen == 2 && !choice_confirmed) {
                if (duration < LONG_PRESS_MS) {
                    // 短按: 在 A/B 之间切换
                    choice_selected = (choice_selected == 0) ? 1 : 0;
                    Serial.printf("Choice: toggle to %c\n", choice_selected == 0 ? 'A' : 'B');
                    if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) {
                        update_choice_highlight();
                        xSemaphoreGive(xGuiSemaphore);
                    }
                } else {
                    // 长按: 确认当前选择
                    choice_confirmed = true;
                    Serial.printf("Choice: confirmed option %c\n", choice_selected == 0 ? 'A' : 'B');
                    if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) {
                        update_choice_highlight();
                        xSemaphoreGive(xGuiSemaphore);
                    }
                }
            }
            // 界面 3 Product Inspo: 短按 A 逐字显示, 长按 A 跳过显示全文
            else if (demo_mode_active && current_screen == 3) {
                if (duration < LONG_PRESS_MS) {
                    if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) {
                        start_typing_animation();
                        xSemaphoreGive(xGuiSemaphore);
                    }
                } else {
                    inspo_typing_active = false;
                    inspo_cursor_visible = false;
                    if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) {
                        lv_label_set_text(label_inspo_content, inspo_full_text);
                        xSemaphoreGive(xGuiSemaphore);
                    }
                    Serial.println("Inspo: skip to full text");
                }
            }
            // 界面 4 User Profile: 长按 A 切换头像
            else if (demo_mode_active && current_screen == 4) {
                if (duration >= LONG_PRESS_MS) {
                    if (profile_count > 1) {
                        profile_index = (profile_index + 1) % profile_count;
                        Serial.printf("Profile: switched to user %d\n", profile_index + 1);
                        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) {
                            update_screen_profile();
                            xSemaphoreGive(xGuiSemaphore);
                        }
                    }
                }
            } else {
                // 正常模式或不在界面 2: 原有逻辑
                if (duration < LONG_PRESS_MS) {
                    // 短按: F2 + 切换音频流
                    streaming = !streaming;
                    if (streaming) {
                        i2s_zero_dma_buffer(I2S_NUM_0);
                        audio_seq_num = 0;
                        Serial.println("Streaming STARTED");
                    } else {
                        Serial.printf("Streaming STOPPED. TX=%u FAIL=%u\n", packetCount, sendFailCount);
                    }
                    send_key(0x3B, "F2 (voice input)");
                    if (current_screen == 0) screen_dirty = true;
                } else {
                    // 长按: Enter 确认 (UX 重定义: 翻页交给按键 B 短按, Enter 从 B 移到此处)
                    send_key(0x28, "Enter (confirm)");
                }
            }
        }
        prev_a = a;
    }

    // --- 按键 B: 短按=切换界面(翻页), 长按=连续Backspace ---
    static uint32_t lastBtnB = 0;
    static bool b_long_triggered = false;
    if (now - lastBtnB >= 10) {
        lastBtnB = now;
        bool b = (digital_read_key_b() == 0);

        if (b && !prev_b) {
            press_time_b = now;
            last_repeat_b = now;
            b_long_triggered = false;
        } else if (b && prev_b) {
            // 持续按住: 长按连续 Backspace
            uint32_t duration = now - press_time_b;
            if (duration >= LONG_PRESS_MS) {
                b_long_triggered = true;
                if (now - last_repeat_b >= REPEAT_INTERVAL_MS) {
                    last_repeat_b = now;
                    send_key(0x2A, "Backspace (repeat)");
                }
            }
        } else if (!b && prev_b) {
            uint32_t duration = now - press_time_b;
            if (!b_long_triggered && duration < LONG_PRESS_MS) {
                // 短按: 切换到下一个界面 (触摸滑动翻页已移除, 翻页改由此键承担)
                order_index = (order_index + 1) % 7;
                Serial.printf("Key B short -> next screen %d\n", SCREEN_ORDER[order_index]);
                switch_screen(SCREEN_ORDER[order_index]);
            }
            // 长按已在持续期间处理 (连续 Backspace)，释放时不再动作
        }
        prev_b = b;
    }

    // --- 按键 BOOT (GPIO0): 短按=返回主界面(AI Status), 长按=跳转 TouchTest ---
    static uint32_t lastBtnBoot = 0;
    if (now - lastBtnBoot >= 10) {
        lastBtnBoot = now;
        bool boot = (digital_read_key_boot() == 0);

        if (boot && !prev_boot) {
            press_time_boot = now;
        } else if (!boot && prev_boot) {
            uint32_t duration = now - press_time_boot;
            if (duration < LONG_PRESS_MS) {
                // 短按 BOOT: 返回主界面 (AI Status = SCREEN_ORDER[0] = 界面 5)
                order_index = 0;
                Serial.println("BOOT short -> return to main (AI Status)");
                switch_screen(SCREEN_ORDER[0]);
            } else {
                // 长按 BOOT: 特殊功能 - 切换到触摸测试界面
                Serial.println("BOOT long press -> Jump to TouchTest screen");
                // 找到触摸测试界面在 SCREEN_ORDER 中的位置
                for (uint8_t i = 0; i < 7; i++) {
                    if (SCREEN_ORDER[i] == 6) {  // 界面 6 = TouchTest
                        order_index = i;
                        break;
                    }
                }
                switch_screen(6);
            }
        }
        prev_boot = boot;
    }

    // --- 触摸滑动翻页已移除: 翻页仅由按键 B(K1) 短按完成 ---
    // (触摸仍用于 LVGL 屏幕内点击交互, 由 lv_port indev 驱动独立处理)

    // --- 音频采集 + ESP-NOW 发送 ---
    if (streaming && espnow_ready) {
        size_t bytesRead = 0;
        i2s_read(I2S_NUM_0, stereoBuf, sizeof(stereoBuf), &bytesRead, portMAX_DELAY);
        int frames = bytesRead / 4;

        for (int i = 0; i < frames; i++) {
            monoBuf[i] = stereoBuf[2 * i + MIC_CHANNEL_OFFSET];
        }

        audio_frame_t frame;
        frame.frame_type = FRAME_TYPE_AUDIO;
        frame.seq_num = audio_seq_num++;
        frame.timestamp = (uint16_t)(millis() & 0xFFFF);
        memcpy(frame.audio_data, monoBuf, AUDIO_PAYLOAD_BYTES);
        frame.crc8 = espnow_crc8((uint8_t*)&frame, sizeof(frame) - 1);

        esp_err_t result = esp_now_send(dongle_mac, (uint8_t*)&frame, sizeof(frame));
        if (result == ESP_OK) {
            packetCount++;
        } else {
            sendFailCount++;
        }

        if (packetCount % 250 == 0 && current_screen == 0) screen_dirty = true;
    }

    // --- 界面刷新 ---
    if (screen_dirty) {
        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) {
            if (current_screen == 0) update_screen_status();
            else if (current_screen == 1) update_screen_token();
            else if (current_screen == 2) update_screen_project();
            else if (current_screen == 3) update_screen_inspo();
            else if (current_screen == 4) update_screen_profile();
            else update_screen_ai_status();
            xSemaphoreGive(xGuiSemaphore);
        }
        screen_dirty = false;
    }

    // --- AI Status 界面的持续旋转动画 (每 100ms 刷新一次) ---
    static uint32_t last_ai_anim = 0;
    if (current_screen == 5 && (now - last_ai_anim >= 100)) {
        last_ai_anim = now;
        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) {
            update_screen_ai_status();
            xSemaphoreGive(xGuiSemaphore);
        }
    }

    // --- 触摸测试界面的实时更新 (每 50ms 刷新一次) ---
    static uint32_t last_touch_update = 0;
    if (current_screen == 6 && (now - last_touch_update >= 50)) {
        last_touch_update = now;
        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) {
            update_screen_touch_test();
            xSemaphoreGive(xGuiSemaphore);
        }
    }

    // --- LVGL 任务处理 ---
    if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) {
        // 推进界面 3 打字动画
        if (demo_mode_active && current_screen == 3) {
            advance_typing();
        }
        lv_task_handler();
        xSemaphoreGive(xGuiSemaphore);
    }

    delay(2);
}

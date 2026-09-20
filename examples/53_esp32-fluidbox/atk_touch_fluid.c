/**
 * @file atk_touch_fluid.c
 * @brief 触摸交互驱动流体实现
 *
 * 物理模型:
 *   - 触摸点相对屏幕中心的位置 → 重力方向 (归一化矢量)
 *   - 触摸点到中心的距离 → 重力强度 (远离中心 = 更强倾斜)
 *   - 滑动速度 (px/s) → 角速度 omega[2] (绕Z轴旋转)
 *   - 松手后 → 指数衰减回自动循环模式
 *
 * 平滑处理:
 *   - 低通滤波触摸位置 (避免抖动)
 *   - 衰减过渡 (松手后 2 秒内平滑过渡到自动模式)
 */

#include "atk_touch_fluid.h"
#include "config.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define TWO_PI (2.0f * M_PI)

// 屏幕中心点 (240×320)
#define SCREEN_CENTER_X 120.0f
#define SCREEN_CENTER_Y 160.0f

// 触摸灵敏度参数
#define TOUCH_GRAVITY_SCALE  1.5f   // 触摸距离 → 重力倍增 (>1 = 更敏感)
#define TOUCH_OMEGA_SCALE    0.02f  // 滑动速度(px/s) → 角速度(rad/s)
#define TOUCH_LOWPASS_ALPHA  0.3f   // 位置低通滤波系数 (越小越平滑)
#define RELEASE_DECAY_TIME   2.0f   // 松手后衰减到自动模式的时间 (秒)

// 自动演示参数 (与 atk_imu_sim.c 一致)
#define AUTO_GRAVITY_ROTATE_PERIOD_S 12.0f
#define AUTO_SHAKE_PERIOD_S          7.0f
#define AUTO_SHAKE_GAIN              0.3f
#define AUTO_ROTATION_GAIN           1.0f
#define AUTO_OMEGA_MAX               0.5f

// 状态机
typedef enum {
    MODE_AUTO,        // 自动循环演示
    MODE_TOUCH,       // 触摸控制中
    MODE_DECAY        // 松手后衰减
} touch_mode_t;

static struct {
    touch_mode_t mode;
    float time;              // 累计时间 (秒)

    // 触摸状态
    float touch_x, touch_y;  // 低通滤波后的触摸位置
    float prev_x, prev_y;    // 上一帧位置 (用于计算滑动速度)
    float slide_vx, slide_vy;// 滑动速度 (px/s)

    // 衰减状态
    float decay_start_time;  // 松手时刻
    sim_forces_t touch_forces; // 松手瞬间的力 (衰减基准)
} s_state;

// 前向声明
static void compute_auto_forces(float time, sim_forces_t *out);

void touch_fluid_init(void) {
    memset(&s_state, 0, sizeof(s_state));
    s_state.mode = MODE_AUTO;
    s_state.touch_x = SCREEN_CENTER_X;
    s_state.touch_y = SCREEN_CENTER_Y;
    s_state.prev_x = SCREEN_CENTER_X;
    s_state.prev_y = SCREEN_CENTER_Y;
}

void touch_fluid_update(bool touch_active, uint16_t touch_x, uint16_t touch_y) {
    // 状态转换
    if (touch_active) {
        if (s_state.mode == MODE_AUTO || s_state.mode == MODE_DECAY) {
            // 进入触摸模式, 初始化位置 (避免突变)
            s_state.touch_x = touch_x;
            s_state.touch_y = touch_y;
            s_state.prev_x = touch_x;
            s_state.prev_y = touch_y;
            s_state.slide_vx = 0.0f;
            s_state.slide_vy = 0.0f;
        }
        s_state.mode = MODE_TOUCH;

        // 低通滤波触摸位置 (平滑抖动)
        s_state.touch_x += TOUCH_LOWPASS_ALPHA * (touch_x - s_state.touch_x);
        s_state.touch_y += TOUCH_LOWPASS_ALPHA * (touch_y - s_state.touch_y);

    } else {
        // 松手
        if (s_state.mode == MODE_TOUCH) {
            s_state.mode = MODE_DECAY;
            s_state.decay_start_time = s_state.time;
            // 保存松手瞬间的力作为衰减起点
            // (在 touch_fluid_read 中填充)
        }
    }
}

void touch_fluid_read(float dt, sim_forces_t *out) {
    s_state.time += dt;

    // 根据模式计算物理力
    switch (s_state.mode) {
        case MODE_TOUCH: {
            // 触摸点相对屏幕中心的偏移
            float dx = s_state.touch_x - SCREEN_CENTER_X;
            float dy = s_state.touch_y - SCREEN_CENTER_Y;
            float dist = sqrtf(dx * dx + dy * dy);

            // 重力方向: 指向触摸点
            float gx = 0.0f, gy = 0.0f;
            if (dist > 5.0f) {  // 死区 (中心 5px 内不产生重力)
                gx = dx / dist;
                gy = dy / dist;
            }

            // 重力强度: 距离越远越强 (模拟倾斜角度)
            float tilt_factor = fminf(dist / 120.0f, 1.0f);  // 归一化到 [0, 1]
            float g_strength = GRAVITY_GAIN * GRAVITY_MPS2 * PX_PER_METER * TOUCH_GRAVITY_SCALE * tilt_factor;

            out->gravity[0] = gx * g_strength;
            out->gravity[1] = gy * g_strength;
            out->gravity[2] = 0.0f;

            // 计算滑动速度 (用于角速度)
            float vx = (s_state.touch_x - s_state.prev_x) / fmaxf(dt, 0.001f);
            float vy = (s_state.touch_y - s_state.prev_y) / fmaxf(dt, 0.001f);
            s_state.prev_x = s_state.touch_x;
            s_state.prev_y = s_state.touch_y;

            // 低通滤波速度 (平滑)
            s_state.slide_vx += 0.5f * (vx - s_state.slide_vx);
            s_state.slide_vy += 0.5f * (vy - s_state.slide_vy);

            // 滑动速度 → 角速度 (绕Z轴, 右手定则: 向右滑 = 逆时针旋转)
            // 使用速度的切向分量 (垂直于半径方向)
            float omega_z = (-s_state.slide_vx * dy + s_state.slide_vy * dx) / fmaxf(dist, 50.0f);
            omega_z *= TOUCH_OMEGA_SCALE;

            out->omega[0] = 0.0f;
            out->omega[1] = 0.0f;
            out->omega[2] = omega_z;

            // 角加速度 (数值微分, 简化为 0)
            out->alpha[0] = 0.0f;
            out->alpha[1] = 0.0f;
            out->alpha[2] = 0.0f;

            // 保存当前力 (用于衰减模式)
            memcpy(&s_state.touch_forces, out, sizeof(sim_forces_t));
            break;
        }

        case MODE_DECAY: {
            // 松手后指数衰减到自动模式
            float elapsed = s_state.time - s_state.decay_start_time;
            float decay = expf(-elapsed / RELEASE_DECAY_TIME);  // e^(-t/T)

            if (decay < 0.01f) {
                // 衰减完成, 切换到自动模式
                s_state.mode = MODE_AUTO;
                // 继续执行 MODE_AUTO 逻辑
            } else {
                // 计算自动模式的力
                sim_forces_t auto_forces;
                compute_auto_forces(s_state.time, &auto_forces);

                // 线性插值: touch_forces * decay + auto_forces * (1 - decay)
                for (int i = 0; i < 3; i++) {
                    out->gravity[i] = s_state.touch_forces.gravity[i] * decay + auto_forces.gravity[i] * (1.0f - decay);
                    out->omega[i]   = s_state.touch_forces.omega[i]   * decay + auto_forces.omega[i]   * (1.0f - decay);
                    out->alpha[i]   = s_state.touch_forces.alpha[i]   * decay + auto_forces.alpha[i]   * (1.0f - decay);
                }
                break;
            }
        }
        // Fall through to MODE_AUTO

        case MODE_AUTO:
        default: {
            // 自动循环演示 (与 atk_imu_sim.c 一致)
            compute_auto_forces(s_state.time, out);
            break;
        }
    }
}

// 自动演示力计算 (复制自 atk_imu_sim.c)
static void compute_auto_forces(float time, sim_forces_t *out) {
    // 1. 基础重力方向: 在 XY 平面缓慢旋转
    const float gravity_angle = TWO_PI * time / AUTO_GRAVITY_ROTATE_PERIOD_S;
    float gx = sinf(gravity_angle);
    float gy = cosf(gravity_angle);
    float gz = 0.0f;

    // 2. 晃动脉冲
    const float shake_phase = TWO_PI * time / AUTO_SHAKE_PERIOD_S;
    const float shake_strength = AUTO_SHAKE_GAIN * sinf(shake_phase);
    const float shake_dir_x = -gy;
    const float shake_dir_y = gx;
    gx += shake_strength * shake_dir_x;
    gy += shake_strength * shake_dir_y;

    // 归一化
    const float g_mag = sqrtf(gx * gx + gy * gy + gz * gz);
    if (g_mag > 0.1f) {
        gx /= g_mag;
        gy /= g_mag;
        gz /= g_mag;
    }

    const float g_px = GRAVITY_GAIN * GRAVITY_MPS2 * PX_PER_METER;
    out->gravity[0] = gx * g_px;
    out->gravity[1] = gy * g_px;
    out->gravity[2] = gz * g_px;

    // 3. 自动角速度
    const float omega_phase = TWO_PI * time / (AUTO_GRAVITY_ROTATE_PERIOD_S * 0.7f);
    const float omega_z = AUTO_ROTATION_GAIN * AUTO_OMEGA_MAX * sinf(omega_phase);

    out->omega[0] = 0.0f;
    out->omega[1] = 0.0f;
    out->omega[2] = omega_z;

    // 角加速度
    const float omega_freq = TWO_PI / (AUTO_GRAVITY_ROTATE_PERIOD_S * 0.7f);
    const float alpha_z = AUTO_ROTATION_GAIN * AUTO_OMEGA_MAX * omega_freq * cosf(omega_phase);

    out->alpha[0] = 0.0f;
    out->alpha[1] = 0.0f;
    out->alpha[2] = alpha_z;
}

/**
 * @file atk_imu_sim.c
 * @brief 自动重力动画实现
 *
 * 三层叠加:
 *   1. 重力方向在 XY 平面旋转 (12秒一圈), 模拟倾斜板子
 *   2. 晃动脉冲 (7秒周期正弦波), 叠加到重力上, 模拟摇晃
 *   3. 自动角速度 (正弦波), 产生离心/科氏力, 模拟旋转板子
 *
 * 与原 imu.c 对比:
 *   - 原版: 低通分离重力/晃动, 陀螺仪测角速度
 *   - 此版: 全部合成, 三角函数直接生成平滑动画
 */

#include "atk_imu_sim.h"
#include <math.h>

#include "config.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define TWO_PI (2.0f * M_PI)

static float s_time = 0.0f;  // 累计时间 (秒)

void imu_sim_init(void) {
    s_time = 0.0f;
}

void imu_sim_read(float dt, sim_forces_t *out) {
    s_time += dt;

    // 1. 基础重力方向: 在 XY 平面缓慢旋转 (12秒一圈)
    const float gravity_angle = TWO_PI * s_time / AUTO_GRAVITY_ROTATE_PERIOD_S;
    float gx = sinf(gravity_angle);
    float gy = cosf(gravity_angle);   // 主方向 (平均向下)
    float gz = 0.0f;

    // 2. 晃动脉冲: 7秒周期正弦波, 叠加到重力上
    const float shake_phase = TWO_PI * s_time / AUTO_SHAKE_PERIOD_S;
    const float shake_strength = AUTO_SHAKE_GAIN * sinf(shake_phase);
    // 晃动方向与重力垂直 (在 XY 平面内)
    const float shake_dir_x = -gy;   // 重力向 (gx, gy) 则晃动向 (-gy, gx)
    const float shake_dir_y = gx;
    gx += shake_strength * shake_dir_x;
    gy += shake_strength * shake_dir_y;

    // 归一化重力方向 (保持单位长度)
    const float g_mag = sqrtf(gx * gx + gy * gy + gz * gz);
    if (g_mag > 0.1f) {
        gx /= g_mag;
        gy /= g_mag;
        gz /= g_mag;
    }

    // 转换为像素/秒² (与原 imu.c 的 GRAVITY_GAIN * GRAVITY_MPS2 * PX_PER_METER 一致)
    const float g_px = GRAVITY_GAIN * GRAVITY_MPS2 * PX_PER_METER;
    out->gravity[0] = gx * g_px;
    out->gravity[1] = gy * g_px;
    out->gravity[2] = gz * g_px;

    // 3. 自动角速度: 正弦波, 产生旋转参考系伪力 (离心/科氏)
    // 主轴为 Z (垂直屏幕), 频率比重力旋转快一些
    const float omega_phase = TWO_PI * s_time / (AUTO_GRAVITY_ROTATE_PERIOD_S * 0.7f);
    const float omega_z = AUTO_ROTATION_GAIN * AUTO_OMEGA_MAX * sinf(omega_phase);

    out->omega[0] = 0.0f;
    out->omega[1] = 0.0f;
    out->omega[2] = omega_z;

    // 角加速度 (alpha): 对 omega 求导
    // d(sin(wt))/dt = w*cos(wt)
    const float omega_freq = TWO_PI / (AUTO_GRAVITY_ROTATE_PERIOD_S * 0.7f);
    const float alpha_z = AUTO_ROTATION_GAIN * AUTO_OMEGA_MAX * omega_freq * cosf(omega_phase);

    out->alpha[0] = 0.0f;
    out->alpha[1] = 0.0f;
    out->alpha[2] = alpha_z;
}

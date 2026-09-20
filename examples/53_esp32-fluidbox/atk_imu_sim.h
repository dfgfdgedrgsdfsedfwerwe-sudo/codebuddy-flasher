/**
 * @file atk_imu_sim.h
 * @brief 自动重力动画 (替代原 imu.c 的 QMI8658 输入)
 *
 * ATK BOX 无加速度计/陀螺仪, 用预设动画序列驱动流体:
 *   - 重力方向缓慢旋转 (360° / 12秒)
 *   - 周期性晃动脉冲 (7秒周期)
 *   - 自动旋转产生漩涡 (角速度正弦波)
 *
 * 接口与 imu.h 一致, sim.c 无需改动。
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "sim.h"

// 初始化自动动画状态
void imu_sim_init(void);

// 更新自动重力/角速度 (每物理步调用)
// dt: 实时秒 (TIME_SCALE 在 sim.c 中应用, 此处收到真实时间)
// out: 填充 gravity[3], omega[3], alpha[3]
void imu_sim_read(float dt, sim_forces_t *out);

#ifdef __cplusplus
}
#endif

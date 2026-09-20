/**
 * @file atk_touch_fluid.h
 * @brief 触摸交互驱动流体 (替代 atk_imu_sim)
 *
 * 交互模式:
 *   1. 触摸拖拽 → 重力方向指向触摸点, 产生"拨动液体"效果
 *   2. 滑动速度 → 转为角速度 (omega), 产生旋转漩涡
 *   3. 松手 → 平滑衰减回自动演示模式
 *
 * 物理映射:
 *   - 触摸位置 → gravity 方向 (屏幕中心为原点)
 *   - 滑动速度 → omega[2] (Z轴旋转)
 *   - 触摸压力/面积 → gravity 强度 (未来可扩展)
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "sim.h"
#include <stdbool.h>

// 初始化触摸交互状态
void touch_fluid_init(void);

// 更新触摸状态 (主循环中调用, 在 sim_step 之前)
// touch_active: 是否有触摸点
// touch_x/y: 屏幕坐标 (0-239, 0-319)
void touch_fluid_update(bool touch_active, uint16_t touch_x, uint16_t touch_y);

// 计算当前物理力 (替代 imu_sim_read)
// dt: 实时秒
// out: 填充 gravity[3], omega[3], alpha[3]
void touch_fluid_read(float dt, sim_forces_t *out);

#ifdef __cplusplus
}
#endif

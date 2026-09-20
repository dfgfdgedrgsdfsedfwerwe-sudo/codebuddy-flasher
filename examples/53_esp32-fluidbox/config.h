#pragma once

// FluidBox 可调参数 (ATK BOX 240×320 移植版)
// 原项目为 368×448 AMOLED, 此处适配为 240×320 ST7789 并口屏。
// 物理常量尽量保持原值以维持流体质感, 仅改分辨率相关项。

// ---------------------------------------------------------------------------
// Display  (ATK BOX: 240 宽 × 320 高)
// ---------------------------------------------------------------------------

#define LCD_H_RES 240
#define LCD_V_RES 320

// 渲染器不持有整帧, 按水平条带绘制。320 / 32 = 10 个条带, 每带 240×32×2 = 15KB。
#define BAND_ROWS 32
#define BAND_COUNT (LCD_V_RES / BAND_ROWS)

#define DISPLAY_BRIGHTNESS 230  // 0..255 (ATK BOX 背光为 XL9555 开关, 此值仅占位)

// ---------------------------------------------------------------------------
// Simulation box
// ---------------------------------------------------------------------------

// x,y 为屏幕像素, z 为盒子深度: 0=玻璃面, BOX_D=背板。
// 深度保持 75 (原值), 盒子宽高改为屏幕分辨率。
#define BOX_W 240.0f
#define BOX_H 320.0f
#define BOX_D 75.0f

#define PX_PER_METER 12677.0f
#define PX_PER_MM (PX_PER_METER / 1000.0f)

// 面板圆角。保持原物理尺寸。
#define BOX_CORNER_MM 4.5f
#define BOX_CORNER_R (BOX_CORNER_MM * PX_PER_MM)

#define BOX_BACK_FILLET_MM 2.0f
#define BOX_BACK_FILLET (BOX_BACK_FILLET_MM * PX_PER_MM)

#define BOX_FRONT_FILLET (BOX_BACK_FILLET * 0.25f)

// 慢动作时间缩放, 让流体看起来像大而重的液体。
#define TIME_SCALE 0.100f

// 物理步长上限 (缩放秒/子步)。稳定性关键。
#define SIM_DT_MAX 0.0022f

#define GRAVITY_MPS2 9.81f

// 额外重力增益, 补偿步长上限损失的节奏。
#define GRAVITY_GAIN 2.2f

// ---------------------------------------------------------------------------
// Particles
// ---------------------------------------------------------------------------

// 数组按 MAX 分配, 实际运行 COUNT。240×320 盒子体积约为原版的 47%,
// 相应减少粒子数以保持相同的液体"颗粒感"和帧率。
#define PARTICLE_MAX 550
#define PARTICLE_COUNT 450

// 静止时相邻粒子间距。
#define REST_SPACING 17.0f

// 交互半径 (1.65× 间距, 约 18 邻居)。
#define SMOOTH_RADIUS 28.0f

#define SUBSTEPS 1

// 双密度松弛刚度。
#define K_PRESSURE 400000.0f
#define K_NEAR_PRESSURE 800000.0f

// 单步单对最大位移 (像素)。
#define MAX_DISPLACEMENT 4.0f

// 墙壁投影随机内嵌, 防止角落粒子堆到一点。
#define WALL_JITTER 0.35f

// Clavet 粘度。
#define VISC_SIGMA 45.0f
#define VISC_BETA 0.03f

#define WALL_RESTITUTION 0.25f

// 墙壁切向阻力 (每步)。
#define WALL_FRICTION 0.96f

// ---------------------------------------------------------------------------
// 自动重力动画 (替代原 IMU 输入)
// ATK BOX 无加速度计/陀螺仪, 用预设动画序列驱动重力方向。
// ---------------------------------------------------------------------------

// 重力方向旋转一整圈的周期 (秒)。
#define AUTO_GRAVITY_ROTATE_PERIOD_S 12.0f

// 晃动脉冲周期 (秒) 与强度 (相对真实重力的倍数)。
#define AUTO_SHAKE_PERIOD_S 7.0f
#define AUTO_SHAKE_GAIN 0.9f

// 自动旋转角速度 (rad/s), 产生离心/科氏漩涡效果。
#define AUTO_ROTATION_GAIN 1.0f
#define AUTO_OMEGA_MAX 2.5f

// sim.c 使用的旋转力总开关 (原 IMU 版常量, 保留原名)。设为 1 启用离心/科氏/欧拉力。
#define ROTATION_GAIN 1.0f

// 原 imu.c 的晃动/低通常量 (自动动画版不用, 但保留以防引用)。
#define GRAVITY_LP_HZ 1.2f
#define SHAKE_GAIN 1.6f

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

// 针孔投影焦距。原 220 对 75 深盒子, 保持不变。
#define PROJ_FOCAL 220.0f

// 玻璃面处粒子绘制半径, 越靠后越小。
#define PARTICLE_RADIUS_PX 6.5f
#define DISC_MAX_R 10

#define HIGHLIGHT_ENABLE 1
#define HIGHLIGHT_LIFT 0.55f

#define SPEED_LEVELS 64
#define DEPTH_LEVELS 16

#define SPEED_COLOR_MAX 5000.0f
#define SPEED_COLOR_GAMMA 0.55f

#define DEPTH_DIM_MIN 0.60f

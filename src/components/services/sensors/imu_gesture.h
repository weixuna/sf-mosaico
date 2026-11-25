/*
 * Mosaico 手势分类器：shake / tilt / flip
 *
 * 参数和判定顺序与 ESP-Mosaico 的 examples/imu_gesture 保持一致，
 * 状态机语义见 docs/imu_porting.md 第 4 节。
 */

#ifndef __IMU_GESTURE_H__
#define __IMU_GESTURE_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    IMU_GESTURE_NONE = 0,
    IMU_GESTURE_SHAKE,
    IMU_GESTURE_TILT,
    IMU_GESTURE_FLIP,
} imu_gesture_type_t;

typedef struct
{
    uint32_t sample_period_ms;
    uint32_t cooldown_ms;
    float shake_delta_g;        /* 相邻两帧模长变化阈值 */
    float shake_abs_g;          /* 模长绝对值阈值 */
    float tilt_xy_g;            /* X/Y 倾斜阈值 */
    float tilt_az_max_g;        /* 倾斜时 |Z| 上限 */
    float flip_z_g;             /* 屏幕朝下的 Z 阈值 */
} imu_gesture_config_t;

/* Mosaico 原始参数 */
#define IMU_GESTURE_CONFIG_MOSAICO() {  \
    .sample_period_ms = 40,             \
    .cooldown_ms      = 600,            \
    .shake_delta_g    = 1.6f,           \
    .shake_abs_g      = 2.6f,           \
    .tilt_xy_g        = 0.75f,          \
    .tilt_az_max_g    = 0.85f,          \
    .flip_z_g         = -0.70f,         \
}

typedef struct
{
    imu_gesture_config_t config;
    float prev_mag;                 /* 上一帧加速度模长 */
    imu_gesture_type_t last;        /* 上次触发的手势 */
    uint32_t cool_until_ms;         /* 冷却结束时刻 */
} imu_gesture_t;

/* config 为 NULL 时使用 Mosaico 默认参数 */
void imu_gesture_init(imu_gesture_t *gesture,
                      const imu_gesture_config_t *config);

/*
 * 输入一帧加速度（单位 g）和当前毫秒时刻。
 * 返回需要上报的手势；同一手势不会重复触发，冷却期内的事件被丢弃。
 */
imu_gesture_type_t imu_gesture_update(imu_gesture_t *gesture,
                                      float ax, float ay, float az,
                                      uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* __IMU_GESTURE_H__ */

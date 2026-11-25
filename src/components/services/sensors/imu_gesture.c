/*
 * Mosaico 手势分类器：shake / tilt / flip
 */

#include <math.h>
#include "imu_gesture.h"

static imu_gesture_type_t classify(const imu_gesture_t *gesture,
                                   float ax, float ay, float az,
                                   float prev_mag, float cur_mag)
{
    const imu_gesture_config_t *cfg = &gesture->config;

    /* 顺序不能变：shake -> flip -> tilt */
    if ((fabsf(cur_mag - prev_mag) >= cfg->shake_delta_g) ||
            (cur_mag >= cfg->shake_abs_g))
    {
        return IMU_GESTURE_SHAKE;
    }

    if (az <= cfg->flip_z_g)
    {
        return IMU_GESTURE_FLIP;
    }

    if (((fabsf(ax) >= cfg->tilt_xy_g) ||
            (fabsf(ay) >= cfg->tilt_xy_g)) &&
            (fabsf(az) < cfg->tilt_az_max_g))
    {
        return IMU_GESTURE_TILT;
    }

    return IMU_GESTURE_NONE;
}

void imu_gesture_init(imu_gesture_t *gesture,
                      const imu_gesture_config_t *config)
{
    if (gesture == NULL)
    {
        return;
    }

    gesture->config = (config != NULL) ? *config :
                      (imu_gesture_config_t)IMU_GESTURE_CONFIG_MOSAICO();
    gesture->prev_mag = 1.0f;       /* 启动时假设设备静止在 1 g */
    gesture->last = IMU_GESTURE_NONE;
    gesture->cool_until_ms = 0;
}

imu_gesture_type_t imu_gesture_update(imu_gesture_t *gesture,
                                      float ax, float ay, float az,
                                      uint32_t now_ms)
{
    imu_gesture_type_t type;
    float mag;

    if (gesture == NULL)
    {
        return IMU_GESTURE_NONE;
    }

    mag = sqrtf(ax * ax + ay * ay + az * az);
    type = classify(gesture, ax, ay, az, gesture->prev_mag, mag);
    gesture->prev_mag = mag;

    if ((type != IMU_GESTURE_NONE) && (type != gesture->last) &&
            ((int32_t)(now_ms - gesture->cool_until_ms) >= 0))
    {
        gesture->last = type;
        gesture->cool_until_ms = now_ms + gesture->config.cooldown_ms;
        return type;
    }

    /* 手势消失后复位，下一次可以再次触发 */
    if (type == IMU_GESTURE_NONE)
    {
        gesture->last = IMU_GESTURE_NONE;
    }

    return IMU_GESTURE_NONE;
}

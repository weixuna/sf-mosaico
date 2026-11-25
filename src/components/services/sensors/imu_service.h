/*
 * IMU 采样服务
 *
 * 固定周期读取 LSM6DS3TR-C，维护一份带锁的快照，并把 shake / tilt / flip
 * 事件回调给订阅者。服务不依赖 LVGL，UI 线程可以安全读取快照。
 */

#ifndef __IMU_SERVICE_H__
#define __IMU_SERVICE_H__

#include <stdint.h>
#include "lsm6ds3tr_c.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    lsm6ds3tr_c_vec3_t accel_g;
    lsm6ds3tr_c_vec3_t gyro_dps;
    float temp_c;
    uint32_t tick_ms;
    uint8_t valid;
} imu_snapshot_t;

typedef enum
{
    IMU_EVT_SHAKE = 0,
    IMU_EVT_TILT,
    IMU_EVT_FLIP,
} imu_event_type_t;

typedef struct
{
    imu_event_type_t type;
    uint32_t tick_ms;
    float ax;
    float ay;
    float az;
} imu_event_t;

/* 回调在采样线程上下文执行，不能做阻塞操作 */
typedef void (*imu_event_cb_t)(const imu_event_t *evt, void *ctx);

/* 创建采样线程；重复调用无副作用 */
int imu_service_init(void);

/* 读取最新快照；valid = 0 表示还没有成功采到数据 */
int imu_service_get(imu_snapshot_t *out);

/* 订阅事件；cb 为 NULL 时取消订阅 */
int imu_service_subscribe(imu_event_cb_t cb, void *ctx);

/* shell 自测用的寄存器访问 */
int imu_service_read_reg(uint8_t reg, uint8_t *val);
int imu_service_write_reg(uint8_t reg, uint8_t val);

#ifdef __cplusplus
}
#endif

#endif /* __IMU_SERVICE_H__ */

/*
 * BMM150 板级适配
 *
 * I2C2: SCL = PA24, SDA = PA25
 * 右侧 0x11，左侧 0x12
 * 电源: TCA9555 P07 SENS_3V3（与 IMU 共用）
 * INT 未使用，第一阶段轮询
 */

#ifndef __BMM150_PORT_H__
#define __BMM150_PORT_H__

#include "bmm150.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 本板的 I2C / 电源 / 双传感器轴映射适配 */
const bmm150_bus_t *bmm150_port_bus(void);

#ifdef __cplusplus
}
#endif

#endif /* __BMM150_PORT_H__ */

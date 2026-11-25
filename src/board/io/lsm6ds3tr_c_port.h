/*
 * LSM6DS3TR-C 板级适配
 *
 * I2C2: SCL = PA24, SDA = PA25
 * 地址: 0x6A（SA0 = 0），INT1 = PA26（第一阶段轮询，不使用中断）
 * 电源: TCA9555 P07 SENS_3V3（与两颗 BMM150 共用）
 */

#ifndef __LSM6DS3TR_C_PORT_H__
#define __LSM6DS3TR_C_PORT_H__

#include "lsm6ds3tr_c.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 本板的 I2C / 电源 / 轴映射适配 */
const lsm6ds3tr_c_bus_t *lsm6ds3tr_c_port_bus(void);

#ifdef __cplusplus
}
#endif

#endif /* __LSM6DS3TR_C_PORT_H__ */

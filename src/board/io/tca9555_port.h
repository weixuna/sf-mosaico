/*
 * TCA9555 port adapter for this project.
 *
 * This is the only file that knows about I2C2 and the 0x20 address.
 * On another project, replace the callbacks with the local I2C API.
 */

#ifndef __TCA9555_PORT_H__
#define __TCA9555_PORT_H__

#include "tca9555.h"

/* 7-bit I2C address: A2 = A1 = A0 = 0 */
#define TCA9555_I2C_ADDR    (0x20U)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Initialize the port adapter (mutex + device descriptor).
 *         Must be called after the I2C bus is initialized.
 */
int tca9555_port_init(void);

/**
 * @brief  Get the TCA9555 device instance.
 */
tca9555_t *tca9555_port_get(void);

/**
 * @brief  Millisecond delay used by pulse-style switches (e.g. WF_PWRKEY).
 */
void tca9555_port_delay_ms(uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif /* __TCA9555_PORT_H__ */

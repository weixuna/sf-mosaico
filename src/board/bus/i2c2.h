/*
 * I2C2 sensor bus driver
 *
 * Board wiring:
 *   PA24 -> I2C2_SCL
 *   PA25 -> I2C2_SDA
 *
 * The bus is used as a 7-bit address I2C master. All device addresses in
 * this API are 7-bit addresses (the R/W bit is added by the driver).
 */

#ifndef __I2C2_H__
#define __I2C2_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 100 kHz is the safe default for an unknown bus. It can be raised to
   400000 after the wiring and pull-ups have been verified. */
#define I2C2_SPEED_HZ           (100000U)
#define I2C2_TIMEOUT_MS         (100U)
#define I2C2_PROBE_TIMEOUT_MS   (10U)

/* Standard 7-bit address range, 0x00~0x07 and 0x78~0x7F are reserved. */
#define I2C2_SCAN_ADDR_MIN      (0x08U)
#define I2C2_SCAN_ADDR_MAX      (0x77U)

/**
 * @brief  Initialize I2C2 on PA24 (SCL) / PA25 (SDA).
 * @return 0 on success, -1 on failure.
 */
int i2c2_init(void);

/**
 * @brief  Probe one 7-bit address.
 * @param  addr 7-bit device address.
 * @return 1 if the device acknowledged, 0 if not, -1 on error.
 */
int i2c2_probe(uint8_t addr);

/**
 * @brief  Scan the whole 7-bit address range and print the result.
 * @return Number of devices found, or -1 on error.
 */
int i2c2_scan(void);

/**
 * @brief  Write a sensor register (8-bit register address).
 * @param  addr 7-bit device address.
 * @param  reg  Register address.
 * @param  data Data to write.
 * @param  len  Number of bytes to write (must be > 0).
 * @return 0 on success, -1 on failure.
 */
int i2c2_write_reg(uint8_t addr, uint8_t reg, const uint8_t *data, uint16_t len);

/**
 * @brief  Read sensor registers (8-bit register address).
 * @param  addr 7-bit device address.
 * @param  reg  Register address.
 * @param  data Buffer for received data.
 * @param  len  Number of bytes to read (must be > 0).
 * @return 0 on success, -1 on failure.
 */
int i2c2_read_reg(uint8_t addr, uint8_t reg, uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* __I2C2_H__ */

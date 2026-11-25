/*
 * Generic TCA9555 16-bit I2C I/O expander driver.
 *
 * This file is MCU/RTOS independent. The project provides I2C read/write
 * callbacks through tca9555_t. See tca9555_port.c for the porting example.
 *
 * Register map:
 *   0x00/0x01  Input port 0/1        (read only)
 *   0x02/0x03  Output port 0/1
 *   0x04/0x05  Polarity inversion 0/1
 *   0x06/0x07  Configuration 0/1     (1 = input, 0 = output)
 */

#ifndef __TCA9555_H__
#define __TCA9555_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TCA9555_REG_INPUT0      (0x00U)
#define TCA9555_REG_INPUT1      (0x01U)
#define TCA9555_REG_OUTPUT0     (0x02U)
#define TCA9555_REG_OUTPUT1     (0x03U)
#define TCA9555_REG_POLARITY0   (0x04U)
#define TCA9555_REG_POLARITY1   (0x05U)
#define TCA9555_REG_CONFIG0     (0x06U)
#define TCA9555_REG_CONFIG1     (0x07U)

#define TCA9555_PIN_NUM         (16U)

/* I2C callbacks: return 0 on success, -1 on failure. */
typedef int (*tca9555_i2c_write_fn)(void *ctx, uint8_t dev_addr,
                                    uint8_t reg, const uint8_t *buf,
                                    uint16_t len);
typedef int (*tca9555_i2c_read_fn)(void *ctx, uint8_t dev_addr,
                                   uint8_t reg, uint8_t *buf,
                                   uint16_t len);

/* Optional lock callbacks: return 0 on success, -1 on failure. */
typedef int (*tca9555_lock_fn)(void *ctx);
typedef void (*tca9555_unlock_fn)(void *ctx);

typedef struct
{
    uint8_t dev_addr;

    tca9555_i2c_write_fn write;
    tca9555_i2c_read_fn read;
    void *i2c_ctx;

    tca9555_lock_fn lock;
    tca9555_unlock_fn unlock;
    void *lock_ctx;

    uint16_t out_shadow;    /* output latch shadow */
    uint16_t dir_shadow;    /* 1 = input, 0 = output */
    uint8_t ready;
} tca9555_t;

/**
 * @brief  Probe the expander and load output/configuration shadows.
 * @return 0 on success, -1 on failure.
 */
int tca9555_init(tca9555_t *dev);

/**
 * @brief  Set polarity inversion. 0 = normal, bit = 1 inverts that input.
 */
int tca9555_set_polarity(tca9555_t *dev, uint16_t polarity);

/**
 * @brief  Configure one pin.
 * @param  pin      0..15 (P00..P17)
 * @param  is_input 1 = input, 0 = output
 * @param  level    Output level to preload (0/1). For inputs it sets the
 *                  latch value used when the pin is later made an output.
 */
int tca9555_pin_config(tca9555_t *dev, uint8_t pin,
                       uint8_t is_input, uint8_t level);

/**
 * @brief  Write one pin. Automatically makes the pin an output if needed.
 */
int tca9555_pin_write(tca9555_t *dev, uint8_t pin, uint8_t level);

/**
 * @brief  Read the actual pin level from the input register.
 */
int tca9555_pin_read(tca9555_t *dev, uint8_t pin, uint8_t *level);

/**
 * @brief  Update selected bits of one output port.
 * @param  port  0 or 1
 * @param  value New output values
 * @param  mask  1 = update this bit, 0 = keep current latch
 */
int tca9555_port_write(tca9555_t *dev, uint8_t port,
                       uint8_t value, uint8_t mask);

/**
 * @brief  Configure one whole port.
 * @param  port       0 or 1
 * @param  input_mask 1 = input, 0 = output
 * @param  level      Output latch value for this port
 */
int tca9555_port_config(tca9555_t *dev, uint8_t port,
                        uint8_t input_mask, uint8_t level);

/**
 * @brief  Read all 16 input pins.
 */
int tca9555_read_all(tca9555_t *dev, uint16_t *inputs);

/**
 * @brief  Get cached output latch values.
 */
int tca9555_get_outputs(tca9555_t *dev, uint16_t *outputs);

/**
 * @brief  Get cached direction bits (1 = input).
 */
int tca9555_get_directions(tca9555_t *dev, uint16_t *dirs);

#ifdef __cplusplus
}
#endif

#endif /* __TCA9555_H__ */

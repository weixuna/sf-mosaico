/*
 * Generic TCA9555 16-bit I2C I/O expander driver.
 *
 * The driver only depends on the callbacks in tca9555_t, so it can be
 * reused with RT-Thread, bare-metal HAL, software I2C, etc.
 */

#include <stddef.h>
#include "tca9555.h"

static int dev_lock(tca9555_t *dev)
{
    if (dev == NULL)
    {
        return -1;
    }

    if (dev->lock != NULL)
    {
        return dev->lock(dev->lock_ctx);
    }

    return 0;
}

static void dev_unlock(tca9555_t *dev)
{
    if ((dev != NULL) && (dev->unlock != NULL))
    {
        dev->unlock(dev->lock_ctx);
    }
}

static int read_reg(tca9555_t *dev, uint8_t reg, uint8_t *value)
{
    if ((dev == NULL) || (dev->read == NULL) || (value == NULL))
    {
        return -1;
    }

    return dev->read(dev->i2c_ctx, dev->dev_addr, reg, value, 1);
}

static int write_reg(tca9555_t *dev, uint8_t reg, uint8_t value)
{
    if ((dev == NULL) || (dev->write == NULL))
    {
        return -1;
    }

    return dev->write(dev->i2c_ctx, dev->dev_addr, reg, &value, 1);
}

static uint8_t reg_for_port(uint8_t reg0, uint8_t port)
{
    return (port == 0) ? reg0 : (uint8_t)(reg0 + 1);
}

static uint8_t shadow_get(uint16_t shadow, uint8_t port)
{
    return (uint8_t)((shadow >> (port * 8)) & 0xFF);
}

static void shadow_set(uint16_t *shadow, uint8_t port, uint8_t value)
{
    if (port == 0)
    {
        *shadow = (uint16_t)((*shadow & 0xFF00) | value);
    }
    else
    {
        *shadow = (uint16_t)((*shadow & 0x00FF) | ((uint16_t)value << 8));
    }
}

int tca9555_init(tca9555_t *dev)
{
    uint8_t out0;
    uint8_t out1;
    uint8_t cfg0;
    uint8_t cfg1;

    if ((dev == NULL) || (dev->read == NULL) || (dev->write == NULL))
    {
        return -1;
    }

    if (dev_lock(dev) != 0)
    {
        return -1;
    }

    if ((read_reg(dev, TCA9555_REG_OUTPUT0, &out0) != 0) ||
            (read_reg(dev, TCA9555_REG_OUTPUT1, &out1) != 0) ||
            (read_reg(dev, TCA9555_REG_CONFIG0, &cfg0) != 0) ||
            (read_reg(dev, TCA9555_REG_CONFIG1, &cfg1) != 0))
    {
        dev->ready = 0;
        dev_unlock(dev);
        return -1;
    }

    dev->out_shadow = ((uint16_t)out1 << 8) | out0;
    dev->dir_shadow = ((uint16_t)cfg1 << 8) | cfg0;
    dev->ready = 1;

    dev_unlock(dev);
    return 0;
}

int tca9555_set_polarity(tca9555_t *dev, uint16_t polarity)
{
    if ((dev == NULL) || !dev->ready)
    {
        return -1;
    }

    if (dev_lock(dev) != 0)
    {
        return -1;
    }

    if ((write_reg(dev, TCA9555_REG_POLARITY0, (uint8_t)(polarity & 0xFF)) != 0) ||
            (write_reg(dev, TCA9555_REG_POLARITY1, (uint8_t)(polarity >> 8)) != 0))
    {
        dev_unlock(dev);
        return -1;
    }

    dev_unlock(dev);
    return 0;
}

int tca9555_pin_config(tca9555_t *dev, uint8_t pin,
                       uint8_t is_input, uint8_t level)
{
    uint8_t port;
    uint8_t bit;
    uint8_t out;
    uint8_t cfg;

    if ((dev == NULL) || !dev->ready || (pin >= TCA9555_PIN_NUM))
    {
        return -1;
    }

    if (dev_lock(dev) != 0)
    {
        return -1;
    }

    port = (uint8_t)(pin >> 3);
    bit = (uint8_t)(1U << (pin & 0x07));

    /* Preload the output latch first, so switching direction cannot glitch. */
    out = shadow_get(dev->out_shadow, port);
    if (level)
    {
        out |= bit;
    }
    else
    {
        out &= (uint8_t)~bit;
    }

    if (write_reg(dev, reg_for_port(TCA9555_REG_OUTPUT0, port), out) != 0)
    {
        dev_unlock(dev);
        return -1;
    }
    shadow_set(&dev->out_shadow, port, out);

    cfg = shadow_get(dev->dir_shadow, port);
    if (is_input)
    {
        cfg |= bit;
    }
    else
    {
        cfg &= (uint8_t)~bit;
    }

    if (write_reg(dev, reg_for_port(TCA9555_REG_CONFIG0, port), cfg) != 0)
    {
        dev_unlock(dev);
        return -1;
    }
    shadow_set(&dev->dir_shadow, port, cfg);

    dev_unlock(dev);
    return 0;
}

int tca9555_pin_write(tca9555_t *dev, uint8_t pin, uint8_t level)
{
    uint8_t port;
    uint8_t bit;
    uint8_t out;
    uint8_t cfg;

    if ((dev == NULL) || !dev->ready || (pin >= TCA9555_PIN_NUM))
    {
        return -1;
    }

    if (dev_lock(dev) != 0)
    {
        return -1;
    }

    port = (uint8_t)(pin >> 3);
    bit = (uint8_t)(1U << (pin & 0x07));

    out = shadow_get(dev->out_shadow, port);
    if (level)
    {
        out |= bit;
    }
    else
    {
        out &= (uint8_t)~bit;
    }

    if (write_reg(dev, reg_for_port(TCA9555_REG_OUTPUT0, port), out) != 0)
    {
        dev_unlock(dev);
        return -1;
    }
    shadow_set(&dev->out_shadow, port, out);

    cfg = shadow_get(dev->dir_shadow, port);
    if (cfg & bit)
    {
        /* Currently an input: switch it to output as a convenience. */
        cfg &= (uint8_t)~bit;
        if (write_reg(dev, reg_for_port(TCA9555_REG_CONFIG0, port), cfg) != 0)
        {
            dev_unlock(dev);
            return -1;
        }
        shadow_set(&dev->dir_shadow, port, cfg);
    }

    dev_unlock(dev);
    return 0;
}

int tca9555_pin_read(tca9555_t *dev, uint8_t pin, uint8_t *level)
{
    uint8_t port;
    uint8_t value;

    if ((dev == NULL) || !dev->ready || (pin >= TCA9555_PIN_NUM) ||
            (level == NULL))
    {
        return -1;
    }

    if (dev_lock(dev) != 0)
    {
        return -1;
    }

    port = (uint8_t)(pin >> 3);
    if (read_reg(dev, reg_for_port(TCA9555_REG_INPUT0, port), &value) != 0)
    {
        dev_unlock(dev);
        return -1;
    }

    *level = (uint8_t)((value >> (pin & 0x07)) & 0x01);
    dev_unlock(dev);

    return 0;
}

int tca9555_port_write(tca9555_t *dev, uint8_t port,
                       uint8_t value, uint8_t mask)
{
    uint8_t out;

    if ((dev == NULL) || !dev->ready || (port > 1))
    {
        return -1;
    }

    if (dev_lock(dev) != 0)
    {
        return -1;
    }

    out = shadow_get(dev->out_shadow, port);
    out = (uint8_t)((out & (uint8_t)~mask) | (value & mask));

    if (write_reg(dev, reg_for_port(TCA9555_REG_OUTPUT0, port), out) != 0)
    {
        dev_unlock(dev);
        return -1;
    }
    shadow_set(&dev->out_shadow, port, out);

    dev_unlock(dev);
    return 0;
}

int tca9555_port_config(tca9555_t *dev, uint8_t port,
                        uint8_t input_mask, uint8_t level)
{
    if ((dev == NULL) || !dev->ready || (port > 1))
    {
        return -1;
    }

    if (dev_lock(dev) != 0)
    {
        return -1;
    }

    /* Preload latches before changing the direction. */
    if (write_reg(dev, reg_for_port(TCA9555_REG_OUTPUT0, port), level) != 0)
    {
        dev_unlock(dev);
        return -1;
    }
    shadow_set(&dev->out_shadow, port, level);

    if (write_reg(dev, reg_for_port(TCA9555_REG_CONFIG0, port), input_mask) != 0)
    {
        dev_unlock(dev);
        return -1;
    }
    shadow_set(&dev->dir_shadow, port, input_mask);

    dev_unlock(dev);
    return 0;
}

int tca9555_read_all(tca9555_t *dev, uint16_t *inputs)
{
    uint8_t in0;
    uint8_t in1;

    if ((dev == NULL) || !dev->ready || (inputs == NULL))
    {
        return -1;
    }

    if (dev_lock(dev) != 0)
    {
        return -1;
    }

    if ((read_reg(dev, TCA9555_REG_INPUT0, &in0) != 0) ||
            (read_reg(dev, TCA9555_REG_INPUT1, &in1) != 0))
    {
        dev_unlock(dev);
        return -1;
    }

    *inputs = ((uint16_t)in1 << 8) | in0;
    dev_unlock(dev);

    return 0;
}

int tca9555_get_outputs(tca9555_t *dev, uint16_t *outputs)
{
    if ((dev == NULL) || !dev->ready || (outputs == NULL))
    {
        return -1;
    }

    if (dev_lock(dev) != 0)
    {
        return -1;
    }

    *outputs = dev->out_shadow;
    dev_unlock(dev);

    return 0;
}

int tca9555_get_directions(tca9555_t *dev, uint16_t *dirs)
{
    if ((dev == NULL) || !dev->ready || (dirs == NULL))
    {
        return -1;
    }

    if (dev_lock(dev) != 0)
    {
        return -1;
    }

    *dirs = dev->dir_shadow;
    dev_unlock(dev);

    return 0;
}

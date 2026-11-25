/*
 * TCA9555 port adapter for this project.
 *
 * I2C bus: I2C2 (PA24 = SCL, PA25 = SDA)
 * Address: 0x20 (A2 = A1 = A0 = 0)
 */

#include <rtthread.h>
#include "i2c2.h"
#include "tca9555.h"
#include "tca9555_port.h"

static struct rt_mutex tca9555_mutex;
static rt_bool_t tca9555_mutex_ready = RT_FALSE;
static tca9555_t tca9555_dev;

#define TCA9555_I2C_RETRY   (3)

static int port_i2c_write(void *ctx, uint8_t dev_addr, uint8_t reg,
                          const uint8_t *buf, uint16_t len)
{
    int i;

    (void)ctx;

    for (i = 0; i < TCA9555_I2C_RETRY; i++)
    {
        if (i2c2_write_reg(dev_addr, reg, buf, len) == 0)
        {
            return 0;
        }

        rt_thread_mdelay(1);
    }

    return -1;
}

static int port_i2c_read(void *ctx, uint8_t dev_addr, uint8_t reg,
                         uint8_t *buf, uint16_t len)
{
    int i;

    (void)ctx;

    for (i = 0; i < TCA9555_I2C_RETRY; i++)
    {
        if (i2c2_read_reg(dev_addr, reg, buf, len) == 0)
        {
            return 0;
        }

        rt_thread_mdelay(1);
    }

    return -1;
}

static int port_lock(void *ctx)
{
    (void)ctx;

    if (!tca9555_mutex_ready)
    {
        return 0;
    }

    return (rt_mutex_take(&tca9555_mutex,
                          rt_tick_from_millisecond(1000)) == RT_EOK) ? 0 : -1;
}

static void port_unlock(void *ctx)
{
    (void)ctx;

    if (tca9555_mutex_ready)
    {
        rt_mutex_release(&tca9555_mutex);
    }
}

int tca9555_port_init(void)
{
    if (!tca9555_mutex_ready)
    {
        if (rt_mutex_init(&tca9555_mutex, "tca9555", RT_IPC_FLAG_PRIO) != RT_EOK)
        {
            return -1;
        }
        tca9555_mutex_ready = RT_TRUE;
    }

    tca9555_dev.dev_addr = TCA9555_I2C_ADDR;
    tca9555_dev.write = port_i2c_write;
    tca9555_dev.read = port_i2c_read;
    tca9555_dev.i2c_ctx = RT_NULL;
    tca9555_dev.lock = port_lock;
    tca9555_dev.unlock = port_unlock;
    tca9555_dev.lock_ctx = RT_NULL;
    tca9555_dev.out_shadow = 0;
    tca9555_dev.dir_shadow = 0;
    tca9555_dev.ready = 0;

    return 0;
}

tca9555_t *tca9555_port_get(void)
{
    return &tca9555_dev;
}

void tca9555_port_delay_ms(uint32_t ms)
{
    rt_thread_mdelay(ms);
}

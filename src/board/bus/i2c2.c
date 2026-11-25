/*
 * I2C2 sensor bus driver / scanner
 *
 * PA24 -> I2C2_SCL
 * PA25 -> I2C2_SDA
 */

#include <rtthread.h>
#include "bf0_hal.h"
#include "i2c2.h"

#define DBG_TAG "sensor.i2c2"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

static I2C_HandleTypeDef i2c2_handle;
static struct rt_mutex i2c2_mutex;
static rt_bool_t i2c2_ready = RT_FALSE;
static rt_bool_t i2c2_mutex_ready = RT_FALSE;

static int i2c2_lock(void)
{
    if (!i2c2_ready && i2c2_init() != 0)
    {
        return -1;
    }

    if (i2c2_mutex_ready &&
            rt_mutex_take(&i2c2_mutex, rt_tick_from_millisecond(1000)) != RT_EOK)
    {
        LOG_E("take mutex timeout");
        return -1;
    }

    return 0;
}

static void i2c2_unlock(void)
{
    if (i2c2_mutex_ready)
    {
        rt_mutex_release(&i2c2_mutex);
    }
}

int i2c2_init(void)
{
    if (i2c2_ready)
    {
        return 0;
    }

    /* PA24 = SCL, PA25 = SDA. Enable internal pull-ups; external 4.7K
       pull-ups are still recommended for a reliable bus. */
    HAL_PIN_Set(PAD_PA24, I2C2_SCL, PIN_PULLUP, 1);
    HAL_PIN_Set(PAD_PA25, I2C2_SDA, PIN_PULLUP, 1);
    HAL_RCC_EnableModule(RCC_MOD_I2C2);

    i2c2_handle.Instance = I2C2;
    i2c2_handle.Mode = HAL_I2C_MODE_MASTER;
    i2c2_handle.Init.ClockSpeed = I2C2_SPEED_HZ;
    i2c2_handle.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    i2c2_handle.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    i2c2_handle.Init.OwnAddress1 = 0;

    if (HAL_I2C_Init(&i2c2_handle) != HAL_OK)
    {
        LOG_E("init failed");
        return -1;
    }

    if (!i2c2_mutex_ready)
    {
        if (rt_mutex_init(&i2c2_mutex, "i2c2", RT_IPC_FLAG_PRIO) != RT_EOK)
        {
            LOG_E("mutex init failed");
            return -1;
        }
        i2c2_mutex_ready = RT_TRUE;
    }

    i2c2_ready = RT_TRUE;
    LOG_I("ready: SCL=PA24, SDA=PA25, %u Hz", (unsigned)I2C2_SPEED_HZ);

    return 0;
}

/*
 * A device that is present acknowledges its address. A one-byte read is
 * used here because the HAL of this chip rejects zero-length transfers.
 * The received byte is discarded.
 */
static int i2c2_probe_locked(uint8_t addr)
{
    HAL_StatusTypeDef status;
    uint8_t dummy;

    __HAL_I2C_ENABLE(&i2c2_handle);
    status = HAL_I2C_Master_Receive(&i2c2_handle, addr, &dummy, 1,
                                    I2C2_PROBE_TIMEOUT_MS);
    if (status != HAL_OK)
    {
        /* NACK or timeout: reset the controller before the next probe. */
        HAL_I2C_Reset(&i2c2_handle);
    }
    __HAL_I2C_DISABLE(&i2c2_handle);

    return (status == HAL_OK) ? 1 : 0;
}

int i2c2_probe(uint8_t addr)
{
    int ret;

    if (addr > 0x7F)
    {
        return -1;
    }

    if (i2c2_lock() != 0)
    {
        return -1;
    }

    ret = i2c2_probe_locked(addr);
    i2c2_unlock();

    return ret;
}

int i2c2_scan(void)
{
    static uint8_t found_addr[I2C2_SCAN_ADDR_MAX - I2C2_SCAN_ADDR_MIN + 1];
    int found = 0;
    int addr;
    int i;

    if (i2c2_lock() != 0)
    {
        return -1;
    }

    rt_kprintf("\n[I2C2] scan 7-bit address 0x%02x~0x%02x (SCL=PA24, SDA=PA25)\n",
               I2C2_SCAN_ADDR_MIN, I2C2_SCAN_ADDR_MAX);
    rt_kprintf("    ");
    for (i = 0; i < 16; i++)
    {
        rt_kprintf(" %x ", i);
    }
    rt_kprintf("\n");

    for (addr = 0; addr < 0x80; addr++)
    {
        if ((addr & 0x0F) == 0)
        {
            rt_kprintf("%02x: ", addr);
        }

        if ((addr < I2C2_SCAN_ADDR_MIN) || (addr > I2C2_SCAN_ADDR_MAX))
        {
            rt_kprintf("   ");
        }
        else if (i2c2_probe_locked((uint8_t)addr) > 0)
        {
            rt_kprintf("%02x ", addr);
            found_addr[found++] = (uint8_t)addr;
        }
        else
        {
            rt_kprintf("-- ");
        }

        if ((addr & 0x0F) == 0x0F)
        {
            rt_kprintf("\n");
        }
    }

    rt_kprintf("[I2C2] %d device(s) found", found);
    for (i = 0; i < found; i++)
    {
        rt_kprintf("  0x%02x (W:0x%02x R:0x%02x)",
                   found_addr[i],
                   (unsigned)(found_addr[i] << 1),
                   (unsigned)((found_addr[i] << 1) | 1));
    }
    rt_kprintf("\n");

    i2c2_unlock();

    return found;
}

int i2c2_write_reg(uint8_t addr, uint8_t reg, const uint8_t *data, uint16_t len)
{
    HAL_StatusTypeDef status;

    if ((data == RT_NULL) || (len == 0))
    {
        return -1;
    }

    if (i2c2_lock() != 0)
    {
        return -1;
    }

    __HAL_I2C_ENABLE(&i2c2_handle);
    status = HAL_I2C_Mem_Write(&i2c2_handle, addr, reg, I2C_MEMADD_SIZE_8BIT,
                               (uint8_t *)data, len, I2C2_TIMEOUT_MS);
    if (status != HAL_OK)
    {
        HAL_I2C_Reset(&i2c2_handle);
    }
    __HAL_I2C_DISABLE(&i2c2_handle);

    i2c2_unlock();

    return (status == HAL_OK) ? 0 : -1;
}

int i2c2_read_reg(uint8_t addr, uint8_t reg, uint8_t *data, uint16_t len)
{
    HAL_StatusTypeDef status;

    if ((data == RT_NULL) || (len == 0))
    {
        return -1;
    }

    if (i2c2_lock() != 0)
    {
        return -1;
    }

    __HAL_I2C_ENABLE(&i2c2_handle);
    status = HAL_I2C_Mem_Read(&i2c2_handle, addr, reg, I2C_MEMADD_SIZE_8BIT,
                              data, len, I2C2_TIMEOUT_MS);
    if (status != HAL_OK)
    {
        HAL_I2C_Reset(&i2c2_handle);
    }
    __HAL_I2C_DISABLE(&i2c2_handle);

    i2c2_unlock();

    return (status == HAL_OK) ? 0 : -1;
}

static void i2c2_scan_shell(void)
{
    if (i2c2_init() != 0)
    {
        rt_kprintf("[I2C2] init failed\n");
        return;
    }

    i2c2_scan();
}
MSH_CMD_EXPORT_ALIAS(i2c2_scan_shell, i2c2_scan, scan I2C2 bus (PA24=SCL, PA25=SDA));

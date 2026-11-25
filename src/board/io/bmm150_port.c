/*
 * BMM150 板级适配
 *
 * I2C2: SCL = PA24, SDA = PA25
 * 右侧 0x11，左侧 0x12
 * 电源: TCA9555 P07 SENS_3V3（与 IMU 共用）
 * INT 未使用，第一阶段轮询
 */

#include <rtthread.h>

#include "i2c2.h"
#include "board_io.h"
#include "bmm150.h"
#include "bmm150_port.h"

#define DBG_TAG "board.mag"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

/*
 * 轴映射：索引 0 = 0x11（右），索引 1 = 0x12（左）。
 * 默认芯片坐标就是逻辑坐标；两颗传感器朝向必须实测确认后再改。
 */
static const uint8_t s_axis_src[2][3] =
{
    {0, 1, 2},
    {0, 1, 2},
};

static const int8_t s_axis_sign[2][3] =
{
    {1, 1, 1},
    {1, 1, 1},
};

static int port_read(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len)
{
    return i2c2_read_reg(addr, reg, buf, len);
}

static int port_write(uint8_t addr, uint8_t reg,
                      const uint8_t *buf, uint16_t len)
{
    return i2c2_write_reg(addr, reg, buf, len);
}

static void port_delay_ms(uint32_t ms)
{
    rt_thread_mdelay(ms);
}

static void port_map(uint8_t addr, bmm150_vec3_t *v)
{
    uint8_t idx = (addr == BMM150_I2C_ADDR_RIGHT) ? 0 : 1;
    int16_t raw[3] = {v->x, v->y, v->z};

    v->x = s_axis_sign[idx][0] * raw[s_axis_src[idx][0]];
    v->y = s_axis_sign[idx][1] * raw[s_axis_src[idx][1]];
    v->z = s_axis_sign[idx][2] * raw[s_axis_src[idx][2]];
}

static int port_board_init(void)
{
    if (board_io_init() != 0)
    {
        LOG_W("board IO init failed");
        return -1;
    }

    /* P07 = SENS_3V3，两颗 BMM150 和 IMU 共用 */
    if (board_io_set(BOARD_IO_SENSOR_PWR_EN, 1) != 0)
    {
        LOG_W("sensor power on failed");
        return -1;
    }

    rt_thread_mdelay(10);

    return 0;
}

static const bmm150_bus_t s_bus =
{
    .read = port_read,
    .write = port_write,
    .delay_ms = port_delay_ms,
    .map = port_map,
    .board_init = port_board_init,
};

const bmm150_bus_t *bmm150_port_bus(void)
{
    return &s_bus;
}

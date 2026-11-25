/*
 * LSM6DS3TR-C 板级适配
 *
 * I2C2: SCL = PA24, SDA = PA25
 * 地址: 0x6A（SA0 = 0），INT1 = PA26（第一阶段轮询，不使用中断）
 * 电源: TCA9555 P07 SENS_3V3（与两颗 BMM150 共用）
 */

#include <rtthread.h>

#include "i2c2.h"
#include "board_io.h"
#include "lsm6ds3tr_c.h"
#include "lsm6ds3tr_c_port.h"

#define DBG_TAG "board.imu"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

/*
 * 轴映射：默认芯片坐标就是逻辑坐标。
 * 屏幕朝上时应为 az ≈ +1 g，朝下 az ≈ -1 g；验证后按需修改 src/sign。
 */
static const uint8_t s_axis_src[3] = {0, 1, 2};
static const int8_t s_axis_sign[3] = {1, 1, 1};

static int port_read(uint8_t reg, uint8_t *buf, uint16_t len)
{
    return i2c2_read_reg(LSM6DS3TR_C_I2C_ADDR, reg, buf, len);
}

static int port_write(uint8_t reg, const uint8_t *buf, uint16_t len)
{
    return i2c2_write_reg(LSM6DS3TR_C_I2C_ADDR, reg, buf, len);
}

static void port_delay_ms(uint32_t ms)
{
    rt_thread_mdelay(ms);
}

static void port_map(lsm6ds3tr_c_vec3_t *v)
{
    float raw[3] = {v->x, v->y, v->z};

    v->x = s_axis_sign[0] * raw[s_axis_src[0]];
    v->y = s_axis_sign[1] * raw[s_axis_src[1]];
    v->z = s_axis_sign[2] * raw[s_axis_src[2]];
}

static int port_board_init(void)
{
    if (board_io_init() != 0)
    {
        LOG_W("board IO init failed");
        return -1;
    }

    /* P07 = SENS_3V3，上电后等器件启动 */
    if (board_io_set(BOARD_IO_SENSOR_PWR_EN, 1) != 0)
    {
        LOG_W("sensor power on failed");
        return -1;
    }

    rt_thread_mdelay(10);

    return 0;
}

static const lsm6ds3tr_c_bus_t s_bus =
{
    .read = port_read,
    .write = port_write,
    .delay_ms = port_delay_ms,
    .map = port_map,
    .board_init = port_board_init,
};

const lsm6ds3tr_c_bus_t *lsm6ds3tr_c_port_bus(void)
{
    return &s_bus;
}

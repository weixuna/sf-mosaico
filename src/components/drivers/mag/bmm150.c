/*
 * BMM150 三轴磁力计驱动
 *
 * 初始化流程、寄存器布局和补偿公式来自 Bosch BMM150 数据手册 /
 * BMM150 Sensor API。输出单位为 uT，磁场溢出时 valid = 0。
 */

#include <stddef.h>
#include "bmm150.h"

/* 寄存器地址 */
#define BMM150_REG_CHIP_ID          0x40
#define BMM150_REG_DATA_X_LSB       0x42
#define BMM150_REG_POWER_CONTROL    0x4B
#define BMM150_REG_OP_MODE_ODR      0x4C
#define BMM150_REG_AXES_ENABLE      0x4E
#define BMM150_REG_REP_XY           0x51
#define BMM150_REG_REP_Z            0x52

/* trim 寄存器 */
#define BMM150_DIG_X1               0x5D
#define BMM150_DIG_Y1               0x5E
#define BMM150_DIG_Z4_LSB           0x62
#define BMM150_DIG_Z2_LSB           0x68

#define BMM150_CHIP_ID_VALUE        0x32
#define BMM150_XY_OVERFLOW          (-4096)
#define BMM150_Z_OVERFLOW           (-16384)

/* 上电 / 配置 */
#define BMM150_STARTUP_MS           3
#define BMM150_NORMAL_DELAY_MS      10

/* ODR 10 Hz（0x4C[5:3] = 0），normal 模式（0x4C[2:1] = 0） */
#define BMM150_CTRL_ODR_MASK        0x38
#define BMM150_CTRL_OPMODE_MASK     0x06

/* 板级适配提供；未移植到新板时返回 NULL */
const bmm150_bus_t *bmm150_port_bus(void);
__attribute__((weak)) const bmm150_bus_t *bmm150_port_bus(void)
{
    return NULL;
}

static int read_reg(bmm150_t *dev, uint8_t reg, uint8_t *buf, uint16_t len)
{
    if ((dev == NULL) || (dev->bus == NULL) || (dev->bus->read == NULL) ||
            (buf == NULL) || (len == 0))
    {
        return -1;
    }

    return dev->bus->read(dev->addr, reg, buf, len);
}

static int write_reg(bmm150_t *dev, uint8_t reg, uint8_t value)
{
    if ((dev == NULL) || (dev->bus == NULL) || (dev->bus->write == NULL))
    {
        return -1;
    }

    return dev->bus->write(dev->addr, reg, &value, 1);
}

static void delay_ms(bmm150_t *dev, uint32_t ms)
{
    if (dev->bus->delay_ms != NULL)
    {
        dev->bus->delay_ms(ms);
    }
}

static int16_t to_i16(float value)
{
    if (value > 32767.0f)
    {
        return 32767;
    }
    if (value < -32768.0f)
    {
        return -32768;
    }

    return (int16_t)(value + ((value >= 0.0f) ? 0.5f : -0.5f));
}

/* 官方 X/Y 补偿公式，参数 fs1/fs2 对应 dig_x1/x2 或 dig_y1/y2 */
static int16_t compensate_xy(int16_t raw, uint16_t rhall,
                             int8_t xy2, uint8_t xy1,
                             int8_t fs2, int8_t fs1, uint16_t xyz1)
{
    float t0 = (float)xyz1 * 16384.0f / (float)rhall;
    float t = t0 - 16384.0f;
    float t1 = (float)xy2 * (t * t / 268435456.0f);
    float t2 = t1 + t * (float)xy1 / 16384.0f;
    float t3 = (float)fs2 + 160.0f;
    float t4 = (float)raw * ((t2 + 256.0f) * t3);

    return to_i16((t4 / 8192.0f + (float)fs1 * 8.0f) / 16.0f);
}

/* 官方 Z 补偿公式 */
static int16_t compensate_z(int16_t raw, uint16_t rhall,
                            int16_t z4, int16_t z3, int16_t z2,
                            uint16_t z1, uint16_t xyz1)
{
    float t0 = (float)raw - (float)z4;
    float t1 = (float)rhall - (float)xyz1;
    float t2 = (float)z3 * t1;
    float t3 = (float)z1 * (float)rhall / 32768.0f;
    float t4 = (float)z2 + t3;
    float t5 = t0 * 131072.0f - t2;

    return to_i16((t5 / (t4 * 4.0f)) / 16.0f);
}

static int read_trim(bmm150_t *dev)
{
    uint8_t x1y1[2] = {0};
    uint8_t xyz[4] = {0};
    uint8_t xy1xy2[10] = {0};

    if ((read_reg(dev, BMM150_DIG_X1, x1y1, sizeof(x1y1)) != 0) ||
            (read_reg(dev, BMM150_DIG_Z4_LSB, xyz, sizeof(xyz)) != 0) ||
            (read_reg(dev, BMM150_DIG_Z2_LSB, xy1xy2, sizeof(xy1xy2)) != 0))
    {
        return -1;
    }

    dev->dig_x1 = (int8_t)x1y1[0];
    dev->dig_y1 = (int8_t)x1y1[1];
    dev->dig_z4 = (int16_t)(((uint16_t)xyz[1] << 8) | xyz[0]);
    dev->dig_x2 = (int8_t)xyz[2];
    dev->dig_y2 = (int8_t)xyz[3];

    dev->dig_z2 = (int16_t)(((uint16_t)xy1xy2[1] << 8) | xy1xy2[0]);
    dev->dig_z1 = (uint16_t)(((uint16_t)xy1xy2[3] << 8) | xy1xy2[2]);
    dev->dig_xyz1 = (uint16_t)((((uint16_t)xy1xy2[5] & 0x7F) << 8) |
                               xy1xy2[4]);
    dev->dig_z3 = (int16_t)(((uint16_t)xy1xy2[7] << 8) | xy1xy2[6]);
    dev->dig_xy2 = (int8_t)xy1xy2[8];
    dev->dig_xy1 = xy1xy2[9];

    return 0;
}

static int configure(bmm150_t *dev)
{
    uint8_t value = 0;

    /* regular 模式：XY repetition = 4，Z repetition = 7 */
    if ((write_reg(dev, BMM150_REG_REP_XY, 4) != 0) ||
            (write_reg(dev, BMM150_REG_REP_Z, 7) != 0))
    {
        return -1;
    }

    /* 使能 XYZ 数据通道；不接中断，DRDY 引脚保持关闭 */
    if (write_reg(dev, BMM150_REG_AXES_ENABLE, 0x00) != 0)
    {
        return -1;
    }

    if (read_reg(dev, BMM150_REG_OP_MODE_ODR, &value, 1) != 0)
    {
        return -1;
    }

    /* ODR 10 Hz + normal 模式 */
    value &= (uint8_t)~(BMM150_CTRL_ODR_MASK | BMM150_CTRL_OPMODE_MASK);
    if (write_reg(dev, BMM150_REG_OP_MODE_ODR, value) != 0)
    {
        return -1;
    }

    delay_ms(dev, BMM150_NORMAL_DELAY_MS);

    return 0;
}

int bmm150_init(bmm150_t *dev, const bmm150_bus_t *bus, uint8_t addr)
{
    uint8_t value = 0;
    int i;

    if (dev == NULL)
    {
        return -1;
    }

    if (bus == NULL)
    {
        bus = bmm150_port_bus();
    }

    if ((bus == NULL) || (bus->read == NULL) || (bus->write == NULL) ||
            (bus->delay_ms == NULL))
    {
        return -1;
    }

    if (dev->initialized)
    {
        return 0;
    }

    dev->addr = addr;
    dev->bus = bus;

    /* 两颗 BMM150 共用 SENS_3V3，板级初始化是幂等的 */
    if ((bus->board_init != NULL) && (bus->board_init() != 0))
    {
        return -1;
    }

    /* 上电进入 sleep 模式，等待启动时间 */
    if (read_reg(dev, BMM150_REG_POWER_CONTROL, &value, 1) != 0)
    {
        return -1;
    }
    value |= 0x01;
    if (write_reg(dev, BMM150_REG_POWER_CONTROL, value) != 0)
    {
        return -1;
    }
    delay_ms(dev, BMM150_STARTUP_MS);

    /* 软复位到已知状态，覆盖 warm reset 或上一次 normal 模式 */
    value |= 0x82;
    if (write_reg(dev, BMM150_REG_POWER_CONTROL, value) != 0)
    {
        return -1;
    }
    delay_ms(dev, 2);

    /* CHIP_ID 必须是 0x32 */
    for (i = 0; i < 3; i++)
    {
        if ((read_reg(dev, BMM150_REG_CHIP_ID, &value, 1) == 0) &&
                (value == BMM150_CHIP_ID_VALUE))
        {
            break;
        }
        delay_ms(dev, 1);
    }
    if (i == 3)
    {
        return -1;
    }

    if ((read_trim(dev) != 0) || (configure(dev) != 0))
    {
        return -1;
    }

    dev->initialized = 1;

    return 0;
}

int bmm150_deinit(bmm150_t *dev)
{
    uint8_t value = 0;

    if (dev == NULL)
    {
        return -1;
    }

    if (dev->initialized &&
            (read_reg(dev, BMM150_REG_POWER_CONTROL, &value, 1) == 0))
    {
        value &= (uint8_t)~0x01;
        write_reg(dev, BMM150_REG_POWER_CONTROL, value);
    }

    dev->initialized = 0;

    return 0;
}

int bmm150_read(bmm150_t *dev, bmm150_sample_t *out)
{
    uint8_t raw[8] = {0};
    int16_t rx;
    int16_t ry;
    int16_t rz;
    uint16_t rhall;

    if ((dev == NULL) || (out == NULL) || !dev->initialized)
    {
        return -1;
    }

    if (read_reg(dev, BMM150_REG_DATA_X_LSB, raw, sizeof(raw)) != 0)
    {
        return -1;
    }

    /* X/Y 是 13 位，Z 是 15 位，RHALL 是 14 位 */
    rx = (int16_t)(((int16_t)(int8_t)raw[1] * 32) | (raw[0] >> 3));
    ry = (int16_t)(((int16_t)(int8_t)raw[3] * 32) | (raw[2] >> 3));
    rz = (int16_t)(((int16_t)(int8_t)raw[5] * 128) | (raw[4] >> 1));
    rhall = (uint16_t)(((uint16_t)raw[7] << 6) | (raw[6] >> 2));

    out->rhall = rhall;
    out->valid = 0;
    out->field.x = 0;
    out->field.y = 0;
    out->field.z = 0;

    /* 溢出或 trim 异常时标记本帧无效，不当作 I2C 失败 */
    if ((rx == BMM150_XY_OVERFLOW) || (ry == BMM150_XY_OVERFLOW) ||
            (rz == BMM150_Z_OVERFLOW) || (rhall == 0) ||
            (dev->dig_xyz1 == 0) || (dev->dig_z1 == 0) ||
            (dev->dig_z2 == 0))
    {
        return 0;
    }

    out->field.x = compensate_xy(rx, rhall, dev->dig_xy2, dev->dig_xy1,
                                 dev->dig_x2, dev->dig_x1, dev->dig_xyz1);
    out->field.y = compensate_xy(ry, rhall, dev->dig_xy2, dev->dig_xy1,
                                 dev->dig_y2, dev->dig_y1, dev->dig_xyz1);
    out->field.z = compensate_z(rz, rhall, dev->dig_z4, dev->dig_z3,
                                dev->dig_z2, dev->dig_z1, dev->dig_xyz1);
    out->valid = 1;

    /* 两颗芯片贴装方向不同，由板级 port 按地址映射 */
    if (dev->bus->map != NULL)
    {
        dev->bus->map(dev->addr, &out->field);
    }

    return 0;
}

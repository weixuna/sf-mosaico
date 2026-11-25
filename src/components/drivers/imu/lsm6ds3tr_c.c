/*
 * LSM6DS3TR-C 六轴 IMU 驱动
 *
 * 初始化序列、量程换算和 14 字节 burst 读取来自 docs/imu_porting.md。
 * 驱动不创建线程、不访问 I2C，所有平台相关操作走 lsm6ds3tr_c_bus_t。
 */

#include <stddef.h>
#include "lsm6ds3tr_c.h"

#define LSM6DS3TR_C_WHO_AM_I_RETRY      3
#define LSM6DS3TR_C_RESET_DELAY_MS      50
#define LSM6DS3TR_C_START_DELAY_MS      20

/* 板级适配提供；未移植到新板时返回 NULL */
const lsm6ds3tr_c_bus_t *lsm6ds3tr_c_port_bus(void);
__attribute__((weak)) const lsm6ds3tr_c_bus_t *lsm6ds3tr_c_port_bus(void)
{
    return NULL;
}

/* 小端 int16 */
static int16_t le16(const uint8_t *p)
{
    return (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static void delay_ms(lsm6ds3tr_c_t *dev, uint32_t ms)
{
    if (dev->bus->delay_ms != NULL)
    {
        dev->bus->delay_ms(ms);
    }
}

static int write_reg(lsm6ds3tr_c_t *dev, uint8_t reg, uint8_t value)
{
    return lsm6ds3tr_c_write_reg(dev, reg, &value, 1);
}

/* 加速度 / 陀螺仪 LSB 对应的物理量，非法量程返回 0 */
static float accel_lsb_to_g(lsm6ds3tr_c_accel_fs_t fs)
{
    static const float scale[] =
    {
        0.000061f,  /* ±2 g  */
        0.000488f,  /* ±16 g */
        0.000122f,  /* ±4 g  */
        0.000244f,  /* ±8 g  */
    };

    return ((unsigned)fs < (sizeof(scale) / sizeof(scale[0]))) ?
           scale[fs] : 0.0f;
}

static float gyro_lsb_to_dps(lsm6ds3tr_c_gyro_fs_t fs)
{
    switch (fs)
    {
    case LSM6DS3TR_C_GYRO_250DPS:
        return 0.00875f;
    case LSM6DS3TR_C_GYRO_125DPS:
        return 0.004375f;
    case LSM6DS3TR_C_GYRO_500DPS:
        return 0.0175f;
    case LSM6DS3TR_C_GYRO_1000DPS:
        return 0.035f;
    case LSM6DS3TR_C_GYRO_2000DPS:
        return 0.07f;
    default:
        return 0.0f;
    }
}

/* ODR 对应的采样周期（ms），用于 data-ready 超时 */
static uint16_t odr_period_ms(lsm6ds3tr_c_odr_t odr)
{
    static const uint16_t period[] =
    {
        0,      /* OFF */
        80,     /* 12.5 Hz */
        39,     /* 26 Hz */
        20,     /* 52 Hz */
        10,     /* 104 Hz */
        5,      /* 208 Hz */
        3,      /* 416 Hz */
        2,      /* 833 Hz */
        1,      /* 1.66 kHz */
        1,      /* 3.33 kHz */
        1,      /* 6.66 kHz */
        625,    /* 1.6 Hz */
    };

    return ((unsigned)odr < (sizeof(period) / sizeof(period[0]))) ?
           period[odr] : 0;
}

static int probe(lsm6ds3tr_c_t *dev)
{
    uint8_t id = 0;
    int i;

    for (i = 0; i < LSM6DS3TR_C_WHO_AM_I_RETRY; i++)
    {
        if ((lsm6ds3tr_c_read_reg(dev, LSM6DS3TR_C_REG_WHO_AM_I,
                                  &id, 1) == 0) &&
                (id == LSM6DS3TR_C_WHO_AM_I_VALUE))
        {
            return 0;
        }
        delay_ms(dev, 1);
    }

    return -1;
}

static int soft_reset(lsm6ds3tr_c_t *dev)
{
    uint8_t value = 0;
    int i;

    if (write_reg(dev, LSM6DS3TR_C_REG_CTRL3_C,
                  LSM6DS3TR_C_CTRL3_C_SW_RESET) != 0)
    {
        return -1;
    }

    delay_ms(dev, LSM6DS3TR_C_RESET_DELAY_MS);

    /* SW_RESET 由芯片自动清零，超时说明复位没有完成 */
    for (i = 0; i < 10; i++)
    {
        if ((lsm6ds3tr_c_read_reg(dev, LSM6DS3TR_C_REG_CTRL3_C,
                                  &value, 1) == 0) &&
                ((value & LSM6DS3TR_C_CTRL3_C_SW_RESET) == 0))
        {
            return 0;
        }
        delay_ms(dev, 5);
    }

    return -1;
}

int lsm6ds3tr_c_init(lsm6ds3tr_c_t *dev, const lsm6ds3tr_c_bus_t *bus,
                     uint8_t addr)
{
    uint8_t ctrl3 = LSM6DS3TR_C_CTRL3_C_BDU |
                    LSM6DS3TR_C_CTRL3_C_IF_INC;

    if (dev == NULL)
    {
        return -1;
    }

    if (bus == NULL)
    {
        bus = lsm6ds3tr_c_port_bus();
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
    dev->config = (lsm6ds3tr_c_config_t)LSM6DS3TR_C_CONFIG_MOSAICO();
    dev->started = 0;

    /* 传感器电源和共享 I2C 由板级适配准备 */
    if ((bus->board_init != NULL) && (bus->board_init() != 0))
    {
        return -1;
    }

    if ((probe(dev) != 0) || (soft_reset(dev) != 0))
    {
        return -1;
    }

    /* BDU 保证高低字节属于同一帧，IF_INC 允许地址自增 */
    if (write_reg(dev, LSM6DS3TR_C_REG_CTRL3_C, ctrl3) != 0)
    {
        return -1;
    }

    /* 加速度、陀螺仪都先进入高性能模式，并关闭高通 / LPF2 */
    if ((write_reg(dev, LSM6DS3TR_C_REG_CTRL6_C, 0x00) != 0) ||
            (write_reg(dev, LSM6DS3TR_C_REG_CTRL7_G, 0x00) != 0) ||
            (write_reg(dev, LSM6DS3TR_C_REG_CTRL8_XL, 0x00) != 0) ||
            (write_reg(dev, LSM6DS3TR_C_REG_CTRL1_XL, 0x00) != 0) ||
            (write_reg(dev, LSM6DS3TR_C_REG_CTRL2_G, 0x00) != 0))
    {
        return -1;
    }

    dev->initialized = 1;

    return 0;
}

int lsm6ds3tr_c_start(lsm6ds3tr_c_t *dev,
                      const lsm6ds3tr_c_config_t *config)
{
    lsm6ds3tr_c_config_t cfg;
    float accel_scale;
    float gyro_scale;
    uint16_t accel_period;
    uint16_t gyro_period;
    uint8_t ctrl1;
    uint8_t ctrl2;
    uint8_t ctrl7;
    uint8_t ctrl8;

    if ((dev == NULL) || !dev->initialized)
    {
        return -1;
    }

    cfg = (config != NULL) ? *config :
          (lsm6ds3tr_c_config_t)LSM6DS3TR_C_CONFIG_MOSAICO();

    /* 陀螺仪没有 1.6 Hz 档；量程必须能换算，否则不静默截断 */
    if ((cfg.accel_odr < LSM6DS3TR_C_ODR_12_5HZ) ||
            (cfg.accel_odr > LSM6DS3TR_C_ODR_1_6HZ) ||
            (cfg.gyro_odr < LSM6DS3TR_C_ODR_12_5HZ) ||
            (cfg.gyro_odr > LSM6DS3TR_C_ODR_6660HZ))
    {
        return -1;
    }

    accel_scale = accel_lsb_to_g(cfg.accel_fs);
    gyro_scale = gyro_lsb_to_dps(cfg.gyro_fs);
    if ((accel_scale <= 0.0f) || (gyro_scale <= 0.0f))
    {
        return -1;
    }

    ctrl1 = (uint8_t)(((uint8_t)cfg.accel_odr << LSM6DS3TR_C_ODR_SHIFT) |
                      ((uint8_t)cfg.accel_fs << LSM6DS3TR_C_ACCEL_FS_SHIFT));
    ctrl2 = (uint8_t)(((uint8_t)cfg.gyro_odr << LSM6DS3TR_C_ODR_SHIFT) |
                      ((uint8_t)cfg.gyro_fs << LSM6DS3TR_C_GYRO_FS_SHIFT));

    if (cfg.gyro_fs == LSM6DS3TR_C_GYRO_125DPS)
    {
        ctrl2 |= LSM6DS3TR_C_CTRL2_G_FS_125;
    }

    ctrl7 = cfg.gyro_hp_en ? LSM6DS3TR_C_CTRL7_G_HP_EN : 0x00;
    ctrl8 = cfg.accel_lpf2_en ? LSM6DS3TR_C_CTRL8_XL_LPF2_EN : 0x00;

    if ((write_reg(dev, LSM6DS3TR_C_REG_CTRL7_G, ctrl7) != 0) ||
            (write_reg(dev, LSM6DS3TR_C_REG_CTRL8_XL, ctrl8) != 0) ||
            (write_reg(dev, LSM6DS3TR_C_REG_CTRL1_XL, ctrl1) != 0) ||
            (write_reg(dev, LSM6DS3TR_C_REG_CTRL2_G, ctrl2) != 0))
    {
        return -1;
    }

    /* 等至少两个 104 Hz 周期，让第一帧数据就绪 */
    delay_ms(dev, LSM6DS3TR_C_START_DELAY_MS);

    accel_period = odr_period_ms(cfg.accel_odr);
    gyro_period = odr_period_ms(cfg.gyro_odr);

    dev->config = cfg;
    dev->accel_lsb_to_g = accel_scale;
    dev->gyro_lsb_to_dps = gyro_scale;
    dev->drdy_timeout_ms = (uint16_t)(2 * ((accel_period > gyro_period) ?
                                           accel_period : gyro_period) + 10);
    dev->started = 1;

    return 0;
}

int lsm6ds3tr_c_stop(lsm6ds3tr_c_t *dev)
{
    int ret;

    if ((dev == NULL) || !dev->initialized)
    {
        return -1;
    }

    ret = write_reg(dev, LSM6DS3TR_C_REG_CTRL1_XL, 0x00);
    ret |= write_reg(dev, LSM6DS3TR_C_REG_CTRL2_G, 0x00);

    dev->started = 0;

    return (ret == 0) ? 0 : -1;
}

int lsm6ds3tr_c_deinit(lsm6ds3tr_c_t *dev)
{
    if (dev == NULL)
    {
        return -1;
    }

    if (dev->started)
    {
        lsm6ds3tr_c_stop(dev);
    }

    dev->initialized = 0;
    dev->started = 0;

    return 0;
}

int lsm6ds3tr_c_read(lsm6ds3tr_c_t *dev, lsm6ds3tr_c_sample_t *out)
{
    uint8_t raw[14];                /* temp(2) + gyro(6) + accel(6) */
    uint8_t status = 0;
    uint16_t elapsed;

    if ((dev == NULL) || (out == NULL) || !dev->started)
    {
        return -1;
    }

    /* 等 accel 和 gyro 都就绪，超时保留上一帧由调用者决定 */
    for (elapsed = 0; elapsed <= dev->drdy_timeout_ms; elapsed++)
    {
        if (lsm6ds3tr_c_read_reg(dev, LSM6DS3TR_C_REG_STATUS,
                                 &status, 1) != 0)
        {
            return -1;
        }

        if ((status & (LSM6DS3TR_C_STATUS_XLDA |
                       LSM6DS3TR_C_STATUS_GDA)) ==
                (LSM6DS3TR_C_STATUS_XLDA | LSM6DS3TR_C_STATUS_GDA))
        {
            break;
        }

        delay_ms(dev, 1);
    }

    if (elapsed > dev->drdy_timeout_ms)
    {
        return -1;
    }

    /* 从温度寄存器开始一次 burst 读，BDU 保证三组数据属于同一帧 */
    if (lsm6ds3tr_c_read_reg(dev, LSM6DS3TR_C_REG_OUT_TEMP_L,
                             raw, sizeof(raw)) != 0)
    {
        return -1;
    }

    out->gyro_dps.x = le16(&raw[2]) * dev->gyro_lsb_to_dps;
    out->gyro_dps.y = le16(&raw[4]) * dev->gyro_lsb_to_dps;
    out->gyro_dps.z = le16(&raw[6]) * dev->gyro_lsb_to_dps;
    out->accel_g.x = le16(&raw[8]) * dev->accel_lsb_to_g;
    out->accel_g.y = le16(&raw[10]) * dev->accel_lsb_to_g;
    out->accel_g.z = le16(&raw[12]) * dev->accel_lsb_to_g;
    out->temp_c = le16(&raw[0]) / 256.0f + 25.0f;
    out->valid = 1;

    /* 芯片贴装方向与逻辑方向不一致时，由板级 port 统一映射 */
    if (dev->bus->map != NULL)
    {
        dev->bus->map(&out->accel_g);
        dev->bus->map(&out->gyro_dps);
    }

    return 0;
}

int lsm6ds3tr_c_read_accel(lsm6ds3tr_c_t *dev, lsm6ds3tr_c_vec3_t *out_g)
{
    lsm6ds3tr_c_sample_t sample;

    if ((out_g == NULL) || (lsm6ds3tr_c_read(dev, &sample) != 0))
    {
        return -1;
    }

    *out_g = sample.accel_g;

    return 0;
}

int lsm6ds3tr_c_read_gyro(lsm6ds3tr_c_t *dev, lsm6ds3tr_c_vec3_t *out_dps)
{
    lsm6ds3tr_c_sample_t sample;

    if ((out_dps == NULL) || (lsm6ds3tr_c_read(dev, &sample) != 0))
    {
        return -1;
    }

    *out_dps = sample.gyro_dps;

    return 0;
}

int lsm6ds3tr_c_read_temp(lsm6ds3tr_c_t *dev, float *out_c)
{
    lsm6ds3tr_c_sample_t sample;

    if ((out_c == NULL) || (lsm6ds3tr_c_read(dev, &sample) != 0))
    {
        return -1;
    }

    *out_c = sample.temp_c;

    return 0;
}

int lsm6ds3tr_c_read_reg(lsm6ds3tr_c_t *dev, uint8_t reg,
                         uint8_t *buf, uint16_t len)
{
    if ((dev == NULL) || (dev->bus == NULL) || (dev->bus->read == NULL) ||
            (buf == NULL) || (len == 0))
    {
        return -1;
    }

    return dev->bus->read(reg, buf, len);
}

int lsm6ds3tr_c_write_reg(lsm6ds3tr_c_t *dev, uint8_t reg,
                          const uint8_t *buf, uint16_t len)
{
    if ((dev == NULL) || (dev->bus == NULL) || (dev->bus->write == NULL) ||
            (buf == NULL) || (len == 0))
    {
        return -1;
    }

    return dev->bus->write(reg, buf, len);
}

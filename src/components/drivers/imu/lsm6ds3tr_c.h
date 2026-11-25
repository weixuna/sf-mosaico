/*
 * LSM6DS3TR-C 六轴 IMU 驱动
 *
 * 驱动只做寄存器级操作，不依赖 RT-Thread；总线读写和延时由板级
 * src/board/io/lsm6ds3tr_c_port.c 提供。
 */

#ifndef __LSM6DS3TR_C_H__
#define __LSM6DS3TR_C_H__

#include <stdint.h>
#include "lsm6ds3tr_c_regs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 7 位 I2C 地址（SA0 = 0） */
#define LSM6DS3TR_C_I2C_ADDR            (0x6A)

typedef struct
{
    float x;
    float y;
    float z;
} lsm6ds3tr_c_vec3_t;

typedef struct
{
    lsm6ds3tr_c_vec3_t accel_g;     /* 加速度，单位 g */
    lsm6ds3tr_c_vec3_t gyro_dps;    /* 角速度，单位 dps */
    float temp_c;                   /* 温度，单位摄氏度 */
    uint8_t valid;                  /* 1 = 本帧有效 */
} lsm6ds3tr_c_sample_t;

/* ODR 编码直接使用寄存器值，0x0B 仅加速度支持 */
typedef enum
{
    LSM6DS3TR_C_ODR_OFF = 0x00,
    LSM6DS3TR_C_ODR_12_5HZ = 0x01,
    LSM6DS3TR_C_ODR_26HZ = 0x02,
    LSM6DS3TR_C_ODR_52HZ = 0x03,
    LSM6DS3TR_C_ODR_104HZ = 0x04,
    LSM6DS3TR_C_ODR_208HZ = 0x05,
    LSM6DS3TR_C_ODR_416HZ = 0x06,
    LSM6DS3TR_C_ODR_833HZ = 0x07,
    LSM6DS3TR_C_ODR_1660HZ = 0x08,
    LSM6DS3TR_C_ODR_3330HZ = 0x09,
    LSM6DS3TR_C_ODR_6660HZ = 0x0A,
    LSM6DS3TR_C_ODR_1_6HZ = 0x0B,
} lsm6ds3tr_c_odr_t;

/* 加速度量程编码不是按顺序排列的 */
typedef enum
{
    LSM6DS3TR_C_ACCEL_2G = 0,
    LSM6DS3TR_C_ACCEL_16G = 1,
    LSM6DS3TR_C_ACCEL_4G = 2,
    LSM6DS3TR_C_ACCEL_8G = 3,
} lsm6ds3tr_c_accel_fs_t;

typedef enum
{
    LSM6DS3TR_C_GYRO_250DPS = 0,
    LSM6DS3TR_C_GYRO_125DPS = 1,
    LSM6DS3TR_C_GYRO_500DPS = 2,
    LSM6DS3TR_C_GYRO_1000DPS = 4,
    LSM6DS3TR_C_GYRO_2000DPS = 6,
} lsm6ds3tr_c_gyro_fs_t;

typedef struct
{
    lsm6ds3tr_c_odr_t accel_odr;
    lsm6ds3tr_c_accel_fs_t accel_fs;
    lsm6ds3tr_c_odr_t gyro_odr;
    lsm6ds3tr_c_gyro_fs_t gyro_fs;
    uint8_t accel_lpf2_en;
    uint8_t gyro_hp_en;
} lsm6ds3tr_c_config_t;

/* 与 Mosaico 对齐的默认配置：104 Hz / ±4 g / 104 Hz / ±2000 dps */
#define LSM6DS3TR_C_CONFIG_MOSAICO() {          \
    .accel_odr     = LSM6DS3TR_C_ODR_104HZ,     \
    .accel_fs      = LSM6DS3TR_C_ACCEL_4G,      \
    .gyro_odr      = LSM6DS3TR_C_ODR_104HZ,     \
    .gyro_fs       = LSM6DS3TR_C_GYRO_2000DPS,  \
    .accel_lpf2_en = 0,                         \
    .gyro_hp_en    = 0,                         \
}

/* 板级总线适配；map/board_init 可选 */
typedef struct
{
    int (*read)(uint8_t reg, uint8_t *buf, uint16_t len);
    int (*write)(uint8_t reg, const uint8_t *buf, uint16_t len);
    void (*delay_ms)(uint32_t ms);
    void (*map)(lsm6ds3tr_c_vec3_t *v);     /* 芯片坐标 -> 逻辑坐标 */
    int (*board_init)(void);                /* 传感器上电等板级初始化 */
} lsm6ds3tr_c_bus_t;

typedef struct
{
    uint8_t addr;
    const lsm6ds3tr_c_bus_t *bus;
    lsm6ds3tr_c_config_t config;
    float accel_lsb_to_g;
    float gyro_lsb_to_dps;
    uint16_t drdy_timeout_ms;
    uint8_t initialized;
    uint8_t started;
} lsm6ds3tr_c_t;

/*
 * 初始化：WHO_AM_I 校验、软复位、BDU/IF_INC、高性能模式。
 * bus 为 NULL 时使用板级 lsm6ds3tr_c_port_bus()。
 */
int lsm6ds3tr_c_init(lsm6ds3tr_c_t *dev, const lsm6ds3tr_c_bus_t *bus,
                     uint8_t addr);

/* 启动采样；config 为 NULL 时使用 LSM6DS3TR_C_CONFIG_MOSAICO() */
int lsm6ds3tr_c_start(lsm6ds3tr_c_t *dev,
                      const lsm6ds3tr_c_config_t *config);

/* 停止采样（ODR = 0），保留驱动实例 */
int lsm6ds3tr_c_stop(lsm6ds3tr_c_t *dev);

/* 停止并清除 initialized；不关闭共享的 SENS_3V3 */
int lsm6ds3tr_c_deinit(lsm6ds3tr_c_t *dev);

/* 一次读取同步的 accel + gyro + temp；失败时不修改 out */
int lsm6ds3tr_c_read(lsm6ds3tr_c_t *dev, lsm6ds3tr_c_sample_t *out);

int lsm6ds3tr_c_read_accel(lsm6ds3tr_c_t *dev, lsm6ds3tr_c_vec3_t *out_g);
int lsm6ds3tr_c_read_gyro(lsm6ds3tr_c_t *dev, lsm6ds3tr_c_vec3_t *out_dps);
int lsm6ds3tr_c_read_temp(lsm6ds3tr_c_t *dev, float *out_c);

/* 直接寄存器访问，返回 0 成功 / -1 失败 */
int lsm6ds3tr_c_read_reg(lsm6ds3tr_c_t *dev, uint8_t reg,
                         uint8_t *buf, uint16_t len);
int lsm6ds3tr_c_write_reg(lsm6ds3tr_c_t *dev, uint8_t reg,
                          const uint8_t *buf, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* __LSM6DS3TR_C_H__ */

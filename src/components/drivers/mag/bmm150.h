/*
 * BMM150 三轴磁力计驱动
 *
 * 驱动只做寄存器操作和官方补偿，不依赖 RT-Thread；I2C、电源和轴映射
 * 由板级 src/board/io/bmm150_port.c 提供。
 *
 * 两颗器件共用一条总线，所以 read/write/map 回调都带 7 位地址：
 *   0x11 -> 右侧
 *   0x12 -> 左侧
 */

#ifndef __BMM150_H__
#define __BMM150_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BMM150_I2C_ADDR_RIGHT   (0x11)
#define BMM150_I2C_ADDR_LEFT    (0x12)

typedef struct
{
    int16_t x;
    int16_t y;
    int16_t z;
} bmm150_vec3_t;

typedef struct
{
    bmm150_vec3_t field;    /* 补偿后的磁场，单位 uT */
    uint16_t rhall;         /* 原始霍尔电阻，调试用 */
    uint8_t valid;          /* 1 = 本帧有效 */
} bmm150_sample_t;

typedef struct
{
    int (*read)(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len);
    int (*write)(uint8_t addr, uint8_t reg, const uint8_t *buf, uint16_t len);
    void (*delay_ms)(uint32_t ms);
    void (*map)(uint8_t addr, bmm150_vec3_t *v);  /* 芯片坐标 -> 逻辑坐标 */
    int (*board_init)(void);                      /* 传感器上电 */
} bmm150_bus_t;

typedef struct
{
    uint8_t addr;
    const bmm150_bus_t *bus;
    int8_t dig_x1;
    int8_t dig_y1;
    int8_t dig_x2;
    int8_t dig_y2;
    int8_t dig_xy2;
    uint8_t dig_xy1;
    uint16_t dig_z1;
    uint16_t dig_xyz1;
    int16_t dig_z2;
    int16_t dig_z3;
    int16_t dig_z4;
    uint8_t initialized;
} bmm150_t;

/*
 * 初始化：上电、3 ms 启动、CHIP_ID 校验、读 trim、10 Hz regular 配置。
 * bus 为 NULL 时使用板级 bmm150_port_bus()。
 */
int bmm150_init(bmm150_t *dev, const bmm150_bus_t *bus, uint8_t addr);

/* 进入 suspend 并清除 initialized；不关闭共享的 SENS_3V3 */
int bmm150_deinit(bmm150_t *dev);

/* 读一帧补偿后的 XYZ；I2C 失败返回 -1，磁场溢出返回 0 且 valid = 0 */
int bmm150_read(bmm150_t *dev, bmm150_sample_t *out);

#ifdef __cplusplus
}
#endif

#endif /* __BMM150_H__ */

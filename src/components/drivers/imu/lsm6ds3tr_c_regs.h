/*
 * LSM6DS3TR-C 寄存器定义（只包含本驱动用到的部分）
 */

#ifndef __LSM6DS3TR_C_REGS_H__
#define __LSM6DS3TR_C_REGS_H__

/* 寄存器地址 */
#define LSM6DS3TR_C_REG_WHO_AM_I        0x0F
#define LSM6DS3TR_C_REG_CTRL1_XL        0x10
#define LSM6DS3TR_C_REG_CTRL2_G         0x11
#define LSM6DS3TR_C_REG_CTRL3_C         0x12
#define LSM6DS3TR_C_REG_CTRL6_C         0x15
#define LSM6DS3TR_C_REG_CTRL7_G         0x16
#define LSM6DS3TR_C_REG_CTRL8_XL        0x17
#define LSM6DS3TR_C_REG_STATUS          0x1E
#define LSM6DS3TR_C_REG_OUT_TEMP_L      0x20
#define LSM6DS3TR_C_REG_OUTX_L_G        0x22
#define LSM6DS3TR_C_REG_OUTX_L_XL       0x28

/* WHO_AM_I 固定值，注意它和 7 位 I2C 地址 0x6A 数值相同 */
#define LSM6DS3TR_C_WHO_AM_I_VALUE      0x6A

/* CTRL3_C */
#define LSM6DS3TR_C_CTRL3_C_SW_RESET    (1U << 0)
#define LSM6DS3TR_C_CTRL3_C_IF_INC      (1U << 2)
#define LSM6DS3TR_C_CTRL3_C_BDU         (1U << 6)

/* STATUS：加速度 / 陀螺仪数据就绪 */
#define LSM6DS3TR_C_STATUS_XLDA         (1U << 0)
#define LSM6DS3TR_C_STATUS_GDA          (1U << 1)

/* CTRL7_G：陀螺仪高通滤波使能 */
#define LSM6DS3TR_C_CTRL7_G_HP_EN       (1U << 6)

/* CTRL8_XL：加速度 LPF2 使能 */
#define LSM6DS3TR_C_CTRL8_XL_LPF2_EN    (1U << 7)

/* CTRL1_XL / CTRL2_G 字段位置 */
#define LSM6DS3TR_C_ODR_SHIFT           4
#define LSM6DS3TR_C_ACCEL_FS_SHIFT      2
#define LSM6DS3TR_C_GYRO_FS_SHIFT       1
#define LSM6DS3TR_C_CTRL2_G_FS_125      (1U << 0)

#endif /* __LSM6DS3TR_C_REGS_H__ */

/*
 * AW32001E battery charger driver
 *
 * SCL -> PA24, SDA -> PA25.  INT is not connected on this board.
 */

#ifndef __AW32001_H__
#define __AW32001_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AW32001_I2C_ADDRESS     0x49

/*
 * Bus/board adapter.  The driver only uses these callbacks; it has no
 * dependency on I2C2, GPIO numbers or TCA9555.
 */
typedef struct
{
    int (*read)(uint8_t reg, uint8_t *buf, uint16_t len);
    int (*write)(uint8_t reg, const uint8_t *buf, uint16_t len);
    int (*board_init)(void);   /* optional, may be NULL */
} aw32001_bus_t;

int aw32001_init(void);
int aw32001_read_reg(uint8_t reg, uint8_t *val);
int aw32001_write_reg(uint8_t reg, uint8_t val);
int aw32001_dump(void);

#ifdef __cplusplus
}
#endif

#endif /* __AW32001_H__ */

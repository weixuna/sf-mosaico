/*
 * Board IO switches on the TCA9555 (UMW TCA9555RTWR), address 0x20.
 *
 * Pin map (from schematic):
 *   P00 RF_BAND_SEL     P12 BOOST_EN
 *   P01 WF_PWRKEY       P13 VCI_EN
 *   P02 TF_DET (input)  P14 TP_RST
 *   P03 HST_WAK_WF      P15 MOTOR_EN
 *   P04 AUDIO_PA_EN     P16 RESET
 *   P05 MIC_PWR_EN      P17 LCD_PWR_EN
 *   P06 WF_PWR_EN       P10/P11 NC
 *   P07 SENS_PWR_EN
 */

#ifndef __BOARD_IO_H__
#define __BOARD_IO_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    BOARD_IO_RF_BAND_SEL = 0,   /* P00: 1 = 5G, 0 = 2.4G */
    BOARD_IO_WIFI_PWRKEY,       /* P01 */
    BOARD_IO_TF_DET,            /* P02, input */
    BOARD_IO_WIFI_HOST_WAKE,    /* P03 */
    BOARD_IO_AUDIO_PA_EN,       /* P04 */
    BOARD_IO_MIC_PWR_EN,        /* P05 */
    BOARD_IO_WIFI_PWR_EN,       /* P06 */
    BOARD_IO_SENSOR_PWR_EN,     /* P07 */
    BOARD_IO_BOOST_EN,          /* P12 */
    BOARD_IO_VCI_EN,            /* P13 */
    BOARD_IO_TP_RESET,          /* P14, active low */
    BOARD_IO_MOTOR_EN,          /* P15 */
    BOARD_IO_LCD_RESET,         /* P16, LCD reset, active low */
    BOARD_IO_LCD_PWR_EN,        /* P17 */
    BOARD_IO_COUNT
} board_io_id_t;

typedef enum
{
    BOARD_RF_BAND_2G4 = 0,
    BOARD_RF_BAND_5G  = 1,
} board_rf_band_t;

/* Initialize I2C2 + TCA9555 and apply default switch states. */
int board_io_init(void);

/* Generic switch access. "on" means the function is active. */
int board_io_set(board_io_id_t id, uint8_t on);
int board_io_get(board_io_id_t id, uint8_t *on);

/*
 * Turn off all power enables but keep the pins marked keep_on active
 * (P07 sensor power, P12 boost, P13 VCI, P17 LCD power by default).
 * Reset pins are released, not asserted.
 */
int board_io_all_off(void);

/*
 * Force all TCA9555 outputs low, including P07/P13/P17.
 * The I2C bus may disappear after P07 is driven low.
 */
int board_io_all_low(void);

/* Print current switch states. */
void board_io_dump(void);

/* One function per switch. */
int board_rf_band_select(board_rf_band_t band);
int board_wifi_pwrkey(uint8_t on);
int board_wifi_pwrkey_pulse(uint32_t ms);
int board_tf_det_get(uint8_t *inserted);
int board_wifi_host_wake(uint8_t on);
int board_audio_pa_enable(uint8_t on);
int board_mic_power_enable(uint8_t on);
int board_wifi_power_enable(uint8_t on);
int board_sensor_power_enable(uint8_t on);
int board_boost_enable(uint8_t on);
int board_vci_enable(uint8_t on);
int board_tp_reset(uint8_t assert);
int board_motor_enable(uint8_t on);
int board_lcd_reset(uint8_t assert);
int board_periph_reset(uint8_t assert);   /* backward-compatible alias */
int board_lcd_power_enable(uint8_t on);

#ifdef __cplusplus
}
#endif

#endif /* __BOARD_IO_H__ */

/*
 * Board IO switches on the TCA9555 (UMW TCA9555RTWR), address 0x20.
 *
 * Porting:
 *   1. Change the table below for the new board.
 *   2. Keep tca9555.c/h unchanged.
 *   3. Replace tca9555_port.c with the local I2C adapter.
 */

#include <rtthread.h>
#include "i2c2.h"
#include "tca9555.h"
#include "tca9555_port.h"
#include "board_io.h"
#include "drv_io.h"

#define DBG_TAG "board.io"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

/* P02 TF_DET: 0 = card inserted, 1 = no card. Change if the socket is opposite. */
#define BOARD_TF_DET_INSERTED_LEVEL     (0U)

/*
 * TCA9555 pin index used by tca9555.c: 0..15 = P00..P07, P10..P17.
 * Use (port, bit) so P16/P17 are not confused with linear index 16/17.
 */
#define IO_PIN(port, bit)               ((uint8_t)(((port) << 3) | (bit)))

typedef struct
{
    const char *name;
    uint8_t pin;
    uint8_t active_high;    /* 1: on = high, 0: on = low */
    uint8_t is_input;
    uint8_t default_on;
    uint8_t keep_on;        /* board_io_all_off keeps this switch on */
    uint8_t no_all_off;     /* board_io_all_off leaves this pin unchanged */
} board_io_cfg_t;

static const board_io_cfg_t board_io_cfg[BOARD_IO_COUNT] =
{
    [BOARD_IO_RF_BAND_SEL]    = {"RF_BAND_SEL",     IO_PIN(0, 0), 1, 0, 0, 0, 1},
    [BOARD_IO_WIFI_PWRKEY]    = {"WIFI_PWRKEY",     IO_PIN(0, 1), 1, 0, 0, 0, 0},
    [BOARD_IO_TF_DET]         = {"TF_DET",          IO_PIN(0, 2), BOARD_TF_DET_INSERTED_LEVEL, 1, 0, 0, 1},
    [BOARD_IO_WIFI_HOST_WAKE] = {"WIFI_HOST_WAKE",  IO_PIN(0, 3), 1, 0, 0, 0, 0},
    [BOARD_IO_AUDIO_PA_EN]    = {"AUDIO_PA_EN",     IO_PIN(0, 4), 1, 0, 0, 0, 0},
    [BOARD_IO_MIC_PWR_EN]     = {"MIC_PWR_EN",      IO_PIN(0, 5), 1, 0, 0, 0, 0},
    [BOARD_IO_WIFI_PWR_EN]    = {"WIFI_PWR_EN",     IO_PIN(0, 6), 1, 0, 0, 0, 0},
    [BOARD_IO_SENSOR_PWR_EN]  = {"SENSOR_PWR_EN",   IO_PIN(0, 7), 1, 0, 1, 1, 0},
    [BOARD_IO_BOOST_EN]       = {"BOOST_EN",        IO_PIN(1, 2), 1, 0, 0, 1, 0},
    [BOARD_IO_VCI_EN]         = {"VCI_EN",          IO_PIN(1, 3), 1, 0, 1, 1, 0},
    [BOARD_IO_TP_RESET]       = {"TP_RESET",        IO_PIN(1, 4), 0, 0, 0, 0, 0},
    [BOARD_IO_MOTOR_EN]       = {"MOTOR_EN",        IO_PIN(1, 5), 1, 0, 0, 0, 0},
    [BOARD_IO_LCD_RESET]      = {"LCD_RESET",       IO_PIN(1, 6), 0, 0, 0, 0, 0},
    [BOARD_IO_LCD_PWR_EN]     = {"LCD_PWR_EN",      IO_PIN(1, 7), 1, 0, 1, 1, 0},
};

static rt_bool_t board_io_ready = RT_FALSE;

static uint8_t board_io_active_level(const board_io_cfg_t *cfg, uint8_t on)
{
    if (cfg->active_high)
    {
        return on ? 1U : 0U;
    }

    return on ? 0U : 1U;
}

int board_io_init(void)
{
    tca9555_t *dev;
    uint8_t i;
    uint8_t failed = 0;

    if (board_io_ready)
    {
        return 0;
    }

    if (i2c2_init() != 0)
    {
        LOG_E("I2C2 init failed");
        return -1;
    }

    if (tca9555_port_init() != 0)
    {
        LOG_E("port init failed");
        return -1;
    }

    dev = tca9555_port_get();
    if (tca9555_init(dev) != 0)
    {
        LOG_E("no TCA9555 at 0x%02x", (unsigned)TCA9555_I2C_ADDR);
        return -1;
    }

    /* P10/P11 are not connected: keep them as inputs. */
    if ((tca9555_pin_config(dev, IO_PIN(1, 0), 1, 0) != 0) ||
            (tca9555_pin_config(dev, IO_PIN(1, 1), 1, 0) != 0))
    {
        LOG_E("configure NC pins failed");
        return -1;
    }

    /* Apply the default state of every board switch. */
    for (i = 0; i < BOARD_IO_COUNT; i++)
    {
        const board_io_cfg_t *cfg = &board_io_cfg[i];
        uint8_t level = board_io_active_level(cfg, cfg->default_on);

        if (cfg->is_input)
        {
            if (tca9555_pin_config(dev, cfg->pin, 1, 0) != 0)
            {
                LOG_E("configure %s failed", cfg->name);
                failed++;
            }
        }
        else
        {
            if (tca9555_pin_config(dev, cfg->pin, 0, level) != 0)
            {
                LOG_E("configure %s failed", cfg->name);
                failed++;
            }
        }
    }

    board_io_ready = RT_TRUE;
    LOG_I("TCA9555 ready, %d switches, %d failed",
          (int)BOARD_IO_COUNT, failed);

    return 0;
}

int board_io_set(board_io_id_t id, uint8_t on)
{
    const board_io_cfg_t *cfg;

    if (id >= BOARD_IO_COUNT)
    {
        return -1;
    }

    if (!board_io_ready && board_io_init() != 0)
    {
        return -1;
    }

    cfg = &board_io_cfg[id];
    if (cfg->is_input)
    {
        return -1;
    }

    return tca9555_pin_write(tca9555_port_get(), cfg->pin,
                             board_io_active_level(cfg, on));
}

int board_io_get(board_io_id_t id, uint8_t *on)
{
    const board_io_cfg_t *cfg;
    tca9555_t *dev;
    uint8_t level;

    if ((id >= BOARD_IO_COUNT) || (on == RT_NULL))
    {
        return -1;
    }

    if (!board_io_ready && board_io_init() != 0)
    {
        return -1;
    }

    cfg = &board_io_cfg[id];
    dev = tca9555_port_get();

    if (cfg->is_input)
    {
        if (tca9555_pin_read(dev, cfg->pin, &level) != 0)
        {
            return -1;
        }

        *on = (level == (cfg->active_high ? 1U : 0U)) ? 1U : 0U;
    }
    else
    {
        uint16_t outputs;

        if (tca9555_get_outputs(dev, &outputs) != 0)
        {
            return -1;
        }

        level = (uint8_t)((outputs >> cfg->pin) & 0x01);
        *on = cfg->active_high ? level : (level ? 0U : 1U);
    }

    return 0;
}

static int board_io_apply_all(uint8_t force_low)
{
    tca9555_t *dev;
    uint8_t i;

    if (!board_io_ready && board_io_init() != 0)
    {
        return -1;
    }

    dev = tca9555_port_get();

    /* Port 1 first: P12..P17. */
    for (i = 0; i < BOARD_IO_COUNT; i++)
    {
        const board_io_cfg_t *cfg = &board_io_cfg[i];
        uint8_t level;

        if (cfg->is_input || (cfg->pin < 8))
        {
            continue;
        }

        if (!force_low && cfg->no_all_off)
        {
            continue;
        }

        if (force_low)
        {
            level = 0;
        }
        else
        {
            level = board_io_active_level(cfg, cfg->keep_on ? 1U : 0U);
        }

        if (tca9555_pin_write(dev, cfg->pin, level) != 0)
        {
            return -1;
        }
    }

    /* Port 0, except P07 which is done last. */
    for (i = 0; i < BOARD_IO_COUNT; i++)
    {
        const board_io_cfg_t *cfg = &board_io_cfg[i];
        uint8_t level;

        if (cfg->is_input || (cfg->pin >= 8) || (cfg->pin == 7))
        {
            continue;
        }

        if (!force_low && cfg->no_all_off)
        {
            continue;
        }

        if (force_low)
        {
            level = 0;
        }
        else
        {
            level = board_io_active_level(cfg, cfg->keep_on ? 1U : 0U);
        }

        if (tca9555_pin_write(dev, cfg->pin, level) != 0)
        {
            return -1;
        }
    }

    /* P07 last: it may power down the sensor rail / I2C pull-ups. */
    {
        const board_io_cfg_t *cfg = &board_io_cfg[BOARD_IO_SENSOR_PWR_EN];
        uint8_t level;

        if (force_low)
        {
            level = 0;
        }
        else
        {
            level = board_io_active_level(cfg, cfg->keep_on ? 1U : 0U);
        }

        if (tca9555_pin_write(dev, cfg->pin, level) != 0)
        {
            return -1;
        }
    }

    return 0;
}

int board_io_all_off(void)
{
    return board_io_apply_all(0);
}

int board_io_all_low(void)
{
    return board_io_apply_all(1);
}

void board_io_dump(void)
{
    uint8_t i;

    if (!board_io_ready && board_io_init() != 0)
    {
        rt_kprintf("[BOARD IO] TCA9555 not ready\n");
        return;
    }

    for (i = 0; i < BOARD_IO_COUNT; i++)
    {
        const board_io_cfg_t *cfg = &board_io_cfg[i];
        uint8_t on = 0;

        if (board_io_get((board_io_id_t)i, &on) != 0)
        {
            rt_kprintf("  %-15s P%u%u  read failed\n", cfg->name,
                       (unsigned)(cfg->pin >> 3), (unsigned)(cfg->pin & 0x07));
        }
        else
        {
            rt_kprintf("  %-15s P%u%u  %s\n", cfg->name,
                       (unsigned)(cfg->pin >> 3), (unsigned)(cfg->pin & 0x07),
                       on ? "ON" : "OFF");
        }
    }
}

int board_rf_band_select(board_rf_band_t band)
{
    if ((band != BOARD_RF_BAND_2G4) && (band != BOARD_RF_BAND_5G))
    {
        return -1;
    }

    return board_io_set(BOARD_IO_RF_BAND_SEL, (band == BOARD_RF_BAND_5G) ? 1U : 0U);
}

int board_wifi_pwrkey(uint8_t on)
{
    return board_io_set(BOARD_IO_WIFI_PWRKEY, on);
}

int board_wifi_pwrkey_pulse(uint32_t ms)
{
    int ret;

    ret = board_wifi_pwrkey(1);
    if (ret != 0)
    {
        return ret;
    }

    tca9555_port_delay_ms(ms);

    return board_wifi_pwrkey(0);
}

int board_tf_det_get(uint8_t *inserted)
{
    return board_io_get(BOARD_IO_TF_DET, inserted);
}

int board_wifi_host_wake(uint8_t on)
{
    return board_io_set(BOARD_IO_WIFI_HOST_WAKE, on);
}

int board_audio_pa_enable(uint8_t on)
{
    return board_io_set(BOARD_IO_AUDIO_PA_EN, on);
}

int board_mic_power_enable(uint8_t on)
{
    return board_io_set(BOARD_IO_MIC_PWR_EN, on);
}

int board_wifi_power_enable(uint8_t on)
{
    return board_io_set(BOARD_IO_WIFI_PWR_EN, on);
}

int board_sensor_power_enable(uint8_t on)
{
    return board_io_set(BOARD_IO_SENSOR_PWR_EN, on);
}

int board_boost_enable(uint8_t on)
{
    return board_io_set(BOARD_IO_BOOST_EN, on);
}

int board_vci_enable(uint8_t on)
{
    return board_io_set(BOARD_IO_VCI_EN, on);
}

int board_tp_reset(uint8_t assert)
{
    return board_io_set(BOARD_IO_TP_RESET, assert);
}

/*
 * Strong override of the weak board hook.  BSP_TP_Reset(1) drives the reset
 * line high (released), BSP_TP_Reset(0) drives it low (asserted), while
 * board_tp_reset(assert) uses assert = 1 for the active-low reset.
 */
void BSP_TP_ResetBoard(uint8_t high1_low0)
{
    (void)board_tp_reset(high1_low0 ? 0U : 1U);
}

int board_motor_enable(uint8_t on)
{
    return board_io_set(BOARD_IO_MOTOR_EN, on);
}

int board_lcd_reset(uint8_t assert)
{
    return board_io_set(BOARD_IO_LCD_RESET, assert);
}

int board_periph_reset(uint8_t assert)
{
    return board_lcd_reset(assert);
}

int board_lcd_power_enable(uint8_t on)
{
    return board_io_set(BOARD_IO_LCD_PWR_EN, on);
}

static void board_io_dump_shell(void)
{
    board_io_dump();
}
MSH_CMD_EXPORT_ALIAS(board_io_dump_shell, board_io_dump, dump board IO states);

static void board_io_off_shell(void)
{
    if (board_io_all_off() == 0)
    {
        rt_kprintf("[BOARD IO] all off (keep_on respected)\n");
    }
    else
    {
        rt_kprintf("[BOARD IO] all off failed\n");
    }
}
MSH_CMD_EXPORT_ALIAS(board_io_off_shell, board_io_off, turn board IO off);

static void board_io_all_low_shell(void)
{
    if (board_io_all_low() == 0)
    {
        rt_kprintf("[BOARD IO] all outputs LOW\n");
    }
    else
    {
        rt_kprintf("[BOARD IO] all low failed\n");
    }
}
MSH_CMD_EXPORT_ALIAS(board_io_all_low_shell, board_io_all_low,
                     force all TCA9555 outputs low);

static void tca9555_off_shell(void)
{
    board_io_all_low();
}
MSH_CMD_EXPORT_ALIAS(tca9555_off_shell, tca9555_off,
                     force all TCA9555 outputs low);

/* Apply the board defaults during RT-Thread app init. If the application
   already called board_io_init(), this becomes a no-op. */
INIT_APP_EXPORT(board_io_init);


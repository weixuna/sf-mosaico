/*
 * AW32001E battery charger driver
 *
 * Board wiring:
 *   SCL  -> PA24 (I2C2_SCL)
 *   SDA  -> PA25 (I2C2_SDA)
 *   INT  -> not connected
 *
 * The AW32001E is a single-cell linear charger with power path.  Two
 * defaults are wrong for this board:
 *
 *   REG06[7] EN_NTC = 1 : the NTC pin only has a fixed 100k to GND, which
 *                        reads as an NTC fault and can trigger the
 *                        charger's tRST_DGL/tRST_DUR battery disconnect.
 *
 *   REG05[6:5] WATCHDOG = 11 : 160 s watchdog, enabled by default in
 *                        charge mode.  When it expires the charger turns
 *                        Q1/Q2 off and resets every register, so VSYS
 *                        collapses and the whole chip looks reset.
 *
 * init disables NTC and the watchdog, enables charging, sets the input
 * current limit to 550 mA, discharge limit to 3.2 A, and charge current to
 * 128 mA, then clears the latched faults by reading REG09.
 *
 * A charger POR (battery inserted/removed, VBUS plugged) restores the
 * defaults, so the poll thread verifies REG05/REG06/REG01 and re-applies
 * the configuration.  The thread also re-probes the charger if the first
 * probe failed before the sensor rail / I2C pull-ups were powered.
 */

#include <rtthread.h>
#include <rtdevice.h>

#include "aw32001.h"

#define DBG_TAG "sensor.aw32001"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#define AW32001_REG_SOURCE_CTRL     0x00
#define AW32001_REG_POWERON_CONF    0x01
#define AW32001_REG_CHG_CURRENT     0x02
#define AW32001_REG_DISCHG_CURRENT  0x03
#define AW32001_REG_CHG_VOLTAGE     0x04
#define AW32001_REG_TIMER_CTRL      0x05
#define AW32001_REG_MAIN_CTRL       0x06
#define AW32001_REG_VOLT_CTRL       0x07
#define AW32001_REG_SYS_STATUS      0x08
#define AW32001_REG_FAULT           0x09
#define AW32001_REG_ADDRESS         0x0A
#define AW32001_REG_INDIVIDUAL_CHG  0x0B
#define AW32001_REG_ADD_FUNC_0      0x0C
#define AW32001_REG_ADD_FUNC_1      0x22

#define AW32001_REG05_WDT_MASK      0x60U   /* REG05[6:5] WATCHDOG */
#define AW32001_REG06_NTC_MASK      0x80U   /* REG06[7] EN_NTC */
#define AW32001_REG01_EXPECTED      0xC4U   /* tRST 20s/2s, charge on, 2.76V */
#define AW32001_POLL_MS             500U
#define AW32001_REPROBE_MS          2000U

static const aw32001_bus_t *s_bus;
static rt_bool_t aw32001_present = RT_FALSE;

/* Implemented by the board adapter (src/board/power/aw32001_port.c). */
RT_WEAK const aw32001_bus_t *aw32001_port_bus(void)
{
    return RT_NULL;
}

static int aw32001_rd(uint8_t reg, uint8_t *val)
{
    return s_bus->read(reg, val, 1);
}

static int aw32001_wr(uint8_t reg, uint8_t val)
{
    return s_bus->write(reg, &val, 1);
}

static int aw32001_update(uint8_t reg, uint8_t mask, uint8_t val)
{
    uint8_t old;

    if (aw32001_rd(reg, &old) != 0)
    {
        return -1;
    }
    old = (uint8_t)((old & ~mask) | (val & mask));
    return aw32001_wr(reg, old);
}

/*
 * REG05[6:5] defaults to 11 (160 s) and the watchdog is enabled in charge
 * mode.  Clear both bits, then read them back: a silently failed write here
 * is not visible until the charger resets the whole system 160 s later.
 * Bit[6:5] = 00 disables the timer regardless of REG05[7].
 */
static int aw32001_wdt_disable(void)
{
    uint8_t v = 0;
    int i;

    for (i = 0; i < 3; i++)
    {
        if ((aw32001_update(AW32001_REG_TIMER_CTRL,
                            AW32001_REG05_WDT_MASK, 0) == 0) &&
                (aw32001_rd(AW32001_REG_TIMER_CTRL, &v) == 0) &&
                ((v & AW32001_REG05_WDT_MASK) == 0))
        {
            return 0;
        }
        rt_thread_mdelay(2);
    }

    return -1;
}

int aw32001_read_reg(uint8_t reg, uint8_t *val)
{
    return aw32001_rd(reg, val);
}

int aw32001_write_reg(uint8_t reg, uint8_t val)
{
    return aw32001_wr(reg, val);
}

static void aw32001_print_regs(void)
{
    static const uint8_t regs[] =
    {
        AW32001_REG_SOURCE_CTRL, AW32001_REG_POWERON_CONF,
        AW32001_REG_CHG_CURRENT, AW32001_REG_DISCHG_CURRENT,
        AW32001_REG_CHG_VOLTAGE, AW32001_REG_TIMER_CTRL,
        AW32001_REG_MAIN_CTRL, AW32001_REG_VOLT_CTRL,
        AW32001_REG_SYS_STATUS, AW32001_REG_FAULT,
        AW32001_REG_ADDRESS, AW32001_REG_INDIVIDUAL_CHG,
        AW32001_REG_ADD_FUNC_0,
    };
    char line[96];
    int n = 0;
    rt_size_t i;

    for (i = 0; i < sizeof(regs); i++)
    {
        uint8_t v = 0;
        if (aw32001_rd(regs[i], &v) == 0)
        {
            n += rt_snprintf(line + n, sizeof(line) - n, "%02X:%02X ", regs[i], v);
        }
    }
    LOG_I("regs %s", line);
}

int aw32001_dump(void)
{
    if (!aw32001_present)
    {
        return -1;
    }
    aw32001_print_regs();
    return 0;
}

static void aw32001_status_log(void)
{
    uint8_t sys = 0, fault = 0;

    if (aw32001_rd(AW32001_REG_SYS_STATUS, &sys) != 0)
    {
        LOG_W("read SYS_STATUS failed");
        return;
    }
    if (aw32001_rd(AW32001_REG_FAULT, &fault) != 0)
    {
        LOG_W("read FAULT failed");
        return;
    }

    LOG_I("SYS=0x%02x WDT=%d CHG=%d PPM=%d PG=%d THERM=%d | FAULT=0x%02x "
          "VIN=%d THERM_SD=%d BAT=%d STMR=%d NTC=%d",
          sys, (sys >> 7) & 1,
          (sys >> 3) & 0x03, (sys >> 2) & 1, (sys >> 1) & 1, sys & 1,
          fault,
          (fault >> 5) & 1, (fault >> 4) & 1, (fault >> 3) & 1,
          (fault >> 2) & 1, fault & 0x03);
}

/*
 * Re-apply every charger register.  Called at init and whenever the poll
 * thread notices that the chip has reset itself.
 */
static int aw32001_apply_config(void)
{
    int err = 0;
    uint8_t sys = 0, fault = 0;

    /* 1. Watchdog off (REG05[6:5] = 00).  It defaults to 160 s and is
       enabled in charge mode; on expiry Q1/Q2 turn off and every register
       returns to its default, which collapses VSYS. */
    if (aw32001_wdt_disable() != 0)
    {
        LOG_W("disable watchdog failed");
        err++;
    }

    /* 2. NTC off: the board only has a fixed 100k on the NTC pin. */
    if (aw32001_update(AW32001_REG_MAIN_CTRL, AW32001_REG06_NTC_MASK, 0) != 0)
    {
        LOG_W("disable NTC failed");
        err++;
    }

    /* 3. Enable charging, and make the fault-triggered battery disconnect
       as harmless as possible:
         tRST_DGL = 11 (20 s, longest)
         tRST_DUR = 0  (2 s, shortest)
         EN_HIZ   = 0
         CEB      = 0  (charge enable)
         VBAT_UVLO= 100 (2.76 V)
       REG01 = 1100 0100 = 0xC4. */
    if (aw32001_wr(AW32001_REG_POWERON_CONF, AW32001_REG01_EXPECTED) != 0)
    {
        LOG_W("enable charge failed");
        err++;
    }

    /* 4. Input current limit 550 mA (REG00=0x8F + EN0P55), VIN_DPM 4.52 V. */
    if (aw32001_wr(AW32001_REG_SOURCE_CTRL, 0x8F) != 0)
    {
        err++;
    }
    if (aw32001_update(AW32001_REG_ADD_FUNC_0, 0x80, 0x80) != 0)
    {
        err++;
    }

    /* 4b. VSYS_REG = 4.95 V.  The charger is linear, so a higher VSYS
       reduces the Q1 drop: (5.0-4.95) instead of (5.0-4.6).  This cuts the
       pass-FET dissipation, which is what drives the thermal shutdown
       seen ~15-20 s into a long flash write. */
    if (aw32001_update(AW32001_REG_VOLT_CTRL, 0x0F, 0x0F) != 0)
    {
        err++;
    }

    /* 5. Charge current 128 mA: REG02[5:0] = 15.  Keep it moderate: the
       linear charge path (VSYS 4.95 V -> VBAT 4.2 V) is the other heat
       source inside the WLCSP. */
    if (aw32001_update(AW32001_REG_CHG_CURRENT, 0x3F, 15) != 0)
    {
        err++;
    }

    /* 6. Battery regulation 4.20 V: REG04[7:2] = 40. */
    if (aw32001_update(AW32001_REG_CHG_VOLTAGE, 0xFC, (40 << 2)) != 0)
    {
        err++;
    }

    /* 7. Discharge limit 3.2 A, termination current 3 mA. */
    if (aw32001_wr(AW32001_REG_DISCHG_CURRENT, 0xF1) != 0)
    {
        err++;
    }

    /* 8. Clear latched faults (REG09 is read-clear). */
    aw32001_rd(AW32001_REG_SYS_STATUS, &sys);
    aw32001_rd(AW32001_REG_FAULT, &fault);

    return (err == 0) ? 0 : -1;
}

/*
 * Verify the registers that a charger POR would change.
 *   0: configuration intact
 *   1: configuration was lost and has been re-applied
 *  -1: I2C error or repair failed
 */
static int aw32001_check_and_repair(void)
{
    uint8_t timer = 0, main_ctrl = 0, poweron = 0;

    if ((aw32001_rd(AW32001_REG_TIMER_CTRL, &timer) != 0) ||
            (aw32001_rd(AW32001_REG_MAIN_CTRL, &main_ctrl) != 0) ||
            (aw32001_rd(AW32001_REG_POWERON_CONF, &poweron) != 0))
    {
        return -1;
    }

    if (((timer & AW32001_REG05_WDT_MASK) == 0) &&
            ((main_ctrl & AW32001_REG06_NTC_MASK) == 0) &&
            (poweron == AW32001_REG01_EXPECTED))
    {
        return 0;
    }

    LOG_W("charger reset (05=%02x 06=%02x 01=%02x), re-applying",
          timer, main_ctrl, poweron);

    return (aw32001_apply_config() == 0) ? 1 : -1;
}

static int aw32001_probe_and_config(void)
{
    uint8_t id = 0;

    if (aw32001_rd(AW32001_REG_ADDRESS, &id) != 0)
    {
        return -1;
    }

    if (id != AW32001_I2C_ADDRESS)
    {
        LOG_W("unexpected chip id 0x%02x", id);
        return -1;
    }

    aw32001_present = RT_TRUE;

    if (aw32001_apply_config() == 0)
    {
        LOG_I("AW32001 ready, chip id 0x%02x", id);
        aw32001_status_log();
        aw32001_print_regs();
        return 0;
    }

    /* Keep retrying the whole sequence rather than hiding a failed write:
       one missed REG05 write is enough for the 160 s watchdog to fire. */
    LOG_W("AW32001 configuration incomplete, will retry");
    aw32001_present = RT_FALSE;

    return -1;
}

static void aw32001_poll_thread(void *arg)
{
    uint8_t sys, fault;
    int comm_fail = 0;
    rt_tick_t next_probe = 0;

    (void)arg;

    while (1)
    {
        rt_thread_mdelay(AW32001_POLL_MS);

        /* The first probe can fail while the sensor rail / I2C pull-ups are
           still settling.  Keep probing instead of giving up forever. */
        if (!aw32001_present)
        {
            if (rt_tick_get() >= next_probe)
            {
                if (aw32001_probe_and_config() == 0)
                {
                    comm_fail = 0;
                }
                else
                {
                    next_probe = rt_tick_get() +
                                 rt_tick_from_millisecond(AW32001_REPROBE_MS);
                }
            }
            continue;
        }

        /* A charger POR (battery/VBUS) restores REG05[6:5]=11 and NTC on.
           Leaving it alone means a reset after 160 s (watchdog) plus an NTC
           fault that can trigger the battery disconnect.  Repair before the
           watchdog fires. */
        switch (aw32001_check_and_repair())
        {
        case 0:
            comm_fail = 0;
            break;
        case 1:
            comm_fail = 0;
            break;
        default:
            if (++comm_fail >= 5)
            {
                LOG_W("AW32001 lost, will re-probe");
                aw32001_present = RT_FALSE;
                comm_fail = 0;
                next_probe = 0;
            }
            continue;
        }

        if (aw32001_rd(AW32001_REG_SYS_STATUS, &sys) != 0 ||
                aw32001_rd(AW32001_REG_FAULT, &fault) != 0)
        {
            if (++comm_fail >= 5)
            {
                LOG_W("i2c read failed, will re-probe");
                aw32001_present = RT_FALSE;
                comm_fail = 0;
                next_probe = 0;
            }
            continue;
        }
        comm_fail = 0;

        /* Thermal regulation or thermal shutdown: drop the charge current
           to 8 mA before the charger reaches its 150 C shutdown. */
        if ((sys & 0x01) || (fault & 0x10))
        {
            static rt_bool_t thermal_reduced = RT_FALSE;
            if (!thermal_reduced)
            {
                aw32001_update(AW32001_REG_CHG_CURRENT, 0x3F, 0);
                thermal_reduced = RT_TRUE;
                LOG_W("thermal: charge current reduced to 8 mA");
            }
        }
    }
}

int aw32001_init(void)
{
    s_bus = aw32001_port_bus();
    if (!s_bus || !s_bus->read || !s_bus->write)
    {
        LOG_E("AW32001 port not registered");
        return -1;
    }

    /*
     * Board adapter: I2C2, sensor rail and default load states.  The
     * charger is not accessible until SENS_3V3 is on.
     */
    if (s_bus->board_init && (s_bus->board_init() != 0))
    {
        LOG_W("AW32001 board init failed, poll thread will retry");
    }

    if (aw32001_probe_and_config() != 0)
    {
        LOG_W("AW32001 not ready at 0x%02x, poll thread will retry",
              AW32001_I2C_ADDRESS);
    }

    {
        rt_thread_t tid = rt_thread_create("aw32001",
                                           aw32001_poll_thread, RT_NULL,
                                           2048, 20, 10);
        if (tid != RT_NULL)
        {
            rt_thread_startup(tid);
        }
        else
        {
            LOG_W("poll thread create failed");
        }
    }

    return 0;
}

static void aw32001_repair_shell(void)
{
    if (aw32001_probe_and_config() != 0)
    {
        rt_kprintf("[AW32001] not ready at 0x%02x, retry later\n",
                   AW32001_I2C_ADDRESS);
        return;
    }

    rt_kprintf("[AW32001] config re-applied, watchdog disabled\n");
    aw32001_print_regs();
}
MSH_CMD_EXPORT_ALIAS(aw32001_repair_shell, aw32001_repair,
                     re-apply AW32001 config and disable its watchdog);

static void aw32001_dump_shell(void)
{
    if (aw32001_dump() != 0)
    {
        rt_kprintf("[AW32001] not present\n");
    }
}
MSH_CMD_EXPORT_ALIAS(aw32001_dump_shell, aw32001_dump, dump AW32001 registers);

INIT_APP_EXPORT(aw32001_init);

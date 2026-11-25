/*
 * SPDX-FileCopyrightText: 2026 SiFli Technologies(Nanjing) Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rtthread.h>

#include "i2c2.h"
#include "board_io.h"
#include "aw32001.h"
#include "aw32001_port.h"

#define DBG_TAG "board.aw32001"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

static int port_read(uint8_t reg, uint8_t *buf, uint16_t len)
{
    return i2c2_read_reg(AW32001_I2C_ADDRESS, reg, buf, len);
}

static int port_write(uint8_t reg, const uint8_t *buf, uint16_t len)
{
    return i2c2_write_reg(AW32001_I2C_ADDRESS, reg, buf, len);
}

static int port_board_init(void)
{
    if (board_io_init() != 0)
    {
        LOG_W("board IO init failed");
        return -1;
    }

    /*
     * Enable the sensor rail / I2C pull-ups and turn off the loads that
     * the TCA9555 powers up by default.  P07 must stay high: it enables
     * SENS_3V3, which also feeds the PA24/PA25 pull-ups.
     */
    board_io_set(BOARD_IO_SENSOR_PWR_EN, 1);
    board_io_set(BOARD_IO_BOOST_EN, 0);
    board_io_set(BOARD_IO_WIFI_PWR_EN, 0);
    board_io_set(BOARD_IO_AUDIO_PA_EN, 0);
    board_io_set(BOARD_IO_MIC_PWR_EN, 0);

    LOG_I("sensor on, boost/wifi/audio/mic off");
    return 0;
}

static const aw32001_bus_t s_bus =
{
    .read = port_read,
    .write = port_write,
    .board_init = port_board_init,
};

const aw32001_bus_t *aw32001_port_bus(void)
{
    return &s_bus;
}

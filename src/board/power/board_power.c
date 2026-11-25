/*
 * SPDX-FileCopyrightText: 2026 SiFli Technologies(Nanjing) Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rtthread.h>
#include "board_power.h"

#if defined(BSP_USING_I2C2) && !defined(SOC_SIMULATOR) && !defined(_MSC_VER)
    #include "board_io.h"

    int board_power_init(void)
    {
        if (board_io_init() != 0)
        {
            rt_kprintf("[BOARD] board IO init failed\n");
            return -1;
        }

        /* Power on the LCD rail, then pulse the panel reset. */
        board_lcd_power_enable(1);
        rt_thread_mdelay(20);

        board_lcd_reset(0);         /* high: idle */
        rt_thread_mdelay(5);

        board_lcd_reset(1);         /* low: reset */
        rt_thread_mdelay(10);

        board_lcd_reset(0);         /* high: release */
        rt_thread_mdelay(120);      /* CO5300 needs time after reset */

        return 0;
    }
#else
    int board_power_init(void)
    {
        return 0;
    }
#endif

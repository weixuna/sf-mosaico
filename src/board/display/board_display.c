/*
 * SPDX-FileCopyrightText: 2026 SiFli Technologies(Nanjing) Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <stdbool.h>
#include "littlevgl2rtt.h"
#include "board_display.h"

#ifndef _WIN32
    #include "drv_lcd.h"

    #if defined(DRV_EPIC_NEW_API)
        extern bool lv_refr_enable_full_buffer(bool enable);
    #endif

    static rt_device_t s_lcd;

    /*
     * The CO5300 init sets MADCTL MX (0x40) for the horizontal half.  The
     * vertical half is the LCDC V mirror, enabled by this control; it also
     * sets drv_lcd.rotate = 180 so set_window()/draw_core() flip the Y
     * window and area (required for partial refresh).
     *
     * POWERON resets the LCDC layer but leaves drv_lcd.rotate at 180, so on
     * resume toggle 0 -> 180 once to resync.
     */
    static void display_rotate_180(bool resume)
    {
        rt_device_t lcd = s_lcd ? s_lcd : rt_device_find("lcd");
        if (!lcd)
            return;

        if (resume)
            rt_device_control(lcd, RTGRAPHIC_CTRL_ROTATE_180, NULL);

        rt_device_control(lcd, RTGRAPHIC_CTRL_ROTATE_180, NULL);
    }

    static void touch_rotate_180(void)
    {
        struct rt_device_rect_info rect = {0, 0, 480, 480};
        rt_device_t tp = rt_device_find("touch");

        if (tp)
            rt_device_control(tp, RTGRAPHIC_CTRL_ROTATE_180, &rect);
    }
#endif /* !_WIN32 */

int board_display_init(const char *lcd_name)
{
    if (RT_EOK != littlevgl2rtt_init(lcd_name))
        return -1;

#ifndef _WIN32
    /*
     * Large page transitions update most of the screen while the LCDC is
     * scanning it.  The partial strip path can then show a moving seam or
     * leave stale bands.  Full-screen double buffering waits for the
     * previous buffer to be free before switching, which removes that race.
     */
    #if defined(DRV_EPIC_NEW_API)
        if (!lv_refr_enable_full_buffer(true))
            rt_kprintf("[BOARD] full-buffer mode not enabled\n");
    #endif

    s_lcd = rt_device_find(lcd_name);
    display_rotate_180(false);
    touch_rotate_180();
#endif

    return 0;
}

void board_display_resume(void)
{
#ifndef _WIN32
    display_rotate_180(true);
#endif
}

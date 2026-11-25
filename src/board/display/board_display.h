/*
 * SPDX-FileCopyrightText: 2026 SiFli Technologies(Nanjing) Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __BOARD_DISPLAY_H__
#define __BOARD_DISPLAY_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Initialize LVGL display/touch and apply the board's physical orientation.
 * Returns 0 on success, negative on failure.
 */
int board_display_init(const char *lcd_name);

/*
 * Re-apply the display orientation after the panel/LCDC was re-initialized
 * by RTGRAPHIC_CTRL_POWERON.
 */
void board_display_resume(void);

#ifdef __cplusplus
}
#endif

#endif /* __BOARD_DISPLAY_H__ */

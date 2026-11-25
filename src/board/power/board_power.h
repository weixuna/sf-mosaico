/*
 * SPDX-FileCopyrightText: 2026 SiFli Technologies(Nanjing) Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __BOARD_POWER_H__
#define __BOARD_POWER_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Initialize the board IO expander, power rails and release the LCD reset.
 * Returns 0 on success, negative on failure.
 */
int board_power_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __BOARD_POWER_H__ */

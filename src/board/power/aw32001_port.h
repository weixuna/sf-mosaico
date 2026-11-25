/*
 * SPDX-FileCopyrightText: 2026 SiFli Technologies(Nanjing) Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __AW32001_PORT_H__
#define __AW32001_PORT_H__

#include "aw32001.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Board adapter for the AW32001 charger driver (I2C2, INT pin, rails). */
const aw32001_bus_t *aw32001_port_bus(void);

#ifdef __cplusplus
}
#endif

#endif /* __AW32001_PORT_H__ */

/* SPDX-License-Identifier: GPL-2.0-only
 * SPDX-FileCopyrightText: 2023 KUNBUS GmbH
 *
 * RevPi RO module (Relais Output)
 */

#ifndef _REVPI_RO_H_
#define _REVPI_RO_H_

#include <linux/types.h>
#include "piControl.h"

int revpi_ro_init(unsigned int devnum);
void revpi_ro_reset(void);
int revpi_ro_config(u8 addr, int num_entries, SEntryInfo *pEnt);
int revpi_ro_cycle(unsigned int devnum);

#endif /* REVPI_RO_H_ */

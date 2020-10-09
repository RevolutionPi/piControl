/*
 * Copyright (C) 2020 KUNBUS GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */
#ifndef _REVPI_FLAT_H
#define _REVPI_FLAT_H

#include <linux/types.h>
#include "piControl.h"

int revpi_flat_init(void);
void revpi_flat_fini(void);
int revpi_flat_reset(void);
void revpi_flat_config(u8 addr, u16 num_entries, SEntryInfo *pEnt);

#endif /* _REVPI_FLAT_H */

/*
 * Copyright (C) 2023 KUNBUS GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */
#ifndef _REVPI_RO_H_
#define _REVPI_RO_H_

#include <linux/types.h>
#include <IoProtocol.h>
#include "piControl.h"


struct revpi_ro_img_out {
	struct revpi_ro_target_state target_state;
	u32 thresh[REVPI_RO_NUM_RELAYS];
} __attribute__((__packed__));

struct revpi_ro_img_in {
	struct revpi_ro_status status;
} __attribute__((__packed__));


int revpi_ro_init(unsigned int devnum);
int revpi_ro_reset(void);
int revpi_ro_config(u8 addr, int num_entries, SEntryInfo *pEnt);
int revpi_ro_cycle(unsigned int devnum);

#endif /* REVPI_RO_H_ */

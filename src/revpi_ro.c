// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: 2023-2026 KUNBUS GmbH

// RevPi RO module (Relais Output)

#include <linux/pibridge_comm.h>

#include "piControlMain.h"
#include "revpi_common.h"
#include "revpi_core.h"
#include "revpi_ro.h"
#include "RevPiDevice.h"

/* Number of registered RO devices */
static unsigned int num_devices;

struct ro_config_list_item {
	u8 addr;
	struct revpi_ro_config config;
};

static struct ro_config_list_item ro_config_list[REV_PI_DEV_CNT_MAX];

void revpi_ro_reset(void)
{
	num_devices = 0;
}

int revpi_ro_config(u8 addr, int num_entries, SEntryInfo *pEnt)
{
	const unsigned int ENTRY_THRESH_FIRST = 2;
	const unsigned int ENTRY_THRESH_LAST = 14;
	struct ro_config_list_item *itm;
	unsigned int thr_idx;
	SEntryInfo *entry;
	int i;

	if (num_devices >= ARRAY_SIZE(ro_config_list)) {
		pr_err("too many ROs (max %zu)\n", ARRAY_SIZE(ro_config_list));
		return -ERANGE;
	}

	itm = &ro_config_list[num_devices];
	memset(itm, 0, sizeof(*itm));
	itm->addr = addr;

	for (i = 0; i < num_entries; i++) {
		entry = &pEnt[i];

		/*
		 * Set initial thresholds for wearout warning (0 means wearout
		 * warning is deactivated).
		 */
		if ((entry->i16uDeviceOffset >= ENTRY_THRESH_FIRST) &&
		    (entry->i16uDeviceOffset <= ENTRY_THRESH_LAST)) {
			thr_idx = (entry->i16uDeviceOffset - ENTRY_THRESH_FIRST) / 4;
			itm->config.thresh[thr_idx] = entry->i32uDefault;
		}
	}

	num_devices++;

	return 0;
}

int revpi_ro_init(unsigned int devnum)
{
	u8 addr = RevPiDevice_getDev(devnum)->i8uAddress;
	struct ro_config_list_item *itm;
	int i;

	for (i = 0; i < num_devices; i++) {
		itm = &ro_config_list[i];

		if (itm->addr == addr)
			break;
	}

	if (i == num_devices)
		return REVPI_MODULE_NOT_CONFIGURED;

	return revpi_send_config(addr, IOP_TYP1_CMD_CFG, &itm->config,
				 sizeof(struct revpi_ro_config));
}

int revpi_ro_cycle(unsigned int devnum)
{
	struct revpi_ro_target_state state_out;
	struct revpi_ro_status status_in;

	return revpi_cyclic_exchange(devnum, IOP_TYP1_CMD_DATA,
				     &state_out, sizeof(state_out),
				     &status_in, sizeof(status_in));
}

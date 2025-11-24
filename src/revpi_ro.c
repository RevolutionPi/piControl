// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: 2023 KUNBUS GmbH

// RevPi RO module (Relais Output)

#include <linux/pibridge_comm.h>

#include "piControlMain.h"
#include "revpi_common.h"
#include "revpi_core.h"
#include "revpi_ro.h"
#include "RevPiDevice.h"

#define REVPI_RO_MAX		10

struct revpi_ro_img_out {
	struct revpi_ro_target_state target_state;
	u32 thresh[REVPI_RO_NUM_RELAYS];
} __attribute__((__packed__));

struct revpi_ro_img_in {
	struct revpi_ro_status status;
} __attribute__((__packed__));

/* Number of registered RO devices */
static unsigned int num_devices;

struct ro_config_list_item {
	u8 addr;
	struct revpi_ro_config config;
};

static struct ro_config_list_item ro_config_list[REVPI_RO_MAX];

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

	if (num_devices >= REVPI_RO_MAX) {
		pr_err("max. number of ROs (%u) exceeded\n", REVPI_RO_MAX);
		return -1;
	}

	itm = &ro_config_list[num_devices];
	memset(itm, 0, sizeof(*itm));
	itm->addr = addr;

	pr_info_dio("%s: RO config done for addr %d \n", __func__, addr);

	for (i = 0; i < num_entries; i++) {
		entry = &pEnt[i];

		/*
		 * Set initial thresholds for wearout warning (0 means wearout
		 * warning is deactivated).
		 */
		if ((entry->i16uOffset >= ENTRY_THRESH_FIRST) &&
		    (entry->i16uOffset <= ENTRY_THRESH_LAST)) {
			thr_idx = (entry->i16uOffset - ENTRY_THRESH_FIRST) / 4;
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
		return 4;  // unknown device

	return pibridge_req_io(piCore_g.pibridge, addr, IOP_TYP1_CMD_CFG,
			       &itm->config, sizeof(struct revpi_ro_config),
			       NULL, 0);
}

int revpi_ro_cycle(unsigned int devnum)
{
	struct revpi_ro_target_state state_out;
	struct revpi_ro_status status_in;
	struct revpi_ro_img_out *img_out;
	struct revpi_ro_img_in *img_in;
	SDevice *dev;
	int ret;

	dev = RevPiDevice_getDev(devnum);

	img_out = (struct revpi_ro_img_out *) (piDev_g.ai8uPI +
					       dev->i16uOutputOffset);
	img_in = (struct revpi_ro_img_in *) (piDev_g.ai8uPI +
					     dev->i16uInputOffset);

	rt_mutex_lock(&piDev_g.lockPI);
	state_out = img_out->target_state;
	rt_mutex_unlock(&piDev_g.lockPI);

	ret = pibridge_req_io(piCore_g.pibridge, dev->i8uAddress,
			      IOP_TYP1_CMD_DATA, &state_out, sizeof(state_out),
			      &status_in, sizeof(status_in));

	if (ret != sizeof(status_in)) {
		pr_debug("RO addr %2d: communication failed (req:%zu,ret:%d)\n",
			dev->i8uAddress, sizeof(status_in), ret);

		if (ret >= 0)
			ret = -EIO;

		return ret;
	}

	rt_mutex_lock(&piDev_g.lockPI);
	img_in->status = status_in;
	rt_mutex_unlock(&piDev_g.lockPI);

	return 0;
}

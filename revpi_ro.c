

#include <linux/types.h>
#include <project.h>
#include <linux/pibridge_comm.h>

#include "revpi_ro.h"
#include "revpi_common.h"
#include "revpi_core.h"

#define REVPI_RO_MAX		10


/* Number of registered RO devices */
static unsigned int num_devices;

struct ro_config_list_item {
	u8 addr;
	struct revpi_ro_config config;
};

static struct ro_config_list_item ro_config_list[REVPI_RO_MAX];

int revpi_ro_reset(void)
{
	num_devices = 0;
	return 0;
}

int revpi_ro_config(u8 addr, int num_entries, SEntryInfo *pEnt)
{
	const unsigned int ENTRY_OFFSET_OUT= 1;
	const unsigned int ENTRY_THRESH_FIRST = 4;
	const unsigned int ENTRY_THRESH_LAST = 16;
	struct ro_config_list_item *itm;
	unsigned int thr_idx;
	SEntryInfo *entry;
	u8 relay_mask = 0;
	int i;

	if (num_devices > REVPI_RO_MAX) {
		pr_err("max. number of ROs (%u) exceeded\n", REVPI_RO_MAX);
		return -1;
	}

	itm = &ro_config_list[num_devices];
	memset(itm, 0, sizeof(*itm));
	itm->addr = addr;

	pr_info_dio("%s: RO config done for addr %d \n", __func__, addr);

	for (i = 0; i < num_entries; i++) {
		entry = &pEnt[i];

		if ((entry->i16uOffset == ENTRY_OFFSET_OUT) &&
		    (entry->i32uDefault))
			relay_mask |= (1 << entry->i8uBitPos);

		if ((entry->i16uOffset >= ENTRY_THRESH_FIRST) &&
		    (entry->i16uOffset <= ENTRY_THRESH_LAST)) {
			thr_idx = ((entry->i16uOffset + 4) - ENTRY_THRESH_FIRST) / 4 - 1;
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

	return pibridge_req_io(addr, IOP_TYP1_CMD_CFG, &itm->config,
			       sizeof(struct revpi_ro_config), NULL, 0);
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
	state_out.mask = img_out->target_state.mask;
	rt_mutex_unlock(&piDev_g.lockPI);

	ret = pibridge_req_io(dev->i8uAddress, IOP_TYP1_CMD_DATA,
			      &state_out, sizeof(state_out),
			      &status_in, sizeof(status_in));

	if (ret != sizeof(status_in)) {
		pr_warn_ratelimited("RO addr %2d: communication failed (req:%u,ret:%u)\n",
			dev->i8uAddress, (unsigned int) sizeof(status_in),
			ret);
		return 1;
	}

	rt_mutex_lock(&piDev_g.lockPI);
	memcpy(&img_in->status, &status_in, sizeof(status_in));
	rt_mutex_unlock(&piDev_g.lockPI);

	return 0;
}

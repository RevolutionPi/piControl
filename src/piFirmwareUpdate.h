/* SPDX-License-Identifier: GPL-2.0-only
 * SPDX-FileCopyrightText: 2017-2024 KUNBUS GmbH
 */

#ifndef PIFIRMWAREUPDATE_H
#define PIFIRMWAREUPDATE_H

#include <linux/firmware.h>
#include "piControlMain.h"
#include "RevPiDevice.h"

#define FIRMWARE_PATH		"/lib/firmware/revpi"


int FWU_update(tpiControlInst *priv, SDevice *pDev_p);
int flash_firmware(unsigned int dev_addr, unsigned int flash_addr,
		   unsigned char *upload_data, unsigned int upload_len);
int erase_flash(unsigned int dev_addr);
int upload_firmware(SDevice *sdev, const struct firmware *fw, u32 mask,
		    unsigned int module_type, unsigned int hw_rev);


#endif // PIFIRMWAREUPDATE_H

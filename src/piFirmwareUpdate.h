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
int upload_firmware(SDevice *sdev, const struct firmware *fw,
		    u32 mask);

#endif // PIFIRMWAREUPDATE_H

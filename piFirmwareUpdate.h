#ifndef PIFIRMWAREUPDATE_H
#define PIFIRMWAREUPDATE_H

#include <linux/limits.h>

#include "PiBridgeMaster.h"
#include "piConfig.h"
#include "RevPiDevice.h"

#define FIRMWARE_PATH		"/lib/firmware/revpi"


int FWU_update(SDevice *pDev_p);

#endif // PIFIRMWAREUPDATE_H

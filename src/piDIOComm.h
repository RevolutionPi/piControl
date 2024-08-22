/* SPDX-License-Identifier: GPL-2.0-only
 * SPDX-FileCopyrightText: 2016-2023 KUNBUS GmbH
 */

#pragma once

#include "common_define.h"
#include "piControl.h"

typedef enum
{
    DIOSTATE_OFFLINE   = 0x00, // Physikalisch nicht verbunden
    DIOSTATE_CYCLIC_IO = 0x01, // Zyklischer Datenaustausch ist aktiv
} DioCommStatus;

void piDIOComm_InitStart(void);

INT32U piDIOComm_Config(uint8_t i8uAddress, uint16_t i16uNumEntries, SEntryInfo * pEnt);

INT32U piDIOComm_Init(INT8U i8uDevice_p);

INT32U piDIOComm_sendCyclicTelegram(INT8U i8uDevice_p);

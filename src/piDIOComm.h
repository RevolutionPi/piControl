/* SPDX-License-Identifier: GPL-2.0-only
 * SPDX-FileCopyrightText: 2016-2023 KUNBUS GmbH
 */

#pragma once

#include "common_define.h"
#include "picontrol_intern.h"
#include "piControl.h"

void piDIOComm_InitStart(void);

u32 piDIOComm_Config(u8 i8uAddress, u16 i16uNumEntries, SEntryInfo * pEnt);

u32 piDIOComm_Init(u8 i8uDevice_p);

u32 piDIOComm_sendCyclicTelegram(u8 i8uDevice_p);

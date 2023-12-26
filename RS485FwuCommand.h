// SPDX-FileCopyrightText: 2017-2024 KUNBUS GmbH
//
// SPDX-License-Identifier: MIT
/*
 * Firmware update of RevPi modules using gateway protocol over RS-485
 */

#pragma once

int fwuEnterFwuMode(u8 address);
int fwuWriteSerialNum(u8 address, u32 i32uSerNum_p);
int fwuEraseFlash (u8 address);
int fwuWrite(u8 address, u32 flashAddr, char *data, u32 length);
int fwuResetModule(u8 address);

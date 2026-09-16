/* SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2017-2024 KUNBUS GmbH
 *
 * Firmware update of RevPi modules using gateway protocol over RS-485
 */

#pragma once

/*
 * Leading fields of the GetFwInfo response. The full response varies
 * between firmware generations, these fields are present in all.
 */
struct fwu_info {
	u16 fwu_version;
	u16 module_type;
	u16 hw_revision;
} __packed;

int fwuEnterFwuMode(u8 address);
int fwuWriteSerialNum(u8 address, u32 i32uSerNum_p);
int fwuEraseFlash (u8 address);
int fwuWrite(u8 address, u32 flashAddr, char *data, u32 length);
int fwuResetModule(u8 address);
int fwuDetectUpdateModeDevice(u8 address);

/* SPDX-License-Identifier: GPL-2.0-only
 * SPDX-FileCopyrightText: 2016-2024 KUNBUS GmbH
 */

#ifndef PRODUCTS_PIKERNELMOD_PICONFIG_H_
#define PRODUCTS_PIKERNELMOD_PICONFIG_H_

#include <linux/types.h>

#include "picontrol_intern.h"

typedef struct _piEntries {
	uint16_t i16uNumEntries;
	SEntryInfo ent[0];
} piEntries;

typedef struct _piDevices {
	uint16_t i16uNumDevices;
	SDeviceInfo dev[0];
} piDevices;

typedef struct _piCopyEntry {
	uint16_t i16uAddr;
	uint16_t i16uLength;
	uint8_t i8uBitMask;	// bitmask for bits to copy
} piCopyEntry;

typedef struct _piCopylist {
	uint16_t i16uNumEntries;
	piCopyEntry ent[0];
} piCopylist;

typedef struct _piConnection {
	uint16_t i16uSrcAddr;
	uint16_t i16uDestAddr;
	uint8_t i8uLength;	// in bit: 1-7, 8, 16, 32
	uint8_t i8uSrcBit;	// used only, if i8uLength < 8
	uint8_t i8uDestBit;	// used only, if i8uLength < 8
} piConnection;

typedef struct _piConnectionlist {
	uint16_t i16uNumEntries;
	piConnection conn[0];
} piConnectionList;

int piConfigParse(const char *filename, piDevices ** devs, piEntries ** ent, piCopylist ** cl,
		  piConnectionList ** conn);

struct file *open_filename(const char *filename, int flags);
void close_filename(struct file *file);
void revpi_set_defaults(unsigned char *mem, piEntries *entries);

#endif

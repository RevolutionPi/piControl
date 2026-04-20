/* SPDX-License-Identifier: GPL-2.0-only
 * SPDX-FileCopyrightText: 2016-2024 KUNBUS GmbH
 */

#ifndef PRODUCTS_PIKERNELMOD_PICONFIG_H_
#define PRODUCTS_PIKERNELMOD_PICONFIG_H_

#include <linux/types.h>

#include "json.h"
#include "picontrol_intern.h"

typedef struct _piEntries {
	u16 i16uNumEntries;
	SEntryInfo ent[0];
} piEntries;

typedef struct _piDevices {
	u16 i16uNumDevices;
	SDeviceInfo dev[0];
} piDevices;

typedef struct _piCopyEntry {
	u16 i16uAddr;
	u16 i16uLength;
	u8 i8uBitMask;	// bitmask for bits to copy
} piCopyEntry;

typedef struct _piCopylist {
	u16 i16uNumEntries;
	piCopyEntry ent[0];
} piCopylist;

typedef struct _piConnection {
	u16 i16uSrcAddr;
	u16 i16uDestAddr;
	u8 i8uLength;	// in bit: 1-7, 8, 16, 32
	u8 i8uSrcBit;	// used only, if i8uLength < 8
	u8 i8uDestBit;	// used only, if i8uLength < 8
} piConnection;

typedef struct _piConnectionlist {
	u16 i16uNumEntries;
	piConnection conn[0];
} piConnectionList;

int piConfigParse(const char *filename, piDevices ** devs, piEntries ** ent, piCopylist ** cl,
		  piConnectionList ** conn);

struct file *open_filename(const char *filename, int flags);
void close_filename(struct file *file);
void revpi_set_defaults(unsigned char *mem, piEntries *entries);
int process_file(json_parser * parser, struct file *input, int *retlines, int *retcols);

#endif

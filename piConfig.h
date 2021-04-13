/*=======================================================================================
 *
 *	       KK    KK   UU    UU   NN    NN   BBBBBB    UU    UU    SSSSSS
 *	       KK   KK    UU    UU   NNN   NN   BB   BB   UU    UU   SS
 *	       KK  KK     UU    UU   NNNN  NN   BB   BB   UU    UU   SS
 *	+----- KKKKK      UU    UU   NN NN NN   BBBBB     UU    UU    SSSSS
 *	|      KK  KK     UU    UU   NN  NNNN   BB   BB   UU    UU        SS
 *	|      KK   KK    UU    UU   NN   NNN   BB   BB   UU    UU        SS
 *	|      KK    KKK   UUUUUU    NN    NN   BBBBBB     UUUUUU    SSSSSS     GmbH
 *	|
 *	|            [#]  I N D U S T R I A L   C O M M U N I C A T I O N
 *	|             |
 *	+-------------+
 *
 *---------------------------------------------------------------------------------------
 *
 * (C) KUNBUS GmbH, Heerweg 15C, 73770 Denkendorf, Germany
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License V2 as published by
 * the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *  For licencing details see COPYING
 *
 *=======================================================================================
 */

#ifndef PRODUCTS_PIKERNELMOD_PICONFIG_H_
#define PRODUCTS_PIKERNELMOD_PICONFIG_H_

#include "json.h"
#include <piControl.h>

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

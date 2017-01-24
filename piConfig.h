/*
 * Copyright (C) 2009 Vincent Hanquez <vincent@snarc.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; version 2.1 or version 3.0 only.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef PRODUCTS_PIKERNELMOD_PICONFIG_H_
#define PRODUCTS_PIKERNELMOD_PICONFIG_H_

#include "json.h"
#include <piControl.h>


typedef struct _piEntries
{
    uint16_t            i16uNumEntries;
    SEntryInfo          ent[0];
} piEntries;

typedef struct _piDevices
{
    uint16_t            i16uNumDevices;
    SDeviceInfo         dev[0];
} piDevices;

typedef struct _piCopyEntry
{
    uint16_t            i16uAddr;
    uint16_t            i16uLength;
    uint8_t             i8uBitMask;    // bitmask for bits to copy
} piCopyEntry;

typedef struct _piCopylist
{
    uint16_t            i16uNumEntries;
    piCopyEntry        ent[0];
} piCopylist;

int piConfigParse(const char *filename, piDevices **devs, piEntries **ent, piCopylist **cl);


#endif

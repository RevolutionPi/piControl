/* SPDX-License-Identifier: GPL-2.0-only
 * SPDX-FileCopyrightText: 2016-2023 KUNBUS GmbH
 */

#pragma once

#include "common_define.h"
#include "ModGateComMain.h"
#include "piIOComm.h"

typedef struct _SRevPiProcessImage SRevPiProcessImage;

#define REV_PI_DEV_UNDEF            255
#define REV_PI_DEV_FIRST_RIGHT      32
#define REV_PI_DEV_FIRST_LEFT	    (REV_PI_DEV_FIRST_RIGHT - 1)
#define REV_PI_DEV_CNT_MAX          64
#define REV_PI_DEV_DEFAULT_SERIAL   1

typedef struct _SDevice
{
    u8 i8uAddress;
    u8 i8uActive;
    u8 i8uScan;			// found on scan
    u16 i16uInputOffset;
    u16 i16uOutputOffset;
    u16 i16uConfigLength;
    u16 i16uConfigOffset;
    u16 i16uErrorCnt;
    MODGATECOM_IDResp sId;
    u8 i8uModuleState;
	u8 i8uPriv;	//used by the module privately
} SDevice;


typedef struct _SDeviceConfig
{
    u8 i8uAddressRight;
    bool gatewayRight;
    u8 i8uAddressLeft;
    bool gatewayLeft;
    u8 i8uDeviceCount;
    u16 i16uErrorCnt;

    u8  i8uStatus;               // status bitfield of RevPi
    unsigned int offset;		// Offset in RevPi in process image
    SDevice dev[REV_PI_DEV_CNT_MAX+1];
} SDeviceConfig;

//-------------------------------------------------------------------------------------------------

bool RevPiDevice_writeNextConfiguration(u8 i8uAddress_p, MODGATECOM_IDResp *pModgateId_p);

void RevPiDevice_init(void);

int RevPiDevice_run(void);
bool RevPiDevice_writeNextConfigurationRight(void);
bool RevPiDevice_writeNextConfigurationLeft(void);
void RevPiDevice_startDataexchange(void);
u8 RevPiDevice_find_by_side_and_type(bool right, u16 module_type);
u8 RevPiDevice_setStatus(u8 clr, u8 set);
u8 RevPiDevice_getStatus(void);

void RevPiDevice_resetDevCnt(void);
void RevPiDevice_incDevCnt(void);
u8 RevPiDevice_getDevCnt(void);

u8 RevPiDevice_getAddrLeft(void);
u8 RevPiDevice_getAddrRight(void);

u16 RevPiDevice_getErrCnt(void);
SDevice *RevPiDevice_getDev(u8 idx);

void RevPiDevice_setCoreOffset(unsigned int offset);
unsigned int RevPiDevice_getCoreOffset(void);

int RevPiDevice_hat_serial(void);
void revpi_dev_update_state(u8 i8uDevice, u32 r, int *retval);
void RevPiDevice_handle_internal_telegrams(void);
int RevPiDevice_setBaseTermination(void);
int RevPiDevice_setLeftModuleTermination(bool terminate);
int RevPiDevice_setRightModuleTermination(bool terminate);

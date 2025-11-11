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
#define REV_PI_DEV_CNT_MAX          64
#define REV_PI_DEV_DEFAULT_SERIAL   1

typedef struct _SDevice
{
    INT8U i8uAddress;
    INT8U i8uActive;
    INT8U i8uScan;			// found on scan
    INT16U i16uInputOffset;
    INT16U i16uOutputOffset;
    INT16U i16uConfigLength;
    INT16U i16uConfigOffset;
    INT16U i16uErrorCnt;
    MODGATECOM_IDResp sId;
    INT8U i8uModuleState;
	INT8U i8uPriv;	//used by the module privately
} SDevice;


typedef struct _SDeviceConfig
{
    INT8U i8uAddressRight;
    INT8U i8uAddressLeft;
    INT8U i8uDeviceCount;
    INT16U i16uErrorCnt;

    INT8U  i8uStatus;               // status bitfield of RevPi
    unsigned int offset;		// Offset in RevPi in process image
    SDevice dev[REV_PI_DEV_CNT_MAX+1];
} SDeviceConfig;

//-------------------------------------------------------------------------------------------------

TBOOL RevPiDevice_writeNextConfiguration(INT8U i8uAddress_p, MODGATECOM_IDResp *pModgateId_p);

void RevPiDevice_init(void);

int RevPiDevice_run(void);
TBOOL RevPiDevice_writeNextConfigurationRight(void);
TBOOL RevPiDevice_writeNextConfigurationLeft(void);
void RevPiDevice_startDataexchange(void);
void RevPiDevice_stopDataexchange(void);
u8 RevPiDevice_find_by_side_and_type(bool right, u16 module_type);
INT8U RevPiDevice_setStatus(INT8U clr, INT8U set);
INT8U RevPiDevice_getStatus(void);

void RevPiDevice_resetDevCnt(void);
void RevPiDevice_incDevCnt(void);
INT8U RevPiDevice_getDevCnt(void);

INT8U RevPiDevice_getAddrLeft(void);
INT8U RevPiDevice_getAddrRight(void);

INT16U RevPiDevice_getErrCnt(void);
SDevice *RevPiDevice_getDev(INT8U idx);

void RevPiDevice_setCoreOffset(unsigned int offset);
unsigned int RevPiDevice_getCoreOffset(void);

int RevPiDevice_hat_serial(void);
void revpi_dev_update_state(INT8U i8uDevice, INT32U r, int *retval);
void RevPiDevice_handle_internal_telegrams(void);

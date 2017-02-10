//+=============================================================================================
//|
//!		\file RevPiDevice.h
//!		Device administration for PiBridge master modules
//|
//+---------------------------------------------------------------------------------------------
//|
//|		File-ID:		$Id: RevPiDevice.h 446 2016-12-07 09:11:34Z mduckeck $
//|		Location:		$URL: http://srv-kunbus03.de.pilz.local/raspi/trunk/products/PiCore/piKernelMod/RevPiDevice.h $
//|		Company:		$Cpn:$
//|
//+=============================================================================================

#pragma once

#include <ModGateComMain.h>
#include <piIOComm.h>

#define REV_PI_DEV_UNDEF            255
#define REV_PI_DEV_FIRST_RIGHT      32
#define REV_PI_DEV_CNT_MAX          64

typedef struct _SDevice
{
    INT8U i8uAddress;
    INT8U i8uActive;
    INT16U i16uInputOffset;
    INT16U i16uOutputOffset;
    INT16U i16uConfigLength;
    INT16U i16uConfigOffset;
    INT16U i16uErrorCnt;
    MODGATECOM_IDResp sId;
} SDevice;


typedef struct _SDeviceConfig
{
    INT8U i8uAddressRight;
    INT8U i8uAddressLeft;
    INT8U i8uDeviceCount;
    INT16U i16uErrorCnt;

    INT8U  i8uStatus;               // status bitfield of RevPi
    SRevPiCoreImage *pCoreData;     // Pointer to process image, may point to NULL
    SDevice dev[REV_PI_DEV_CNT_MAX];
} SDeviceConfig;

extern SDeviceConfig RevPiScan;

TBOOL RevPiDevice_writeNextConfiguration(INT8U i8uAddress_p, MODGATECOM_IDResp *pModgateId_p);

void RevPiDevice_init(void);
void RevPiDevice_finish(void);

int RevPiDevice_run(void);
TBOOL RevPiDevice_writeNextConfigurationRight(void);
TBOOL RevPiDevice_writeNextConfigurationLeft(void);
void RevPiDevice_startDataexchange(void);
void RevPiDevice_stopDataexchange(void);


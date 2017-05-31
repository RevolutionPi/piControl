//+=============================================================================================
//|
//!             \file PiBridgeMaster.h
//!             State machine for PiBridge master modules
//|
//+---------------------------------------------------------------------------------------------
//|
//|             File-ID:                $Id: PiBridgeMaster.h 11244 2016-12-07 09:11:48Z mduckeck $
//|             Location:               $URL: http://srv-kunbus03.de.pilz.local/feldbus/software/trunk/platform/ModGateCom/sw/PiBridgeMaster.h $
//|             Company:                $Cpn:$
//|
//+=============================================================================================

#pragma once

typedef enum _EPiBridgeMasterStatus {
	// states for IO Protocol
	enPiBridgeMasterStatus_Init,	// 0
	enPiBridgeMasterStatus_MasterIsPresentSignalling1,	// 1
	enPiBridgeMasterStatus_MasterIsPresentSignalling2,	// 2
	enPiBridgeMasterStatus_InitialSlaveDetectionRight,	// 3
	enPiBridgeMasterStatus_ConfigRightStart,	// 4
	enPiBridgeMasterStatus_ConfigDialogueRight,	// 5
	enPiBridgeMasterStatus_SlaveDetectionRight,	// 6
	enPiBridgeMasterStatus_InitialSlaveDetectionLeft,	// 7
	enPiBridgeMasterStatus_ConfigLeftStart,	// 8
	enPiBridgeMasterStatus_ConfigDialogueLeft,	// 9
	enPiBridgeMasterStatus_SlaveDetectionLeft,	// 10
	enPiBridgeMasterStatus_EndOfConfig,	// 11
	enPiBridgeMasterStatus_Continue,	// 12
	enPiBridgeMasterStatus_InitRetry,	// 13

	// states for MGate Protocol
	enPiBridgeMasterStatus_FWUMode,	// 14
	enPiBridgeMasterStatus_ProgramSerialNum,	// 15
	enPiBridgeMasterStatus_FWUFlashErase,	// 16
	enPiBridgeMasterStatus_FWUFlashWrite,	// 17
	enPiBridgeMasterStatus_FWUReset,	// 18

} EPiBridgeMasterStatus;

typedef struct _SRevPiCoreImage {
	// input data: set by driver
	INT8U i8uStatus;
	INT8U i8uIOCycle;
	INT16U i16uRS485ErrorCnt;
	INT8U i8uCPUTemperature;
	INT8U i8uCPUFrequency;

	// output data: set by application
	INT8U i8uLED;
	INT16U i16uRS485ErrorLimit1;
	INT16U i16uRS485ErrorLimit2;

} __attribute__ ((__packed__)) SRevPiCoreImage;

void PiBridgeMaster_Reset(void);
int PiBridgeMaster_Adjust(void);
void PiBridgeMaster_setDefaults(void);
int PiBridgeMaster_Run(void);
void PiBridgeMaster_Stop(void);
void PiBridgeMaster_Continue(void);
int PiBridgeMaster_gotoMGateComMode(void);
INT32S PiBridgeMaster_FWUModeEnter(INT32U address, INT8U i8uScanned);
INT32S PiBridgeMaster_FWUsetSerNum(INT32U serNum);
INT32S PiBridgeMaster_FWUflashErase(void);
INT32S PiBridgeMaster_FWUflashWrite(INT32U flash_add, char *data, INT32U length);
INT32S PiBridgeMaster_FWUReset(void);

/* SPDX-License-Identifier: GPL-2.0-only
 * SPDX-FileCopyrightText: 2016-2023 KUNBUS GmbH
 */

#pragma once

#include "common_define.h"

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

extern EPiBridgeMasterStatus eRunStatus_s;

void PiBridgeMaster_Reset(void);
int PiBridgeMaster_Adjust(void);
void PiBridgeMaster_setDefaults(void);
int PiBridgeMaster_Run(void);
void PiBridgeMaster_Stop(void);
void PiBridgeMaster_Continue(void);
INT32S PiBridgeMaster_FWUModeEnter(INT32U address, INT8U i8uScanned);
INT32S PiBridgeMaster_FWUsetSerNum(INT32U serNum);
INT32S PiBridgeMaster_FWUflashErase(void);
INT32S PiBridgeMaster_FWUflashWrite(INT32U flash_add, char *data, INT32U length);
INT32S PiBridgeMaster_FWUReset(void);

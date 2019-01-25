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

#pragma once

#include "IoProtocol.h"

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

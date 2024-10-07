/* SPDX-License-Identifier: GPL-2.0-only
 * SPDX-FileCopyrightText: 2017-2023 KUNBUS GmbH
 */

//+=============================================================================================
//|
//!		\file fwuMain.h
//!
//!		Firmware update main routines.
//|
//+---------------------------------------------------------------------------------------------
//|
//|		Files required:	(none)
//|
//+=============================================================================================
#ifndef FWUFLASHFILEMAIN_H_INC
#define FWUFLASHFILEMAIN_H_INC

#if defined(__STM32GENERIC__)
    #define FWU_CODE_SECTION    __attribute__ ((section(".firmwareUpdate")))
#else // Common Define for Visual Studio / WIN32
    #define FWU_CODE_SECTION
#endif

#include "common_define.h"

#ifdef __cplusplus
extern "C" { 
#endif 


typedef struct StrFileHeadData {
    INT16U usType;
    INT16U usHwRev;
    INT16U usFwuRev;
    INT32U ulFlashStart;
    INT32U ulFlashEnd;
    INT32U ulFwuEntry;
    INT32U ulFlashCrc;
} __attribute__((__packed__)) TFileHeadData; // sizeof = 22

typedef struct StrFPGAHeadData {
    INT32U ulFPGALen;
    INT32U ulFPGACrc;
} __attribute__((__packed__)) TFPGAHeadData; // sizeof = 8

typedef struct StrFileHead {
    INT8U acSync[2];
    INT32U ulLength;
    TFileHeadData dat;
    TFPGAHeadData fpga;
} __attribute__((__packed__)) TFileHead; // sizeof = 36


#define FWU_FILENAME_PATTERN		"/*.kfu"
#define FWU_FILENAME_PATTERN_0		'/'
#define FWU_FILENAME_PATTERN_1		'*'
#define FWU_FILENAME_PATTERN_2		'.'
#define FWU_FILENAME_PATTERN_3		'k'
#define FWU_FILENAME_PATTERN_4		'f'
#define FWU_FILENAME_PATTERN_5		'u'
#define FWU_FILENAME_PATTERN_6		'\0'
#define FWU_FILENAME_MODE_0			'r'
#define FWU_FILENAME_MODE_1			'b'
#define FWU_FILENAME_MODE_2			'\0'

extern void FWU_FS_main (void *file, TFileHead *header, INT8U write_fpga) FWU_CODE_SECTION;
extern void FWU_BuR_main(INT32U i32uAppPrgStartAdd, INT32U i32uAppPrgSize) FWU_CODE_SECTION;
extern TBOOL FWU_check_file(void *file, TFileHead *header) FWU_CODE_SECTION;
extern void FWU_error (INT16U i16uErrorCode) FWU_CODE_SECTION;

#ifdef  __cplusplus 
} 
#endif 

#endif // FWUFLASHFILEMAIN_H_INC




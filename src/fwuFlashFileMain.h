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
    u16 usType;
    u16 usHwRev;
    u16 usFwuRev;
    u32 ulFlashStart;
    u32 ulFlashEnd;
    u32 ulFwuEntry;
    u32 ulFlashCrc;
} __attribute__((__packed__)) TFileHeadData; // sizeof = 22

typedef struct StrFPGAHeadData {
    u32 ulFPGALen;
    u32 ulFPGACrc;
} __attribute__((__packed__)) TFPGAHeadData; // sizeof = 8

typedef struct StrFileHead {
    u8 acSync[2];
    u32 ulLength;
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

extern void FWU_FS_main (void *file, TFileHead *header, u8 write_fpga) FWU_CODE_SECTION;
extern void FWU_BuR_main(u32 i32uAppPrgStartAdd, u32 i32uAppPrgSize) FWU_CODE_SECTION;
extern bool FWU_check_file(void *file, TFileHead *header) FWU_CODE_SECTION;
extern void FWU_error (u16 i16uErrorCode) FWU_CODE_SECTION;

#ifdef  __cplusplus 
} 
#endif 

#endif // FWUFLASHFILEMAIN_H_INC




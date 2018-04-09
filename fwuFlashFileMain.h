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



#ifdef __cplusplus
extern "C" { 
#endif 


typedef 
#include <COMP_packBegin.h>     
struct StrFileHeadData
{
    INT16U usType;
    INT16U usHwRev;
    INT16U usFwuRev;
    INT32U ulFlashStart;
    INT32U ulFlashEnd;
    INT32U ulFwuEntry;
    INT32U ulFlashCrc;
} 
#include <COMP_packEnd.h>     
TFileHeadData;              // sizeof = 22

typedef 
#include <COMP_packBegin.h>     
struct StrFPGAHeadData
{
    unsigned long ulFPGALen;
    unsigned long ulFPGACrc;
} 
#include <COMP_packEnd.h>     
TFPGAHeadData;              // sizeof = 8

typedef 
#include <COMP_packBegin.h>     
struct StrFileHead
{
    INT8U acSync[2];
    INT32U ulLength;
    TFileHeadData dat;
    TFPGAHeadData fpga;
} 
#include <COMP_packEnd.h>     
TFileHead;                  // sizeof = 36


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




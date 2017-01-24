//+=============================================================================================
//|
//!    \file kbAlloc.h
//!    memory allocation in memory partition
//|
//+---------------------------------------------------------------------------------------------
//|
//|    File-ID:    $Id: kbAlloc.h 6269 2014-06-11 14:55:32Z fbrandauer $
//|    Location:   $URL: http://srv-kunbus03.de.pilz.local/feldbus/software/trunk/platform/utilities/sw/kbAlloc.h $
//|    Copyright:  KUNBUS GmbH
//|
//+=============================================================================================

#ifndef KBALLOC_H_INC
#define KBALLOC_H_INC

//+=============================================================================================
//|		Include-Dateien / include files
//+=============================================================================================
#include <common_debug.h>

//+=============================================================================================
//|		Prototypen / prototypes
//+=============================================================================================
#ifdef __cplusplus
extern "C" { 
#endif 

extern void *kbUT_initHeap(INT8U *pHeap, INT32U i32uLen_p);
#if CMN_CHECK_LEVEL >= 2
#define kbUT_malloc(heap, owner, size)	kbUT_malloc_debug(heap, owner, size, __FILE__, __LINE__)
#define kbUT_calloc(heap, owner, size)	kbUT_calloc_debug(heap, owner, size, __FILE__, __LINE__)

extern void *kbUT_malloc_debug(void *pHandle_p, INT8U i8uOwner_p, INT32U i32uLen_p,
	const char* file, const int line
	);
extern void *kbUT_calloc_debug(void *pHandle_p, INT8U i8uOwner_p, INT32U i32uLen_p,
	const char* file, const int line
	);
#else
extern void *kbUT_malloc(void *pHandle_p, INT8U i8uOwner_p, INT32U i32uLen_p
	);
extern void *kbUT_calloc(void *pHandle_p, INT8U i8uOwner_p, INT32U i32uLen_p
	);
#endif

extern void kbUT_free(void *pvMem_p);

#if CMN_CHECK_LEVEL >= 3
#include <stdio.h>
extern void kbUT_dump(void *pHandle_p, FILE *fp);
#endif
INT32U kbUT_minFree(void *pHandle_p);

#ifdef  __cplusplus 
} 
#endif 

#endif // KBUTILITIES_H_INC


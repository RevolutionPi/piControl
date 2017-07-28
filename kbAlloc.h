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


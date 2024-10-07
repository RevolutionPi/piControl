/* SPDX-License-Identifier: GPL-2.0-only
 * SPDX-FileCopyrightText: 2016-2023 KUNBUS GmbH
 */

//+=============================================================================================
//|
//!		\file systick.h
//!
//!		Cortex M3 System tick access.
//|
//|		Files required:	(none)
//|
//+=============================================================================================
#ifndef __SYSTICK_H
#define __SYSTICK_H

#include "../../common_define.h"

#ifdef __cplusplus
extern "C" { 
#endif 


//+=============================================================================================
//|		Typen / types
//+=============================================================================================

//typedef ...


//+=============================================================================================
//|		Prototypen / prototypes
//+=============================================================================================
extern void		SysTickHandler				( void );
extern void		ext6_handler				( void );

extern INT32U	kbGetTickCount				( void );


extern void BSP_STK_init (void);


#ifdef __cplusplus
} 
#endif 

//-------------------- reinclude-protection -------------------- 
#endif//__SYSTICK_H



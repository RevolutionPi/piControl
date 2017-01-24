//+=============================================================================================
//|
//!		\file systick.h
//!
//!		Cortex M3 System tick access.
//|
//+=============================================================================================
//|	
//|		File-ID:		$Id: systick.h 10119 2016-04-04 06:40:46Z reusch $
//|		Location:		$URL: http://srv-kunbus03.de.pilz.local/feldbus/software/trunk/platform/bsp/sw/bsp/systick/systick.h $ $
//|		Company:		$Cpn: KUNBUS GmbH $
//|
//+=============================================================================================
//|
//|		Files required:	(none)
//|
//+=============================================================================================
#ifndef __SYSTICK_H
#define __SYSTICK_H

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



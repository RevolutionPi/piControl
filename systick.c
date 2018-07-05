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

#include <common_define.h>
#include <bsp/systick/systick.h>

#include <linux/ktime.h>
#include <linux/hrtimer.h>

//+=============================================================================================
//|             Konstanten / constants
//+=============================================================================================

//+=============================================================================================
//|             Variablen / variables
//+=============================================================================================

static INT32U i32uCounter_s = 0;
//static INT32U i32uCounterStart_s = 0;

static TBOOL hrtime_initialized;
struct hrtimer hrtime_g;


//-------------------------------------------------------------------------------------------------
INT32U kbGetTickCount(void)
{
	ktime_t ktime_l;
	if (!hrtime_initialized) {
		hrtimer_init(&hrtime_g, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
		hrtime_initialized = bTRUE;
	}

	ktime_l = hrtimer_cb_get_time(&hrtime_g);
	i32uCounter_s = ktime_to_ms(ktime_l);

	return (i32uCounter_s);
}

//*************************************************************************************************

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
#ifdef __KUNBUSPI_KERNEL__
#include <linux/ktime.h>
#include <linux/hrtimer.h>
#else
#include <time.h>
#endif

//+=============================================================================================
//|             Konstanten / constants
//+=============================================================================================

//+=============================================================================================
//|             Variablen / variables
//+=============================================================================================

static INT32U i32uCounter_s = 0;
//static INT32U i32uCounterStart_s = 0;

#ifdef __KUNBUSPI_KERNEL__
static TBOOL hrtime_initialized;
struct hrtimer hrtime_g;
#endif


//-------------------------------------------------------------------------------------------------
INT32U kbGetTickCount(void)
{
#if 0
	i32uCounter_s = jiffies_to_msecs(jiffies - INITIAL_JIFFIES);
#else
#ifdef __KUNBUSPI_KERNEL__
	ktime_t ktime_l;
	if (!hrtime_initialized) {
		hrtimer_init(&hrtime_g, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
		hrtime_initialized = bTRUE;
	}

	ktime_l = hrtimer_cb_get_time(&hrtime_g);
	i32uCounter_s = ktime_to_ms(ktime_l);
#else
	struct timespec tNow;
	clock_gettime(CLOCK_MONOTONIC, &tNow);

#if 0
	if (i32uCounterStart_s == 0) {
		i32uCounterStart_s = tNow.tv_sec * 1000 + tNow.tv_nsec / 1000000;
		i32uCounter_s = 0;
	} else {
		i32uCounter_s = tNow.tv_sec * 1000 + tNow.tv_nsec / 1000000;
		i32uCounter_s -= i32uCounterStart_s;
	}
#else
	i32uCounter_s = tNow.tv_sec * 1000 + tNow.tv_nsec / 1000000;
#endif
#endif
#endif
	return (i32uCounter_s);
}

//*************************************************************************************************

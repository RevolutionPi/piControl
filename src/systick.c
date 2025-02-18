// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: 2016-2023 KUNBUS GmbH

#include <linux/hrtimer.h>

#include "bsp/systick/systick.h"
#include "common_define.h"

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

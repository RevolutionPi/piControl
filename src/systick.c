// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: 2016-2023 KUNBUS GmbH

#include <linux/hrtimer.h>
#include <linux/version.h>

#include "bsp/systick/systick.h"
#include "common_define.h"

//+=============================================================================================
//|             Konstanten / constants
//+=============================================================================================

//+=============================================================================================
//|             Variablen / variables
//+=============================================================================================

static u32 i32uCounter_s = 0;
//static u32 i32uCounterStart_s = 0;

static bool hrtime_initialized;
struct hrtimer hrtime_g;

//-------------------------------------------------------------------------------------------------
u32 kbGetTickCount(void)
{
	ktime_t ktime_l;
	if (!hrtime_initialized) {
#if KERNEL_VERSION(6, 13, 0) > LINUX_VERSION_CODE
		hrtimer_init(&hrtime_g, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
#else
		hrtimer_setup(&hrtime_g, hrtimer_dummy_timeout, CLOCK_MONOTONIC,
			      HRTIMER_MODE_ABS);
#endif
		hrtime_initialized = true;
	}

	ktime_l = hrtimer_cb_get_time(&hrtime_g);
	i32uCounter_s = ktime_to_ms(ktime_l);

	return (i32uCounter_s);
}

//*************************************************************************************************

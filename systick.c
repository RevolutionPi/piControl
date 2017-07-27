//+=============================================================================================
//|
//!             \file systick.c
//!
//!             Raspberry System tick generator/access.
//!
//!             Implementation of system tick handlers and Software timers ( mS base ).
//|
//+=============================================================================================
//|
//|             File-ID:                $Id: $
//|             Location:               $URL: $
//|             Company:                $Cpn: KUNBUS GmbH $
//|
//+=============================================================================================
//|
//|             Files required: (none)
//|
//+=============================================================================================

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

//!
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

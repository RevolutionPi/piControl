//+=============================================================================================
//|
//!		\file systick.c
//!
//!		Raspberry System tick generator/access.
//!
//!		Implementation of system tick handlers and Software timers ( mS base ).
//|
//+=============================================================================================
//|
//|		File-ID:		$Id: $
//|		Location:		$URL: $
//|		Company:		$Cpn: KUNBUS GmbH $
//|
//+=============================================================================================
//|
//|		Files required:	(none)
//|
//+=============================================================================================

#include <common_define.h>
#include <bsp/systick/systick.h>
#ifdef __KUNBUSPI_KERNEL__
#include <linux/ktime.h>
#else
#include <time.h>
#endif

//+=============================================================================================
//|		Konstanten / constants
//+=============================================================================================

//+=============================================================================================
//|		Variablen / variables
//+=============================================================================================

static INT32U i32uCounter_s = 0;

//! fetches actual time from CLOCK_REALTIME
//!
//-------------------------------------------------------------------------------------------------
INT32U kbGetTickCount ( void )
{
  struct timespec tNow;
#ifdef __KUNBUSPI_KERNEL__
  getnstimeofday(&tNow);
#else
  clock_gettime(CLOCK_REALTIME, &tNow);
#endif
    i32uCounter_s = tNow.tv_sec * 1000 + tNow.tv_nsec/1000000;
  return (i32uCounter_s);
}



//*************************************************************************************************

// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: 2016-2023 KUNBUS GmbH

#include <linux/jiffies.h>

#include "common_define.h"
#include "kbUtilities.h"

void kbUT_TimerStart(kbUT_Timer *ptTimer_p, u32 i32uDuration_p)
{
	ptTimer_p->i32uStartTime = jiffies_to_msecs(jiffies);
	ptTimer_p->i32uDuration = i32uDuration_p;
	ptTimer_p->bExpired = false;
	ptTimer_p->bRun = true;
}

bool kbUT_TimerExpired(kbUT_Timer *ptTimer_p)
{
	u32 i32uTimeDiff_l;
	bool bRv_l = false;

	if (ptTimer_p->bRun == true) {
		i32uTimeDiff_l = (jiffies_to_msecs(jiffies) - ptTimer_p->i32uStartTime);
		if (i32uTimeDiff_l >= ptTimer_p->i32uDuration) {
			ptTimer_p->bExpired = true;
			ptTimer_p->bRun = false;
		}
	}

	bRv_l = ptTimer_p->bExpired;

	ptTimer_p->bExpired = false; // Reset Flag, so that it is only once TRUE

	return bRv_l;
}

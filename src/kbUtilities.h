/* SPDX-License-Identifier: GPL-2.0-only
 * SPDX-FileCopyrightText: 2016-2023 KUNBUS GmbH
 */

#ifndef KBUTILITIES_H_INC
#define KBUTILITIES_H_INC

#include "common_define.h"

typedef struct kbUT_StrTimer {
	u32 i32uStartTime;
	u32 i32uDuration;
	bool bRun;
	bool bExpired;
} kbUT_Timer;

extern void kbUT_TimerStart(kbUT_Timer *ptTimer_p, u32 i32uDuration_p);
extern bool kbUT_TimerExpired(kbUT_Timer *ptTimer_p);

#endif // KBUTILITIES_H_INC

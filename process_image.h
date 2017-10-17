/*
 * process_image.h - cyclic update of the process image
 *
 * Copyright (C) 2017 KUNBUS GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#include <linux/hrtimer.h>
#include <linux/math64.h>

struct cycletimer {
	struct hrtimer_sleeper sleeper;
	u32 cycletime;
	u64 next_wake;
};

static inline enum hrtimer_restart wake_up_sleeper(struct hrtimer *timer)
{
	struct hrtimer_sleeper *sleeper =
		container_of(timer, struct hrtimer_sleeper, timer);

	wake_up_process(sleeper->task);
	return HRTIMER_NORESTART;
}

static inline void cycletimer_sleep(struct cycletimer *ct)
{
	struct hrtimer *timer = &ct->sleeper.timer;
	u64 now = ktime_to_ns(hrtimer_cb_get_time(timer));
	u64 this_wake = ct->next_wake;

	ct->next_wake += ct->cycletime;

	if (ct->next_wake < now) {
		ct->next_wake = roundup_u64(now, ct->cycletime);
		dev_warn(piDev_g.dev, "%s: missed %lld cycles\n",
			 current->comm, div_u64(ct->next_wake - this_wake,
						ct->cycletime) - 1);
	}

	set_current_state(TASK_UNINTERRUPTIBLE);
	hrtimer_start(timer, ns_to_ktime(ct->next_wake), HRTIMER_MODE_ABS);
	schedule();
}

static inline void cycletimer_change(struct cycletimer *ct, u32 cycletime)
{
	struct hrtimer *timer = &ct->sleeper.timer;

	ct->cycletime = cycletime;
	ct->next_wake = rounddown_u64(ktime_to_ns(hrtimer_cb_get_time(timer)),
				      cycletime);
}

static inline void cycletimer_init_on_stack(struct cycletimer *ct,
					    u32 cycletime)
{
	struct hrtimer *timer = &ct->sleeper.timer;

	hrtimer_init_on_stack(timer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
	timer->function = wake_up_sleeper;
	ct->sleeper.task = current;
	ct->cycletime = cycletime;
	ct->next_wake = rounddown_u64(ktime_to_ns(hrtimer_cb_get_time(timer)),
				      cycletime);
}

static inline void cycletimer_destroy(struct cycletimer *ct)
{
	hrtimer_cancel(&ct->sleeper.timer);
	destroy_hrtimer_on_stack(&ct->sleeper.timer);
}

static __always_inline void assign_bit_in_byte(u8 nr, u8 *addr, bool value)
{
	if (value)
		*addr |= BIT(nr);
	else
		*addr &= ~BIT(nr);
}

static __always_inline int test_bit_in_byte(u8 nr, u8 *addr)
{
	return (*addr >> nr) & 1;
}

#define flip_process_image(shadow, offset)				   \
{									   \
	rt_mutex_lock(&piDev_g.lockPI);					   \
	((typeof(shadow))(piDev_g.ai8uPI + (offset)))->drv = shadow->drv;  \
	shadow->usr = ((typeof(shadow))(piDev_g.ai8uPI + (offset)))->usr;  \
	rt_mutex_unlock(&piDev_g.lockPI);				   \
}

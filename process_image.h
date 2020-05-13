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
	ktime_t cycletime;
};

static inline enum hrtimer_restart wake_up_sleeper(struct hrtimer *timer)
{
	struct hrtimer_sleeper *sleeper = container_of(timer, struct hrtimer_sleeper, timer);

	wake_up_process(sleeper->task);
	return HRTIMER_NORESTART;
}

static inline void cycletimer_sleep(struct cycletimer *ct)
{
	struct hrtimer *timer = &ct->sleeper.timer;
	u64 missed_cycles = hrtimer_forward_now(timer, ct->cycletime);

	if (missed_cycles == 0)
		pr_warn("%s: premature cycle\n", current->comm);
	else if (missed_cycles > 1)
		pr_warn("%s: missed %lld cycles\n", current->comm,
			missed_cycles - 1);

	set_current_state(TASK_UNINTERRUPTIBLE);
	hrtimer_restart(timer);
	schedule();
}

static inline void cycletimer_change(struct cycletimer *ct, u32 cycletime)
{
	struct hrtimer *timer = &ct->sleeper.timer;

	ct->cycletime = ns_to_ktime(cycletime);
	hrtimer_cancel(timer);
	hrtimer_set_expires(timer, hrtimer_cb_get_time(timer));
}

static inline void cycletimer_init_on_stack(struct cycletimer *ct, u32 cycletime)
{
	struct hrtimer *timer = &ct->sleeper.timer;

	hrtimer_init_on_stack(timer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
	timer->function = wake_up_sleeper;
	ct->sleeper.task = current;
	cycletimer_change(ct, cycletime);
}

static inline void cycletimer_destroy(struct cycletimer *ct)
{
	hrtimer_cancel(&ct->sleeper.timer);
	destroy_hrtimer_on_stack(&ct->sleeper.timer);
}

static __always_inline void assign_bit_in_byte(u8 nr, u8 * addr, bool value)
{
	if (value)
		*addr |= BIT(nr);
	else
		*addr &= ~BIT(nr);
}

static __always_inline int test_bit_in_byte(u8 nr, u8 * addr)
{
	return (*addr >> nr) & 1;
}

#define flip_process_image(shadow, offset)						\
{											\
	if (piDev_g.stopIO == false) {							\
		if (((typeof(shadow))(piDev_g.ai8uPI + (offset))) == 0 || (shadow) == 0) \
			pr_err("NULL pointer: %p %p\n", ((typeof(shadow))(piDev_g.ai8uPI + (offset))), (shadow)); \
		my_rt_mutex_lock(&piDev_g.lockPI);					\
		((typeof(shadow))(piDev_g.ai8uPI + (offset)))->drv = (shadow)->drv;	\
		(shadow)->usr = ((typeof(shadow))(piDev_g.ai8uPI + (offset)))->usr;	\
		rt_mutex_unlock(&piDev_g.lockPI);					\
	}										\
}

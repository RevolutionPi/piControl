/* SPDX-License-Identifier: GPL-2.0-only
 * SPDX-FileCopyrightText: 2017-2024 KUNBUS GmbH
 *
 * process_image.h - cyclic update of the process image
 */

#ifndef _PROCESS_IMAGE_H
#define _PROCESS_IMAGE_H

#define REVPI_COMPACT_WARN_MISSED_CYCLES           10

#include <linux/hrtimer.h>
#include <linux/sched.h>
#include <linux/completion.h>
#include <linux/version.h>
#include "revpi_compact.h"

struct cycletimer {
	struct hrtimer timer;
	ktime_t cycletime;
	struct completion timer_expired;
};

static inline enum hrtimer_restart wake_up_sleeper(struct hrtimer *timer)
{
	struct cycletimer *ct = container_of(timer, struct cycletimer, timer);
	complete(&ct->timer_expired);
	return HRTIMER_NORESTART;
}

static inline void cycletimer_sleep(struct cycletimer *ct,
				    struct revpi_compact_stats *stats)
{
	struct hrtimer *timer = &ct->timer;
	u64 missed_cycles = hrtimer_forward_now(timer, ct->cycletime);

	if (missed_cycles == 0) /* should never happen (TM) */
		pr_warn("%s: premature cycle\n", current->comm);
	else if (missed_cycles > 1) {
		write_seqlock(&stats->lock);
		stats->lost_cycles += (missed_cycles - 1);
		write_sequnlock(&stats->lock);

		if (missed_cycles > REVPI_COMPACT_WARN_MISSED_CYCLES) {
			pr_warn("%s: missed %lld cycles\n", current->comm,
				missed_cycles - 1);
		}
	}

	reinit_completion(&ct->timer_expired);
	hrtimer_start_expires(timer, HRTIMER_MODE_ABS_HARD);
	wait_for_completion(&ct->timer_expired);
}

static inline void cycletimer_change(struct cycletimer *ct, u32 cycletime)
{
	struct hrtimer *timer = &ct->timer;

	ct->cycletime = ns_to_ktime(cycletime);
	hrtimer_cancel(timer);
	hrtimer_set_expires(timer, hrtimer_cb_get_time(timer));
}

static inline void cycletimer_init_on_stack(struct cycletimer *ct, u32 cycletime)
{
	struct hrtimer *timer = &ct->timer;

#if KERNEL_VERSION(6, 13, 0) > LINUX_VERSION_CODE
	hrtimer_init_on_stack(timer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS_HARD);
	timer->function = wake_up_sleeper;
#else
	hrtimer_setup_on_stack(timer, wake_up_sleeper, CLOCK_MONOTONIC,
			       HRTIMER_MODE_ABS_HARD);
#endif
	init_completion(&ct->timer_expired);
	cycletimer_change(ct, cycletime);
}

static inline void cycletimer_destroy(struct cycletimer *ct)
{
	hrtimer_cancel(&ct->timer);
	destroy_hrtimer_on_stack(&ct->timer);
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
	if (!test_bit(PICONTROL_DEV_FLAG_STOP_IO, &piDev_g.flags)) {			\
		if (((typeof(shadow))(piDev_g.ai8uPI + (offset))) == 0 || (shadow) == 0) \
			pr_err("NULL pointer: %p %p\n", ((typeof(shadow))(piDev_g.ai8uPI + (offset))), (shadow)); \
		my_rt_mutex_lock(&piDev_g.lockPI);					\
		((typeof(shadow))(piDev_g.ai8uPI + (offset)))->drv = (shadow)->drv;	\
		(shadow)->usr = ((typeof(shadow))(piDev_g.ai8uPI + (offset)))->usr;	\
		rt_mutex_unlock(&piDev_g.lockPI);					\
	}										\
}
#endif /* _PROCESS_IMAGE_H */

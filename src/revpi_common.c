// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: 2017-2024 KUNBUS GmbH

// revpi_common.c - common routines for RevPi machines

#include <linux/kthread.h>
#include <linux/leds.h>
#include <linux/sched.h>
#include <linux/types.h>

#include "piControlMain.h"
#include "revpi_common.h"
#include "RevPiDevice.h"

#define VCMSG_ID_ARM_CLOCK 0x000000003	/* Clock/Voltage ID's */

void revpi_rgb_led_trigger_event(u16 led_prev, u16 led)
{
	u16 changed = led_prev ^ led;
	if (changed == 0)
		return;

	// A1
	if (changed & PICONTROL_LED_RGB_A1_RED) {
		led_trigger_event(&piDev_g.a1_red, (led & PICONTROL_LED_RGB_A1_RED) ? LED_FULL : LED_OFF);
	}
	if (changed & PICONTROL_LED_RGB_A1_GREEN) {
		led_trigger_event(&piDev_g.a1_green, (led & PICONTROL_LED_RGB_A1_GREEN) ? LED_FULL : LED_OFF);
	}
	if (changed & PICONTROL_LED_RGB_A1_BLUE) {
		led_trigger_event(&piDev_g.a1_blue, (led & PICONTROL_LED_RGB_A1_BLUE) ? LED_FULL : LED_OFF);
	}
	// A2
	if (changed & PICONTROL_LED_RGB_A2_RED) {
		led_trigger_event(&piDev_g.a2_red, (led & PICONTROL_LED_RGB_A2_RED) ? LED_FULL : LED_OFF);
	}
	if (changed & PICONTROL_LED_RGB_A2_GREEN) {
		led_trigger_event(&piDev_g.a2_green, (led & PICONTROL_LED_RGB_A2_GREEN) ? LED_FULL : LED_OFF);
	}
	if (changed & PICONTROL_LED_RGB_A2_BLUE) {
		led_trigger_event(&piDev_g.a2_blue, (led & PICONTROL_LED_RGB_A2_BLUE) ? LED_FULL : LED_OFF);
	}
	// A3
	if (changed & PICONTROL_LED_RGB_A3_RED) {
		led_trigger_event(&piDev_g.a3_red, (led & PICONTROL_LED_RGB_A3_RED) ? LED_FULL : LED_OFF);
	}
	if (changed & PICONTROL_LED_RGB_A3_GREEN) {
		led_trigger_event(&piDev_g.a3_green, (led & PICONTROL_LED_RGB_A3_GREEN) ? LED_FULL : LED_OFF);
	}
	if (changed & PICONTROL_LED_RGB_A3_BLUE) {
		led_trigger_event(&piDev_g.a3_blue, (led & PICONTROL_LED_RGB_A3_BLUE) ? LED_FULL : LED_OFF);
	}
	// A4
	if (changed & PICONTROL_LED_RGB_A4_RED) {
		led_trigger_event(&piDev_g.a4_red, (led & PICONTROL_LED_RGB_A4_RED) ? LED_FULL : LED_OFF);
	}
	if (changed & PICONTROL_LED_RGB_A4_GREEN) {
		led_trigger_event(&piDev_g.a4_green, (led & PICONTROL_LED_RGB_A4_GREEN) ? LED_FULL : LED_OFF);
	}
	if (changed & PICONTROL_LED_RGB_A4_BLUE) {
		led_trigger_event(&piDev_g.a4_blue, (led & PICONTROL_LED_RGB_A4_BLUE) ? LED_FULL : LED_OFF);
	}
	// A5
	if (changed & PICONTROL_LED_RGB_A5_RED) {
		led_trigger_event(&piDev_g.a5_red, (led & PICONTROL_LED_RGB_A5_RED) ? LED_FULL : LED_OFF);
	}
	if (changed & PICONTROL_LED_RGB_A5_GREEN) {
		led_trigger_event(&piDev_g.a5_green, (led & PICONTROL_LED_RGB_A5_GREEN) ? LED_FULL : LED_OFF);
	}
	if (changed & PICONTROL_LED_RGB_A5_BLUE) {
		led_trigger_event(&piDev_g.a5_blue, (led & PICONTROL_LED_RGB_A5_BLUE) ? LED_FULL : LED_OFF);
	}
}

void revpi_led_trigger_event(u16 led_prev, u16 led)
{
	u16 changed = led_prev ^ led;
	if (changed == 0)
		return;

	if (changed & PICONTROL_LED_A1_GREEN) {
		led_trigger_event(&piDev_g.a1_green, (led & PICONTROL_LED_A1_GREEN) ? LED_FULL : LED_OFF);
	}
	if (changed & PICONTROL_LED_A1_RED) {
		led_trigger_event(&piDev_g.a1_red, (led & PICONTROL_LED_A1_RED) ? LED_FULL : LED_OFF);
	}
	if (changed & PICONTROL_LED_A2_GREEN) {
		led_trigger_event(&piDev_g.a2_green, (led & PICONTROL_LED_A2_GREEN) ? LED_FULL : LED_OFF);
	}
	if (changed & PICONTROL_LED_A2_RED) {
		led_trigger_event(&piDev_g.a2_red, (led & PICONTROL_LED_A2_RED) ? LED_FULL : LED_OFF);
	}

	if ((piDev_g.machine_type == REVPI_CONNECT) ||
	    (piDev_g.machine_type == REVPI_CONNECT_SE) ||
	    (piDev_g.machine_type == REVPI_FLAT)) {
		if (changed & PICONTROL_LED_A3_GREEN) {
			led_trigger_event(&piDev_g.a3_green, (led & PICONTROL_LED_A3_GREEN) ? LED_FULL : LED_OFF);
		}
		if (changed & PICONTROL_LED_A3_RED) {
			led_trigger_event(&piDev_g.a3_red, (led & PICONTROL_LED_A3_RED) ? LED_FULL : LED_OFF);
		}
	}

	if (piDev_g.machine_type == REVPI_FLAT) {
		if (changed & PICONTROL_LED_A4_GREEN) {
			led_trigger_event(&piDev_g.a4_green, (led & PICONTROL_LED_A4_GREEN) ? LED_FULL : LED_OFF);
		}
		if (changed & PICONTROL_LED_A4_RED) {
			led_trigger_event(&piDev_g.a4_red, (led & PICONTROL_LED_A4_RED) ? LED_FULL : LED_OFF);
		}
		if (changed & PICONTROL_LED_A5_GREEN) {
			led_trigger_event(&piDev_g.a5_green, (led & PICONTROL_LED_A5_GREEN) ? LED_FULL : LED_OFF);
		}
		if (changed & PICONTROL_LED_A5_RED) {
			led_trigger_event(&piDev_g.a5_red, (led & PICONTROL_LED_A5_RED) ? LED_FULL : LED_OFF);
		}
	}
}

static enum revpi_power_led_mode power_led_mode_s = 255;
static unsigned long power_led_timer_s;
static bool power_led_red_state_s;
char *lock_file;
int lock_line;

void revpi_power_led_red_set(enum revpi_power_led_mode mode)
{
	switch (mode) {
	case REVPI_POWER_LED_OFF:
		if (power_led_mode_s == REVPI_POWER_LED_OFF
			|| power_led_mode_s == REVPI_POWER_LED_ON_500MS
			|| power_led_mode_s == REVPI_POWER_LED_ON_1000MS)
			return; // nothing to do
		power_led_red_state_s = false;
		led_trigger_event(&piDev_g.power_red, LED_OFF);
		break;
	default:
	case REVPI_POWER_LED_ON:
		if (power_led_mode_s == REVPI_POWER_LED_ON)
			return; // nothing to do
		power_led_red_state_s = true;
		led_trigger_event(&piDev_g.power_red, LED_FULL);
		break;
	case REVPI_POWER_LED_FLICKR:
		// just set the mode variable, anything else is done in the run function
		if (jiffies_to_msecs(jiffies - power_led_timer_s) > 10000) {
			//pr_info("power led flickr\n");
			power_led_timer_s = jiffies;
		}
		break;
	case REVPI_POWER_LED_ON_500MS:
	case REVPI_POWER_LED_ON_1000MS:
		power_led_red_state_s = true;
		led_trigger_event(&piDev_g.power_red, LED_FULL);
		power_led_timer_s = jiffies;
		break;
	}
	power_led_mode_s = mode;
}


void revpi_check_timeout(void)
{
	ktime_t now = ktime_get();
	struct list_head *pCon;

	my_rt_mutex_lock(&piDev_g.lockListCon);
	list_for_each(pCon, &piDev_g.listCon) {
		tpiControlInst *pos_inst;
		pos_inst = list_entry(pCon, tpiControlInst, list);

		if (pos_inst->tTimeoutDurationMs != 0) {
			if (ktime_compare(now, pos_inst->tTimeoutTS) > 0) {
				// set all outputs to 0
				int i;
				my_rt_mutex_lock(&piDev_g.lockPI);
				for (i = 0; i < RevPiDevice_getDevCnt(); i++) {
					if (RevPiDevice_getDev(i)->i8uActive) {
						memset(piDev_g.ai8uPI + RevPiDevice_getDev(i)->i16uOutputOffset, 0, RevPiDevice_getDev(i)->sId.i16uFBS_OutputLength);
					}
				}
				rt_mutex_unlock(&piDev_g.lockPI);
				pos_inst->tTimeoutTS = ktime_add_ms(ktime_get(), pos_inst->tTimeoutDurationMs);

				// this must only be done for one connection
				rt_mutex_unlock(&piDev_g.lockListCon);
				return;
			}
		}
	}
	rt_mutex_unlock(&piDev_g.lockListCon);
}

void revpi_power_led_red_run(void)
{
	switch (power_led_mode_s) {
	case REVPI_POWER_LED_FLICKR:
		if (power_led_red_state_s && jiffies_to_msecs(jiffies - power_led_timer_s) > 10) {
			power_led_red_state_s = false;
			led_trigger_event(&piDev_g.power_red, LED_OFF);
			power_led_timer_s = jiffies;
		} else if (!power_led_red_state_s && jiffies_to_msecs(jiffies - power_led_timer_s) > 90) {
			power_led_red_state_s = true;
			led_trigger_event(&piDev_g.power_red, LED_FULL);
			power_led_timer_s = jiffies;
		}
		break;
	case REVPI_POWER_LED_ON_500MS:
		if (jiffies_to_msecs(jiffies - power_led_timer_s) > 500) {
			power_led_red_state_s = false;
			led_trigger_event(&piDev_g.power_red, LED_OFF);
			power_led_mode_s = REVPI_POWER_LED_OFF;
		}
		break;
	case REVPI_POWER_LED_ON_1000MS:
		if (jiffies_to_msecs(jiffies - power_led_timer_s) > 1000) {
			power_led_red_state_s = false;
			led_trigger_event(&piDev_g.power_red, LED_OFF);
			power_led_mode_s = REVPI_POWER_LED_OFF;
		}
		break;
	default:
		;		// nothing to do
	}
}

int set_rt_priority(struct task_struct *task, int priority)
{
	struct sched_attr attr;

	if (!task || priority < 0 || priority > MAX_RT_PRIO - 1)
		return -EINVAL;

	memset(&attr, 0, sizeof(attr));
	attr.sched_policy = SCHED_FIFO;
	attr.sched_priority = priority;

	return sched_setattr_nocheck(task, &attr);
}

/**
 * set_kthread_prios - assign realtime priority to specific kthreads
 * @ktprios: null-terminated array of kthread/priority tuples
 *
 * Walk the children of kthreadd and compare the command name to the ones
 * specified in @ktprios.  Upon finding a match, assign the given priority
 * with SCHED_FIFO policy.
 *
 * Return 0 on success or a negative errno on failure.
 * Normally failure only occurs because an invalid priority was specified.
 */
int set_kthread_prios(const struct kthread_prio *ktprios)
{
	const struct kthread_prio *ktprio;
	struct task_struct *child;
	int ret = 0;

	read_lock(&tasklist_lock);
	for (ktprio = ktprios; ktprio->comm[0]; ktprio++) {
		bool found = false;

		list_for_each_entry(child, &kthreadd_task->children, sibling)
			if (!strncmp(child->comm, ktprio->comm,
				     TASK_COMM_LEN)) {
				found = true;
				ret = set_rt_priority(child, ktprio->prio);
				if (ret) {
					pr_err("cannot set priority of %s\n",
					       ktprio->comm);
					goto out;
				} else {
					pr_info("set priority of %s to %d\n",
						ktprio->comm, ktprio->prio);
				}
				break;
			}

		if (!found) {
			pr_err("cannot find kthread %s\n", ktprio->comm);
			ret = -ENOENT;
			goto out;
		}
	}
out:
	read_unlock(&tasklist_lock);
	return ret;
}

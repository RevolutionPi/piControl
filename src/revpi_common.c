// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: 2017-2026 KUNBUS GmbH

// revpi_common.c - common routines for RevPi machines

#include <linux/kthread.h>
#include <linux/leds.h>
#include <linux/pibridge_comm.h>
#include <linux/sched.h>
#include <linux/types.h>

#include "piControlMain.h"
#include "revpi_common.h"
#include "revpi_core.h"
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


/* clamp each range: lengths come from the device table without lockPI held */
void revpi_zero_active_outputs(void)
{
	int i;

	guard(rt_mutex)(&piDev_g.lockPI);

	for (i = 0; i < RevPiDevice_getDevCnt(); i++) {
		SDevice *dev = RevPiDevice_getDev(i);
		u16 offset = dev->i16uOutputOffset;
		u16 len = dev->sId.i16uFBS_OutputLength;

		if (!dev->i8uActive)
			continue;
		if (offset >= PICONTROL_PROCESS_IMAGE_LEN)
			continue;
		if (offset + len > PICONTROL_PROCESS_IMAGE_LEN)
			len = PICONTROL_PROCESS_IMAGE_LEN - offset;

		memset(piDev_g.ai8uPI + offset, 0, len);
	}
}

/*
 * Fetch a module's output data from the process image for sending, or zero the
 * buffer while I/O is stopped. Pairs with revpi_store_input_data().
 */
void revpi_fetch_output_data(void *dst, u16 offset, size_t len)
{
	if (test_bit(PICONTROL_DEV_FLAG_STOP_IO, &piDev_g.flags)) {
		memset(dst, 0, len);
		return;
	}

	scoped_guard(rt_mutex, &piDev_g.lockPI)
		memcpy(dst, piDev_g.ai8uPI + offset, len);
}

/* Store a module's received input data into the process image, unless stopped. */
void revpi_store_input_data(u16 offset, const void *src, size_t len)
{
	if (test_bit(PICONTROL_DEV_FLAG_STOP_IO, &piDev_g.flags))
		return;

	scoped_guard(rt_mutex, &piDev_g.lockPI)
		memcpy(piDev_g.ai8uPI + offset, src, len);
}

/*
 * Send one cyclic telegram and require the full expected reply. Returns 0 or
 * a negative errno.
 */
int revpi_cyclic_request(u8 addr, u8 cmd, void *snd, size_t snd_len,
			 void *rcv, size_t rcv_len)
{
	int ret;

	ret = pibridge_req_io(piCore_g.pibridge, addr, cmd, snd, snd_len,
			      rcv, rcv_len);
	if (ret != rcv_len) {
		pr_debug("addr %u cmd %#x: cyclic io failed (req:%zu, ret:%d)\n",
			 addr, cmd, rcv_len, ret);
		return ret < 0 ? ret : -EIO;
	}

	return 0;
}

/*
 * Run one cyclic exchange for a module whose output and input images map
 * directly to fixed telegram buffers: send the output image, then store the
 * response into the input image. Returns 0 or a negative errno.
 */
int revpi_cyclic_exchange(u8 devnum, u8 cmd, void *out, size_t out_len,
			  void *in, size_t in_len)
{
	SDevice *dev = RevPiDevice_getDev(devnum);
	int ret;

	revpi_fetch_output_data(out, dev->i16uOutputOffset, out_len);

	ret = revpi_cyclic_request(dev->i8uAddress, cmd, out, out_len,
				   in, in_len);
	if (ret < 0)
		return ret;

	revpi_store_input_data(dev->i16uInputOffset, in, in_len);

	return 0;
}

/* Send a module configuration telegram (no reply expected). 0 or -errno. */
int revpi_send_config(u8 addr, u8 cmd, void *buf, size_t len)
{
	int ret;

	ret = pibridge_req_io(piCore_g.pibridge, addr, cmd, buf, len, NULL, 0);
	if (ret < 0)
		pr_debug("addr %u cmd %#x: config failed (ret:%d)\n",
			 addr, cmd, ret);

	return ret;
}

void revpi_check_timeout(void)
{
	ktime_t now = ktime_get();
	struct list_head *pCon;

	scoped_guard(rt_mutex, &piDev_g.lockListCon) {
		list_for_each(pCon, &piDev_g.listCon) {
			tpiControlInst *pos_inst;
			pos_inst = list_entry(pCon, tpiControlInst, list);

			if (pos_inst->tTimeoutDurationMs != 0) {
				if (ktime_compare(now, pos_inst->tTimeoutTS) > 0) {
					pr_warn_ratelimited("Watchdog timeout with duration %lu, setting outputs to 0\n",
							    pos_inst->tTimeoutDurationMs);
					// set all outputs to 0
					revpi_zero_active_outputs();
					pos_inst->tTimeoutTS = ktime_add_ms(ktime_get(), pos_inst->tTimeoutDurationMs);

					// this must only be done for one connection
					return;
				}
			}
		}
	}
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

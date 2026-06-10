/* SPDX-License-Identifier: GPL-2.0-only
 * SPDX-FileCopyrightText: 2017-2024 KUNBUS GmbH
 */

#ifndef _REVPI_COMMON_H
#define _REVPI_COMMON_H

#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/version.h>
#include <uapi/linux/sched/types.h>

enum revpi_power_led_mode {
	REVPI_POWER_LED_OFF = 0,
	REVPI_POWER_LED_ON = 1,
	REVPI_POWER_LED_FLICKR = 2,
	REVPI_POWER_LED_ON_500MS = 3,
	REVPI_POWER_LED_ON_1000MS = 4,
};

void revpi_rgb_led_trigger_event(u16 led_prev, u16 led);
void revpi_led_trigger_event(u16 led_prev, u16 led);
void revpi_power_led_red_set(enum revpi_power_led_mode mode);
void revpi_power_led_red_run(void);
void revpi_check_timeout(void);

extern char *lock_file;
extern int lock_line;

struct kthread_prio {
	const char comm[TASK_COMM_LEN];
	int prio;
};

int set_kthread_prios(const struct kthread_prio *ktprios);
int set_rt_priority(struct task_struct *task, int priority);
#endif /* _REVPI_COMMON_H */

/*
 * revpi_common.c - common routines for RevPi machines
 *
 * Copyright (C) 2017 KUNBUS GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#include <linux/leds.h>
#include <soc/bcm2835/raspberrypi-firmware.h>

#include "revpi_common.h"
#include "common_define.h"
#include "project.h"
#include "ModGateComMain.h"
#include "RevPiDevice.h"
#include "piControlMain.h"
#include "piControl.h"

#define VCMSG_ID_ARM_CLOCK 0x000000003	/* Clock/Voltage ID's */

void revpi_led_trigger_event(u8 * led_prev, u8 led)
{
	if (*led_prev == led)
		return;

	led_trigger_event(&piDev_g.a1_green, (led & PICONTROL_LED_A1_GREEN) ? LED_FULL : LED_OFF);
	led_trigger_event(&piDev_g.a1_red, (led & PICONTROL_LED_A1_RED) ? LED_FULL : LED_OFF);
	led_trigger_event(&piDev_g.a2_green, (led & PICONTROL_LED_A2_GREEN) ? LED_FULL : LED_OFF);
	led_trigger_event(&piDev_g.a2_red, (led & PICONTROL_LED_A2_RED) ? LED_FULL : LED_OFF);

	*led_prev = led;
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
		//pr_info("power led green\n");
		power_led_red_state_s = false;
		led_trigger_event(&piDev_g.power_red, LED_OFF);
		break;
	default:
	case REVPI_POWER_LED_ON:
		if (power_led_mode_s == REVPI_POWER_LED_ON)
			return; // nothing to do
		//pr_info("power led red\n");
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
		//pr_info("power led pulse\n");
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

	rt_mutex_lock(&piDev_g.lockListCon);
	list_for_each(pCon, &piDev_g.listCon) {
		tpiControlInst *pos_inst;
		pos_inst = list_entry(pCon, tpiControlInst, list);

		if (pos_inst->tTimeoutDurationMs != 0) {
			if (ktime_compare(now, pos_inst->tTimeoutTS) > 0) {
				// set all outputs to 0
				int i;
				rt_mutex_lock(&piDev_g.lockPI);
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

int bcm2835_cpufreq_clock_property(u32 tag, u32 id, u32 * val)
{
	struct rpi_firmware *fw = rpi_firmware_get(NULL);
	struct {
		u32 id;
		u32 val;
	} packet;
	int ret;

	packet.id = id;
	packet.val = *val;
	ret = rpi_firmware_property(fw, tag, &packet, sizeof(packet));
	if (ret)
		return ret;

	*val = packet.val;

	return 0;
}

uint32_t bcm2835_cpufreq_get_clock(void)
{
	u32 rate;
	int ret;

	ret = bcm2835_cpufreq_clock_property(RPI_FIRMWARE_GET_CLOCK_RATE, VCMSG_ID_ARM_CLOCK, &rate);
	if (ret) {
		pr_err("Failed to get clock (%d)\n", ret);
		return 0;
	}

	rate /= 1000 * 1000;	//convert to MHz

	return rate;
}

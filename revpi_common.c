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

#include "common_define.h"
#include "project.h"
#include "ModGateComMain.h"
#include "piControlMain.h"
#include "piControl.h"

void revpi_led_trigger_event(u8 *led_prev, u8 led)
{
	if (*led_prev == led)
		return;

	led_trigger_event(&piDev_g.a1_green,
			  (led & PICONTROL_LED_A1_GREEN) ? LED_FULL : LED_OFF);
	led_trigger_event(&piDev_g.a1_red,
			  (led & PICONTROL_LED_A1_RED)   ? LED_FULL : LED_OFF);
	led_trigger_event(&piDev_g.a2_green,
			  (led & PICONTROL_LED_A2_GREEN) ? LED_FULL : LED_OFF);
	led_trigger_event(&piDev_g.a2_red,
			  (led & PICONTROL_LED_A2_RED)   ? LED_FULL : LED_OFF);

	*led_prev = led;
}

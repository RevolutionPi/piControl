/*
 * revpi_core.c - RevPi Core specific functions
 *
 * Copyright (C) 2017 KUNBUS GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>	// included for all kernel modules
#include <linux/kernel.h>	// included for KERN_INFO
#include <linux/init.h>		// included for __init and __exit macros
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <asm/uaccess.h>
#include <asm/elf.h>
#include <linux/semaphore.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kthread.h>
#include <linux/hrtimer.h>
#include <linux/delay.h>
#include <linux/rtmutex.h>
#include <linux/sem.h>
#include <linux/gpio.h>
#include <linux/wait.h>
#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/thermal.h>
#include <linux/of.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio/machine.h>
#include <asm/div64.h>

#include "common_define.h"
#include "project.h"

#include "revpi_common.h"
#include "revpi_core.h"
#include "compat.h"

static const struct kthread_prio revpi_core_kthread_prios[] = {
	/* spi pump to RevPi Gateways */
	{ .comm = "spi0",		.prio = MAX_USER_RT_PRIO/2 + 4 },
	{ }
};

static struct gpiod_lookup_table revpi_core_gpios = {
	.dev_id = "piControl0",
	.table  = { GPIO_LOOKUP_IDX("pinctrl-bcm2835", 42, "Sniff1A", 0, GPIO_ACTIVE_HIGH),	// 1 links
		    GPIO_LOOKUP_IDX("pinctrl-bcm2835", 43, "Sniff1B", 0, GPIO_ACTIVE_HIGH),	// 1 rechts
		    GPIO_LOOKUP_IDX("pinctrl-bcm2835", 28, "Sniff2A", 0, GPIO_ACTIVE_HIGH),	// 2 links
		    GPIO_LOOKUP_IDX("pinctrl-bcm2835", 29, "Sniff2B", 0, GPIO_ACTIVE_HIGH),	// 2 rechts
	},
};

// im Schaltplan heißen die Pins 1B und 2B, da sie links sind, müssten sie aber 1A und 1B heißen.
// hier in der Software werden sie mit A benannt
static struct gpiod_lookup_table revpi_connect_gpios = {
	.dev_id = "piControl0",
	.table  = { GPIO_LOOKUP_IDX("pinctrl-bcm2835", 43, "Sniff1A",	0, GPIO_ACTIVE_HIGH),	// 1 links
		    GPIO_LOOKUP_IDX("pinctrl-bcm2835", 29, "Sniff2A",	0, GPIO_ACTIVE_HIGH),	// 2 links
		    GPIO_LOOKUP_IDX("pinctrl-bcm2835",  0, "X2_DI",	0, GPIO_ACTIVE_HIGH),	// Digital In auf X2 Stecker
		    GPIO_LOOKUP_IDX("pinctrl-bcm2835",  1, "X2_DO",	0, GPIO_ACTIVE_HIGH),	// Digital Out (Relais) auf X2 Stecker
		    GPIO_LOOKUP_IDX("pinctrl-bcm2835", 42, "WDTrigger", 0, GPIO_ACTIVE_HIGH),	// Watchdog trigger
	},
};

SRevPiCore piCore_g;

/**
 * revpi_core_find_gate() - find RevPiDevice for given netdev
 * @netdev: network device used to communicate with a RevPi Gate
 * @module_type: module type of the RevPi Gate
 */
u8 revpi_core_find_gate(struct net_device *netdev, u16 module_type)
{
	int i;

	if (piDev_g.machine_type == REVPI_CORE &&
	    !strcmp(dev_name(netdev->dev.parent), "spi0.1")) {
		if (piCore_g.i8uRightMGateIdx == REV_PI_DEV_UNDEF) {
			/*
			 * The gateway was not discoverable via RS-485,
			 * but the PiBridge Ethernet communication is up.
			 * Search for the gateway's config entry.
			 */
			pr_info("search for right mGate %d\n", module_type);
			i = RevPiDevice_find_by_side_and_type(true,
				     module_type | PICONTROL_NOT_CONNECTED);
			if (i != REV_PI_DEV_UNDEF) {
				pr_info("found mGate %d\n", i);
				piCore_g.i8uRightMGateIdx = i;
				RevPiDevice_getDev(i)->i8uActive = 1;
				RevPiDevice_getDev(i)->sId.i16uModulType &=
					       PICONTROL_NOT_CONNECTED_MASK;
			}
		}
		return piCore_g.i8uRightMGateIdx;
	}

	if ((piDev_g.machine_type == REVPI_CORE &&
	     !strcmp(dev_name(netdev->dev.parent), "spi0.0")) ||
	    (piDev_g.machine_type == REVPI_CONNECT &&
	     !strcmp(dev_name(netdev->dev.parent), "spi0.1"))) {
		if (piCore_g.i8uLeftMGateIdx == REV_PI_DEV_UNDEF) {
			pr_info("search for left mGate %d\n", module_type);
			i = RevPiDevice_find_by_side_and_type(false,
				    module_type | PICONTROL_NOT_CONNECTED);
			if (i != REV_PI_DEV_UNDEF) {
				pr_info("found mGate %d\n", i);
				piCore_g.i8uLeftMGateIdx = i;
				RevPiDevice_getDev(i)->i8uActive = 1;
				RevPiDevice_getDev(i)->sId.i16uModulType &=
					      PICONTROL_NOT_CONNECTED_MASK;
			}
		}
		return piCore_g.i8uLeftMGateIdx;
	}

	return REV_PI_DEV_UNDEF;
}

/**
 * revpi_core_gate_connected() - react to state change of gateway connection
 * @idx: RevPiDevice index of gateway
 * @connected: new state of gateway connection
 */
void revpi_core_gate_connected(SDevice *revpi_dev, bool connected)
{
	u8 status = 0;

	if (!revpi_dev)
		return;

	if (revpi_dev == RevPiDevice_getDev(piCore_g.i8uLeftMGateIdx))
		status = PICONTROL_STATUS_LEFT_GATEWAY;
	else if (revpi_dev == RevPiDevice_getDev(piCore_g.i8uRightMGateIdx))
		status = PICONTROL_STATUS_RIGHT_GATEWAY;

	if (connected)
		RevPiDevice_setStatus(0, status);
	else
		RevPiDevice_setStatus(status, 0);
}

static enum hrtimer_restart piIoTimer(struct hrtimer *pTimer)
{
	up(&piCore_g.ioSem);
	return HRTIMER_NORESTART;
}

static int piIoThread(void *data)
{
	//TODO int value = 0;
	ktime_t time;
	ktime_t now;
	s64 tDiff;

	hrtimer_init(&piCore_g.ioTimer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
	piCore_g.ioTimer.function = piIoTimer;

	pr_info("piIO thread started\n");

	now = hrtimer_cb_get_time(&piCore_g.ioTimer);

	PiBridgeMaster_Reset();

	while (!kthread_should_stop()) {
		if (PiBridgeMaster_Run() < 0)
			break;

		time = now;
		now = hrtimer_cb_get_time(&piCore_g.ioTimer);

		time = ktime_sub(now, time);
		piCore_g.image.drv.i8uIOCycle = ktime_to_ms(time);

		if (!ktime_equal(piDev_g.tLastOutput1, piDev_g.tLastOutput2)) {
			tDiff = ktime_to_ns(ktime_sub(piDev_g.tLastOutput1, piDev_g.tLastOutput2));
			tDiff = tDiff << 1;	// multiply by 2
			if (ktime_to_ns(ktime_sub(now, piDev_g.tLastOutput1)) > tDiff && isRunning()) {
				int i;
				// the outputs were not written by logiCAD for more than twice the normal period
				// the logiRTS must have been stopped or crashed
				// -> set all outputs to 0
				pr_info("logiRTS timeout, set all output to 0\n");
				if (piDev_g.stopIO == false) {
					my_rt_mutex_lock(&piDev_g.lockPI);
					for (i = 0; i < piDev_g.cl->i16uNumEntries; i++) {
						uint16_t len = piDev_g.cl->ent[i].i16uLength;
						uint16_t addr = piDev_g.cl->ent[i].i16uAddr;

						if (len >= 8) {
							len /= 8;
							memset(piDev_g.ai8uPI + addr, 0, len);
						} else {
							uint8_t val;
							uint8_t mask = piDev_g.cl->ent[i].i8uBitMask;

							val = piDev_g.ai8uPI[addr];
							val &= ~mask;
							piDev_g.ai8uPI[addr] = val;
						}
					}
					rt_mutex_unlock(&piDev_g.lockPI);
				}
				piDev_g.tLastOutput1 = ktime_set(0, 0);
				piDev_g.tLastOutput2 = ktime_set(0, 0);
			}
		}

		revpi_check_timeout();

		if (piCore_g.eBridgeState == piBridgeInit) {
			time = ktime_add_ns(time, INTERVAL_RS485);
		} else {
			time = ktime_add_ns(time, INTERVAL_IO_COMM);
		}

		if (ktime_after(now, time)) {
			// the call of PiBridgeMaster_Run() needed more time than the INTERVAL
			// -> wait an additional ms
			//pr_info("%d ms too late, state %d\n", (int)((now.tv64 - time.tv64) >> 20), piCore_g.eBridgeState);
			time = ktime_add_ns(now, INTERVAL_ADDITIONAL);
		}

		hrtimer_start(&piCore_g.ioTimer, time, HRTIMER_MODE_ABS);
		down(&piCore_g.ioSem);	// wait for timer
	}

	RevPiDevice_finish();

	pr_info("piIO exit\n");
	return 0;
}

int revpi_core_init(void)
{
	struct sched_param param;
	int ret = 0;

	piCore_g.i8uLeftMGateIdx = REV_PI_DEV_UNDEF;
	piCore_g.i8uRightMGateIdx = REV_PI_DEV_UNDEF;

	if (piDev_g.machine_type == REVPI_CORE) {
		// the Core has two modular gateway ports
		gpiod_add_lookup_table(&revpi_core_gpios);
	} else if (piDev_g.machine_type == REVPI_CONNECT) {
		// the connect has only a only one modular gateway port on the left
		gpiod_add_lookup_table(&revpi_connect_gpios);
	}

	rt_mutex_init(&piCore_g.lockUserTel);
	sema_init(&piCore_g.semUserTel, 0);
	piCore_g.pendingUserTel = false;

	rt_mutex_init(&piCore_g.lockGateTel);
	sema_init(&piCore_g.semGateTel, 0);
	piCore_g.pendingGateTel = false;

	rt_mutex_init(&piCore_g.lockBridgeState);
	sema_init(&piCore_g.ioSem, 0);

	piCore_g.gpio_sniff1a = gpiod_get(piDev_g.dev, "Sniff1A", GPIOD_IN);
	if (IS_ERR(piCore_g.gpio_sniff1a)) {
		pr_err("cannot acquire gpio sniff 1a\n");
		ret = PTR_ERR(piCore_g.gpio_sniff1a);
		goto err_remove_table;
	}
	piCore_g.gpio_sniff2a = gpiod_get(piDev_g.dev, "Sniff2A", GPIOD_IN);
	if (IS_ERR(piCore_g.gpio_sniff2a)) {
		pr_err("cannot acquire gpio sniff 2a\n");
		ret = PTR_ERR(piCore_g.gpio_sniff2a);
		goto err_gpiod_put;
	}
	if (piDev_g.machine_type == REVPI_CORE) {
		piCore_g.gpio_sniff1b = gpiod_get(piDev_g.dev, "Sniff1B", GPIOD_IN);
		if (IS_ERR(piCore_g.gpio_sniff1b)) {
			pr_err("cannot acquire gpio sniff 1b\n");
			ret = PTR_ERR(piCore_g.gpio_sniff1b);
			goto err_gpiod_put;
		}
		piCore_g.gpio_sniff2b = gpiod_get(piDev_g.dev, "Sniff2B", GPIOD_IN);
		if (IS_ERR(piCore_g.gpio_sniff2b)) {
			pr_err("cannot acquire gpio sniff 2b\n");
			ret = PTR_ERR(piCore_g.gpio_sniff2b);
			goto err_gpiod_put;
		}
	}
	if (piDev_g.machine_type == REVPI_CONNECT) {
		piCore_g.gpio_x2di = gpiod_get(piDev_g.dev, "X2_DI", GPIOD_IN);
		if (IS_ERR(piCore_g.gpio_x2di)) {
			pr_err("cannot acquire gpio x2 di\n");
			ret = PTR_ERR(piCore_g.gpio_x2di);
			goto err_gpiod_put;
		}
		piCore_g.gpio_x2do = gpiod_get(piDev_g.dev, "X2_DO", GPIOD_OUT_LOW);
		if (IS_ERR(piCore_g.gpio_x2do)) {
			pr_err("cannot acquire gpio x2 do\n");
			ret = PTR_ERR(piCore_g.gpio_x2do);
			goto err_gpiod_put;
		}
		piCore_g.gpio_wdtrigger = gpiod_get(piDev_g.dev, "WDTrigger", GPIOD_OUT_LOW);
		if (IS_ERR(piCore_g.gpio_wdtrigger)) {
			pr_err("cannot acquire gpio watchdog trigger\n");
			ret = PTR_ERR(piCore_g.gpio_wdtrigger);
			goto err_gpiod_put;
		}
	}

	if (piIoComm_init()) {
		pr_err("open serial port failed\n");
		ret = -EFAULT;
		goto err_gpiod_put;
	}

	/* run threads */
	ret = set_kthread_prios(revpi_core_kthread_prios);
	if (ret)
		goto err_close_serial;

	piCore_g.pUartThread = kthread_run(&UartThreadProc, (void *)NULL, "piControl Uart");
	if (IS_ERR(piCore_g.pUartThread)) {
		pr_err("kthread_run(uart) failed\n");
		ret = PTR_ERR(piCore_g.pUartThread);
		goto err_close_serial;
	}
	param.sched_priority = RT_PRIO_UART;
	sched_setscheduler(piCore_g.pUartThread, SCHED_FIFO, &param);
	if (ret) {
		pr_err("cannot set rt prio of uart thread\n");
		goto err_stop_uart_thread;
	}

	piCore_g.pIoThread = kthread_run(&piIoThread, NULL, "piControl I/O");
	if (IS_ERR(piCore_g.pIoThread)) {
		pr_err("kthread_run(io) failed\n");
		ret = PTR_ERR(piCore_g.pIoThread);
		goto err_stop_uart_thread;
	}
	param.sched_priority = RT_PRIO_BRIDGE;
	ret = sched_setscheduler(piCore_g.pIoThread, SCHED_FIFO, &param);
	if (ret) {
		pr_err("cannot set rt prio of io thread\n");
		goto err_stop_io_thread;
	}

	return ret;

err_stop_io_thread:
	kthread_stop(piCore_g.pIoThread);
err_stop_uart_thread:
	kthread_stop(piCore_g.pUartThread);
err_close_serial:
	piIoComm_finish();
err_gpiod_put:
	if (!IS_ERR_OR_NULL(piCore_g.gpio_sniff1a))
		gpiod_put(piCore_g.gpio_sniff1a);
	if (!IS_ERR_OR_NULL(piCore_g.gpio_sniff2a))
		gpiod_put(piCore_g.gpio_sniff2a);
	if (piDev_g.machine_type == REVPI_CORE) {
		if (!IS_ERR_OR_NULL(piCore_g.gpio_sniff1b))
			gpiod_put(piCore_g.gpio_sniff1b);
		if (!IS_ERR_OR_NULL(piCore_g.gpio_sniff2b))
			gpiod_put(piCore_g.gpio_sniff2b);
	} else if (piDev_g.machine_type == REVPI_CONNECT) {
		if (!IS_ERR_OR_NULL(piCore_g.gpio_x2di))
			gpiod_put(piCore_g.gpio_x2di);
		if (!IS_ERR_OR_NULL(piCore_g.gpio_x2do))
			gpiod_put(piCore_g.gpio_x2do);
		if (!IS_ERR_OR_NULL(piCore_g.gpio_wdtrigger))
			gpiod_put(piCore_g.gpio_wdtrigger);
	}
err_remove_table:
	if (piDev_g.machine_type == REVPI_CORE)
		gpiod_remove_lookup_table(&revpi_core_gpios);
	else if (piDev_g.machine_type == REVPI_CONNECT)
		gpiod_remove_lookup_table(&revpi_connect_gpios);

	return ret;
}

void revpi_core_fini(void)
{
	// the IoThread cannot be stopped
	kthread_stop(piCore_g.pIoThread);
	kthread_stop(piCore_g.pUartThread);
	piIoComm_finish();

	/* reset GPIO direction */
	piIoComm_writeSniff1A(enGpioValue_Low, enGpioMode_Input);
	gpiod_put(piCore_g.gpio_sniff1a);
	piIoComm_writeSniff2A(enGpioValue_Low, enGpioMode_Input);
	gpiod_put(piCore_g.gpio_sniff2a);
	if (piDev_g.machine_type == REVPI_CORE) {
		piIoComm_writeSniff1B(enGpioValue_Low, enGpioMode_Input);
		gpiod_put(piCore_g.gpio_sniff1b);
		piIoComm_writeSniff2B(enGpioValue_Low, enGpioMode_Input);
		gpiod_put(piCore_g.gpio_sniff2b);
	} else if (piDev_g.machine_type == REVPI_CONNECT) {
		gpiod_put(piCore_g.gpio_x2di);
		gpiod_put(piCore_g.gpio_x2do);
		gpiod_put(piCore_g.gpio_wdtrigger);
	}

	if (piDev_g.machine_type == REVPI_CORE)
		gpiod_remove_lookup_table(&revpi_core_gpios);
	else if (piDev_g.machine_type == REVPI_CONNECT)
		gpiod_remove_lookup_table(&revpi_connect_gpios);
}

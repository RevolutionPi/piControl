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
	{ .comm = "spi0",		.prio = MAX_RT_PRIO/2 + 4 },
	{ }
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

	pr_info("piIO exit\n");
	return 0;
}

static void deinit_sniff_gpios(void)
{
	/* reset GPIO direction */
	piIoComm_writeSniff1A(enGpioValue_Low, enGpioMode_Input);
	piIoComm_writeSniff2A(enGpioValue_Low, enGpioMode_Input);

	if (piDev_g.machine_type == REVPI_CORE ||
	    piDev_g.machine_type == REVPI_CORE_SE) {
		piIoComm_writeSniff1B(enGpioValue_Low, enGpioMode_Input);
		piIoComm_writeSniff2B(enGpioValue_Low, enGpioMode_Input);
	}
}

static void deinit_gpios(void)
{
	deinit_sniff_gpios();
}

static int init_sniff_gpios(struct platform_device *pdev)
{
	struct gpio_descs *descs;

	descs = devm_gpiod_get_array(&pdev->dev, "left-sniff", GPIOD_IN);
	if (IS_ERR(descs)) {
		dev_err(&pdev->dev, "Failed to get left sniff gpios\n");
		return PTR_ERR(descs);
	}

	if (descs->ndescs != 2) {
		dev_err(&pdev->dev, "Invalid number of left sniff gpios (%u)\n",
			descs->ndescs);
		return -ENXIO;
	}
	/*
	 * Note: in the schematic for the connect these pins are named 1B
	 * and 2B although since they are for the left side, they should
	 * rather be named 1A and 1B. However here in software we name them
	 * 1a and 2a for both connect and core.
	 */
	piCore_g.gpio_sniff1a = descs->desc[0];
	piCore_g.gpio_sniff2a = descs->desc[1];

	if (piDev_g.machine_type == REVPI_CORE ||
	    piDev_g.machine_type == REVPI_CORE_SE) {
		descs = devm_gpiod_get_array(&pdev->dev, "right-sniff", GPIOD_IN);
		if (IS_ERR(descs)) {
			dev_err(&pdev->dev, "Failed to get right sniff gpios\n");
			return PTR_ERR(descs);
		}

		if (descs->ndescs != 2) {
			dev_err(&pdev->dev, "Invalid number of right sniff gpios (%u)\n",
				descs->ndescs);
			return -ENXIO;
		}
		piCore_g.gpio_sniff1b = descs->desc[0];
		piCore_g.gpio_sniff2b = descs->desc[1];
	}

	return 0;
}

static int init_connect_gpios(struct platform_device *pdev)
{
	piCore_g.gpio_x2di = devm_gpiod_get_index(&pdev->dev, "connect", 0, GPIOD_IN);
	if (IS_ERR(piCore_g.gpio_x2di)) {
		dev_err(&pdev->dev, "cannot acquire gpio x2 di\n");
		return PTR_ERR(piCore_g.gpio_x2di);
	}

	piCore_g.gpio_x2do = devm_gpiod_get_index(&pdev->dev, "connect", 1, GPIOD_OUT_LOW);
	if (IS_ERR(piCore_g.gpio_x2do)) {
		dev_err(&pdev->dev, "cannot acquire gpio x2 do\n");
		return PTR_ERR(piCore_g.gpio_x2do);
	}

	piCore_g.gpio_wdtrigger = devm_gpiod_get_index(&pdev->dev, "connect", 2, GPIOD_OUT_LOW);
	if (IS_ERR(piCore_g.gpio_wdtrigger)) {
		dev_err(&pdev->dev, "cannot acquire gpio watchdog trigger\n");
		return PTR_ERR(piCore_g.gpio_wdtrigger);
	}
	return 0;
}

static int init_gpios(struct platform_device *pdev)
{
	int ret;

	ret = init_sniff_gpios(pdev);
	if (ret) {
		dev_err(piDev_g.dev, "Failed to init sniff gpios: %i\n", ret);
		return ret;
	}

	if (piDev_g.machine_type == REVPI_CONNECT ||
	    piDev_g.machine_type == REVPI_CONNECT_SE) {
		ret = init_connect_gpios(pdev);
		if (ret) {
			dev_err(piDev_g.dev, "Failed to init connect gpios: %i\n",
				ret);
			goto err_deinit_sniff_gpios;
		}
	}
	return 0;

err_deinit_sniff_gpios:
	deinit_sniff_gpios();
	return ret;
}

static int pibridge_probe(struct platform_device *pdev)
{
	struct sched_param param;
	int ret;

	piCore_g.i8uLeftMGateIdx = REV_PI_DEV_UNDEF;
	piCore_g.i8uRightMGateIdx = REV_PI_DEV_UNDEF;

	ret = init_gpios(pdev);
	if (ret) {
		dev_err(piDev_g.dev, "Failed to init gpios: %i\n", ret);
		return ret;
	}

	rt_mutex_init(&piCore_g.lockUserTel);
	sema_init(&piCore_g.semUserTel, 0);
	piCore_g.pendingUserTel = false;

	rt_mutex_init(&piCore_g.lockGateTel);
	sema_init(&piCore_g.semGateTel, 0);
	piCore_g.pendingGateTel = false;

	rt_mutex_init(&piCore_g.lockBridgeState);
	sema_init(&piCore_g.ioSem, 0);

	piCore_g.fw = revpi_get_firmware();
	if (!piCore_g.fw) {
		dev_err(piDev_g.dev, "Failed to get revpi firmware\n");
		goto err_deinit_gpios;
	}

	/* run thread */
	ret = set_kthread_prios(revpi_core_kthread_prios);
	if (ret)
		goto err_release_fw;

	piCore_g.pIoThread = kthread_run(&piIoThread, NULL, "piControl I/O");
	if (IS_ERR(piCore_g.pIoThread)) {
		pr_err("kthread_run(io) failed\n");
		ret = PTR_ERR(piCore_g.pIoThread);
		goto err_release_fw;
	}
	param.sched_priority = RT_PRIO_BRIDGE;
	ret = sched_setscheduler(piCore_g.pIoThread, SCHED_FIFO, &param);
	if (ret) {
		pr_err("cannot set rt prio of io thread\n");
		goto err_stop_io_thread;
	}

	return 0;

err_stop_io_thread:
	kthread_stop(piCore_g.pIoThread);
err_release_fw:
	revpi_release_firmware(piCore_g.fw);
err_deinit_gpios:
	deinit_gpios();

	return ret;
}

static int pibridge_remove(struct platform_device *pdev)
{
	kthread_stop(piCore_g.pIoThread);
	revpi_release_firmware(piCore_g.fw);
	deinit_gpios();

	return 0;
}

static const struct of_device_id of_pibridge_match[] = {
	{ .compatible = "kunbus,pibridge", },
	{},
};

static struct platform_driver pibridge_driver = {
	.probe		= pibridge_probe,
	.remove 	= pibridge_remove,
	.driver		= {
		.name	= "pibridge",
		.of_match_table = of_pibridge_match,
	},
};

int revpi_core_init(void)
{
	int ret;

	ret = platform_driver_register(&pibridge_driver);
	if (ret)
		pr_err("failed to register pibridge driver: %i\n", ret);

	return ret;
}

void revpi_core_fini(void)
{
	platform_driver_unregister(&pibridge_driver);
}

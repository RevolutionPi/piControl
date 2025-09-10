// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: 2017-2024 KUNBUS GmbH

// revpi_core.c - RevPi Core specific functions

#include <linux/gpio.h>
#include <linux/mod_devicetable.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/platform_device.h>

#include "revpi_common.h"
#include "revpi_core.h"

#define CREATE_TRACE_POINTS
#include "picontrol_trace.h"

/*
 * The priority has to be at least 54 (same as SPI thread), otherwise lots
 * of UART reception errors are observed. The reason for this effect is not
 * yet completely clear, presumably the UART has to switch as quickly as
 * possible from data transmission to data reception to avoid errors during
 * piBridge communication.
 */
#define RT_PRIO_BRIDGE  (MAX_RT_PRIO/2 + 4)

extern unsigned int picontrol_max_cycle_deviation;

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

	if (!strcmp(netdev->name, "piright")) {
		if (piCore_g.i8uRightMGateIdx == REV_PI_DEV_UNDEF) {
			/*
			 * The gateway was not discoverable via RS-485,
			 * but the PiBridge Ethernet communication is up.
			 * Search for the gateway's config entry.
			 */
			pr_info("%s: found uninitialized gateway of type %d, try to find configuration\n",
				netdev->name, module_type);
			i = RevPiDevice_find_by_side_and_type(true,
				     module_type | PICONTROL_NOT_CONNECTED);
			if (i != REV_PI_DEV_UNDEF) {
				pr_info("%s: found configuration for uninitialized gateway\n",
					netdev->name);
				piCore_g.i8uRightMGateIdx = i;
				RevPiDevice_getDev(i)->i8uActive = 1;
				RevPiDevice_getDev(i)->sId.i16uModulType &=
					       PICONTROL_NOT_CONNECTED_MASK;
			}
		}
		return piCore_g.i8uRightMGateIdx;
	}

	if (!strcmp(netdev->name, "pileft")) {
		if (piCore_g.i8uLeftMGateIdx == REV_PI_DEV_UNDEF) {
			pr_info("%s: found uninitialized gateway of type %d, try to find configuration\n",
				netdev->name, module_type);
			i = RevPiDevice_find_by_side_and_type(false,
				    module_type | PICONTROL_NOT_CONNECTED);
			if (i != REV_PI_DEV_UNDEF) {
				pr_info("%s: found configuration for uninitialized gateway\n",
					netdev->name);
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
	struct picontrol_cycle *cycle = &piDev_g.cycle;
	unsigned int last_cycle;
	unsigned int min_cycle;
	unsigned int max_cycle;
	ktime_t time;
	ktime_t now;
	s64 tDiff;

	/* Note: we use this timer for both, a fixed cycle interval length and
	   measurement of the cycle time */
	hrtimer_init(&piCore_g.ioTimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);

	piCore_g.ioTimer.function = piIoTimer;

	pr_debug("piIO thread started\n");

	PiBridgeMaster_Reset();

	/* Get start time of the first cycle */
	now = hrtimer_cb_get_time(&piCore_g.ioTimer);

	while (!kthread_should_stop()) {
		trace_picontrol_cycle_start(piCore_g.cycle_num);

		if (PiBridgeMaster_Run() < 0)
			break;

		time = now;
		now = hrtimer_cb_get_time(&piCore_g.ioTimer);

		last_cycle = ktime_to_us(ktime_sub(now, time));

		/* On process image store cycle time in msec */
		piCore_g.image.drv.i8uIOCycle = last_cycle / 1000;

		if (piDev_g.tLastOutput1 != piDev_g.tLastOutput2) {
			tDiff = ktime_to_ns(ktime_sub(piDev_g.tLastOutput1, piDev_g.tLastOutput2));
			tDiff = tDiff << 1;	// multiply by 2
			if (ktime_to_ns(ktime_sub(now, piDev_g.tLastOutput1)) > tDiff && isRunning()) {
				int i;
				// the outputs were not written by logiCAD for more than twice the normal period
				// the logiRTS must have been stopped or crashed
				// -> set all outputs to 0
				pr_info("logiRTS timeout, set all output to 0\n");
				if (!test_bit(PICONTROL_DEV_FLAG_STOP_IO,
					&piDev_g.flags)) {
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

		hrtimer_forward(&piCore_g.ioTimer, now,
				ns_to_ktime(piControl_get_cycle_duration() * 1000));
		hrtimer_restart(&piCore_g.ioTimer);
		down(&piCore_g.ioSem);	// wait for timer

		if (piCore_g.data_exchange_running) {
			write_seqlock(&cycle->lock);
			cycle->last = last_cycle;

			if (cycle->min > last_cycle)
				cycle->min = last_cycle;

			if (cycle->max < last_cycle)
				cycle->max = last_cycle;

			max_cycle = cycle->max;
			min_cycle = cycle->min;
			write_sequnlock(&cycle->lock);

			if (picontrol_max_cycle_deviation &&
			    (last_cycle > (min_cycle +
					picontrol_max_cycle_deviation))) {
				trace_picontrol_cycle_exceeded(last_cycle,
							       min_cycle);
			}

			trace_picontrol_cycle_end(piCore_g.cycle_num, last_cycle);
			piCore_g.cycle_num++;
		}
	}

	pr_info("piIO exit\n");
	pr_info("Number of cycles: %llu\n", piCore_g.cycle_num);
	pr_info("Cycle min: %u usecs\n", min_cycle);
	pr_info("Cycle max: %u usecs\n", max_cycle);

	return 0;
}

static void deinit_sniff_gpios(void)
{
	/* reset GPIO direction */
	piIoComm_writeSniff1A(enGpioValue_Low, enGpioMode_Input);
	piIoComm_writeSniff2A(enGpioValue_Low, enGpioMode_Input);

	if (!piDev_g.only_left_pibridge) {
		piIoComm_writeSniff1B(enGpioValue_Low, enGpioMode_Input);
		piIoComm_writeSniff2B(enGpioValue_Low, enGpioMode_Input);
	}
}

static void deinit_gpios(void)
{
	deinit_sniff_gpios();
}

static int init_pbswitch_gpios(struct platform_device *pdev)
{
	struct gpio_desc *desc;

	desc = devm_gpiod_get_index_optional(&pdev->dev, "left-pbswitch",
					     REVPI_PIBRIDGE_ETHERNET_GPIO_MPX,
					     GPIOD_OUT_LOW);
	if (desc) {
		if (IS_ERR(desc)) {
			dev_err(&pdev->dev,
				"cannot acquire left pbswitch-mpx gpio\n");
			return PTR_ERR(desc);
		}
		piCore_g.gpio_pbswitch_mpx_left = desc;

		desc = devm_gpiod_get_index(&pdev->dev, "left-pbswitch",
					    REVPI_PIBRIDGE_ETHERNET_GPIO_DETECT,
					    GPIOD_IN);
		if (IS_ERR(desc)) {
			dev_err(&pdev->dev,
				"cannot acquire left pbswitch-detect gpio\n");
			return PTR_ERR(desc);
		}
		piCore_g.gpio_pbswitch_detect_left = desc;
	}

	desc = devm_gpiod_get_index_optional(&pdev->dev, "right-pbswitch",
					     REVPI_PIBRIDGE_ETHERNET_GPIO_MPX,
					     GPIOD_OUT_LOW);
	if (desc) {
		if (IS_ERR(desc)) {
			dev_err(&pdev->dev,
				"cannot acquire right pbswitch-mpx gpio\n");
			return PTR_ERR(desc);
		}
		piCore_g.gpio_pbswitch_mpx_right = desc;

		desc = devm_gpiod_get_index(&pdev->dev, "right-pbswitch",
					    REVPI_PIBRIDGE_ETHERNET_GPIO_DETECT,
					    GPIOD_IN);
		if (IS_ERR(desc)) {
			dev_err(&pdev->dev,
				"cannot acquire right pbswitch-detect gpio\n");
			return PTR_ERR(desc);
		}
		piCore_g.gpio_pbswitch_detect_right = desc;
	}

	return 0;
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

	if (!piDev_g.only_left_pibridge) {
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

	/*
	 * The Sniff2A pin has no external pull-down. It can happen that the pin
	 * keeps its capacity for a longer time after the pin has been driven high.
	 * Thus we might detect a module even if none is connected. Configure a
	 * pull-down on the pin to drain any capacity.
	 */
	gpiod_set_config(
		piCore_g.gpio_sniff2a,
		pinconf_to_config_packed(PIN_CONFIG_BIAS_PULL_DOWN, 0));

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

	/* WD trigger is not needed on Connect 4 */
	if (piDev_g.machine_type == REVPI_CONNECT ||
	    piDev_g.machine_type == REVPI_CONNECT_SE) {
		piCore_g.gpio_wdtrigger = devm_gpiod_get_index(&pdev->dev, "connect", 2, GPIOD_OUT_LOW);
		if (IS_ERR(piCore_g.gpio_wdtrigger)) {
			dev_err(&pdev->dev, "cannot acquire gpio watchdog trigger\n");
			return PTR_ERR(piCore_g.gpio_wdtrigger);
		}
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
	    piDev_g.machine_type == REVPI_CONNECT_SE ||
	    piDev_g.machine_type == REVPI_CONNECT_4) {
		ret = init_connect_gpios(pdev);
		if (ret) {
			dev_err(piDev_g.dev, "Failed to init connect gpios: %i\n",
				ret);
			goto err_deinit_sniff_gpios;
		}
	}

	ret = init_pbswitch_gpios(pdev);
	if (ret) {
		dev_err(piDev_g.dev, "Failed to init pbswitch gpios: %i\n", ret);
		goto err_deinit_sniff_gpios;
	}

	return 0;

err_deinit_sniff_gpios:
	deinit_sniff_gpios();
	return ret;
}

static int pibridge_probe(struct platform_device *pdev)
{
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

	/* set rt prio for spi PiBridge interfaces on RevPi Core or Connect */
	if (piDev_g.revpi_gate_supported && (piDev_g.machine_type == REVPI_CORE || piDev_g.machine_type == REVPI_CONNECT )) {
		ret = set_kthread_prios(revpi_core_kthread_prios);
		if (ret)
			goto err_deinit_gpios;
	}

	piCore_g.pIoThread = kthread_run(&piIoThread, NULL, "piControl I/O");
	if (IS_ERR(piCore_g.pIoThread)) {
		pr_err("kthread_run(io) failed\n");
		ret = PTR_ERR(piCore_g.pIoThread);
		goto err_deinit_gpios;
	}
	ret = set_rt_priority(piCore_g.pIoThread, RT_PRIO_BRIDGE);
	if (ret) {
		pr_err("cannot set rt prio of io thread\n");
		goto err_stop_io_thread;
	}

	return 0;

err_stop_io_thread:
	kthread_stop(piCore_g.pIoThread);
err_deinit_gpios:
	deinit_gpios();

	return ret;
}

#if KERNEL_VERSION(6, 11, 0) <= LINUX_VERSION_CODE
static void pibridge_remove(struct platform_device *pdev)
#else
static int pibridge_remove(struct platform_device *pdev)
#endif
{
	kthread_stop(piCore_g.pIoThread);
	deinit_gpios();

#if KERNEL_VERSION(6, 11, 0) > LINUX_VERSION_CODE
	return 0;
#endif
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

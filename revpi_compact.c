/*
 * revpi_compact.c - RevPi Compact specific handling
 *
 * Copyright (C) 2017 KUNBUS GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#include <linux/gpio/consumer.h>
#include <linux/gpio/machine.h>
#include <linux/iio/consumer.h>
#include <linux/iio/driver.h>
#include <linux/iio/iio.h>
#include <linux/iio/machine.h>
#include <linux/kthread.h>
#include <linux/spi/max3191x.h>
#include <linux/spi/spi.h>
#include "project.h"
#include "common_define.h"
#include "ModGateComMain.h"
#include "PiBridgeMaster.h"
#include "piControlMain.h"
#include "process_image.h"
#include "pt100.h"
#include "revpi_common.h"

#define REVPI_COMPACT_IO_CYCLE 1000 * NSEC_PER_USEC
#define REVPI_COMPACT_AIN_CYCLE 125 * NSEC_PER_MSEC

struct revpi_compact {
	struct revpi_compact_image image;
	struct revpi_compact_config config;
	struct task_struct *io_thread;
	struct task_struct *ain_thread;
	struct device *din_dev;
	struct gpio_desc *dout_fault;
	struct gpio_descs *din;
	struct gpio_descs *dout;
	struct iio_dev *ain_dev, *aout_dev;
	struct iio_channel *ain;
	struct iio_channel *aout[2];
};

static struct gpiod_lookup_table revpi_compact_gpios = {
	.dev_id = "piControl0",
	.table  = { GPIO_LOOKUP_IDX("max31913", 0, "din", 0, 0),
		    GPIO_LOOKUP_IDX("max31913", 1, "din", 1, 0),
		    GPIO_LOOKUP_IDX("max31913", 2, "din", 2, 0),
		    GPIO_LOOKUP_IDX("max31913", 3, "din", 3, 0),
		    GPIO_LOOKUP_IDX("max31913", 4, "din", 4, 0),
		    GPIO_LOOKUP_IDX("max31913", 5, "din", 5, 0),
		    GPIO_LOOKUP_IDX("max31913", 6, "din", 6, 0),
		    GPIO_LOOKUP_IDX("max31913", 7, "din", 7, 0),
		    GPIO_LOOKUP_IDX("74hc595", 0, "dout", 0, 0),
		    GPIO_LOOKUP_IDX("74hc595", 1, "dout", 1, 0),
		    GPIO_LOOKUP_IDX("74hc595", 2, "dout", 2, 0),
		    GPIO_LOOKUP_IDX("74hc595", 3, "dout", 3, 0),
		    GPIO_LOOKUP_IDX("74hc595", 4, "dout", 4, 0),
		    GPIO_LOOKUP_IDX("74hc595", 5, "dout", 5, 0),
		    GPIO_LOOKUP_IDX("74hc595", 6, "dout", 6, 0),
		    GPIO_LOOKUP_IDX("74hc595", 7, "dout", 7, 0),
		  },
};

static const struct iio_map revpi_compact_ain[] = {
	IIO_MAP("ain0", "piControl0", "ain0"),
	IIO_MAP("ain1", "piControl0", "ain1"),
	IIO_MAP("ain2", "piControl0", "ain2"),
	IIO_MAP("ain3", "piControl0", "ain3"),
	IIO_MAP("ain4", "piControl0", "ain4"),
	IIO_MAP("ain5", "piControl0", "ain5"),
	IIO_MAP("ain6", "piControl0", "ain6"),
	IIO_MAP("ain7", "piControl0", "ain7"),
	IIO_MAP("ain0_rtd", "piControl0", "ain0_rtd"),
	IIO_MAP("ain1_rtd", "piControl0", "ain1_rtd"),
	IIO_MAP("ain2_rtd", "piControl0", "ain2_rtd"),
	IIO_MAP("ain3_rtd", "piControl0", "ain3_rtd"),
	IIO_MAP("ain4_rtd", "piControl0", "ain4_rtd"),
	IIO_MAP("ain5_rtd", "piControl0", "ain5_rtd"),
	IIO_MAP("ain6_rtd", "piControl0", "ain6_rtd"),
	IIO_MAP("ain7_rtd", "piControl0", "ain7_rtd"),
	{ }
};

static const struct iio_map revpi_compact_aout[] = {
	IIO_MAP("A", "piControl0", "aout0"),
	IIO_MAP("B", "piControl0", "aout1"),
	{ }
};

static int revpi_compact_poll_io(void *data)
{
	struct revpi_compact *machine = (struct revpi_compact *)data;
	struct revpi_compact_image *image = &machine->image;
	struct revpi_compact_image prev = { };
	struct cycletimer ct;
	int ret, i, val[8];
	bool err;

	/* force write of aout channels on first cycle */
	for (i = 0; i < ARRAY_SIZE(prev.usr.aout); i++)
		prev.usr.aout[i] = -1;

	cycletimer_init_on_stack(&ct, REVPI_COMPACT_IO_CYCLE);

	while (!kthread_should_stop()) {
		/* poll din */
		ret = gpiod_get_array_value_cansleep(machine->din->ndescs,
						     machine->din->desc, val);
		image->drv.din_status = max3191x_get_status(machine->din_dev);
		image->drv.din = 0;
		if (ret)
			image->drv.din_status |= BIT(7);
		else
			for (i = 0; i < ARRAY_SIZE(val); i++)
				image->drv.din |= val[i] << i;

		/* poll dout fault pin */
		image->drv.dout_status =
			!!gpiod_get_value_cansleep(machine->dout_fault) << 5;

		flip_process_image(image, machine->config.offset);

		/* write dout on every cycle to feed watchdog */
		/* FIXME: GPIO core should return non-void for set() */
		for (i = 0; i < ARRAY_SIZE(val); i++)
			val[i] = image->usr.dout & BIT(i);
		gpiod_set_array_value_cansleep(machine->dout->ndescs,
					       machine->dout->desc, val);

		/* write aout channels only if changed by user */
		err = false;
		for (i = 0; i < ARRAY_SIZE(image->usr.aout); i++)
			if (image->usr.aout[i] != prev.usr.aout[i]) {
				/*  raw = (value in mV << 8 bit) / 10V */
				int raw = (image->usr.aout[i] << 8) / 10000;
				ret = iio_write_channel_raw(machine->aout[i],
							    min(raw, 255));
				if (ret)
					err = true;
				else
					/*
					 * On success save in prev,
					 * on failure retry during next cycle.
					 */
					prev.usr.aout[i] = image->usr.aout[i];
			}
		assign_bit_in_byte(AOUT_TX_ERR, &image->drv.aout_status, err);

		/* update LEDs if changed by user */
		revpi_led_trigger_event(&prev.usr.led, image->usr.led);

		cycletimer_sleep(&ct);
	}

	cycletimer_destroy(&ct);
	return 0;
}

static int revpi_compact_poll_ain(void *data)
{
	struct revpi_compact *machine = (struct revpi_compact *)data;
	struct revpi_compact_image *image = &machine->image;
	bool  rtd[ARRAY_SIZE(machine->config.ain)];
	bool pt1k[ARRAY_SIZE(machine->config.ain)];
	int   mux[ARRAY_SIZE(machine->config.ain)];
	int  chan[ARRAY_SIZE(machine->config.ain)];
	int ret, i, numchans, raw;
	struct cycletimer ct;

	/* determine which channels are enabled, cache them in chan[] */
	for (i = 0, numchans = 0; i < ARRAY_SIZE(chan); i++) {
		u8 *config = &machine->config.ain[i];
		if (test_bit_in_byte(AIN_ENABLED, config)) {
			rtd[numchans]  = test_bit_in_byte(AIN_RTD, config);
			pt1k[numchans] = test_bit_in_byte(AIN_PT1K, config);
			mux[numchans]  = i + ARRAY_SIZE(chan) * rtd[numchans];
			chan[numchans] = i;
			numchans++;
		}
	}

	if (!numchans) {
		/* no channels enabled, no need to poll */
		machine->ain_thread = NULL;
		return 0;
	}

	i = 0;
	cycletimer_init_on_stack(&ct, REVPI_COMPACT_AIN_CYCLE);

	while (!kthread_should_stop()) {
		unsigned long long tmp;

		/* poll ain */
		ret = iio_read_channel_raw(&machine->ain[mux[i]], &raw);
		assign_bit_in_byte(AIN_TX_ERR, &image->drv.ain_status, ret < 0);
		if (ret < 0) {
			image->drv.ain[chan[i]] = 0;
			goto next_chan;
		}

		/* raw value in mV = ((raw * 12.5V) >> 21 bit) + 6.25V */
		tmp = shift_right((s64)raw * 12500 * 100000000LL, 21);
		raw = (int)div_s64(tmp, 100000000LL) + 6250;

		if (rtd[i]) {
			/*
			 * resistance in Ohm = raw value in mV / 2.5 mA,
			 * scaled by 10 for PT1000 or by 100 for PT100
			 * to match up with values in pt100_table.inc
			 */
			int resistance = pt1k[i] ? raw * 100 / 25
						 : raw * 1000 / 25;
			GetPt100Temperature(resistance, &raw);
		}

		image->drv.ain[chan[i]] = raw;

next_chan:
		if (++i == numchans)
			i = 0;
		cycletimer_sleep(&ct);
	}

	cycletimer_destroy(&ct);
	return 0;
}

static int match_name(struct device *dev, void *data)
{
	const char *name = data;

	if (dev->bus == &spi_bus_type)
		return sysfs_streq(name, to_spi_device(dev)->modalias);
	else if (dev->bus == &iio_bus_type)
		return sysfs_streq(name, dev_to_iio_dev(dev)->name);
	else
		return sysfs_streq(name, dev_name(dev));
}

int revpi_compact_init(struct revpi_compact_config *config)
{
	struct revpi_compact *machine;
	struct sched_param param;
	struct device *dev;
	int ret;

	machine = devm_kzalloc(piDev_g.dev, sizeof(*machine), GFP_KERNEL);
	if (!machine)
		return -ENOMEM;

	piDev_g.machine = machine;
	machine->config = *config;
	gpiod_add_lookup_table(&revpi_compact_gpios);

	machine->din =  gpiod_get_array(piDev_g.dev, "din", GPIOD_ASIS);
	if (IS_ERR(machine->din)) {
		dev_err(piDev_g.dev, "cannot acquire digital input pins\n");
		ret = PTR_ERR(machine->din);
		goto err_remove_table;
	}

	machine->dout = gpiod_get_array(piDev_g.dev, "dout", GPIOD_ASIS);
	if (IS_ERR(machine->dout)) {
		dev_err(piDev_g.dev, "cannot acquire digital output pins\n");
		ret = PTR_ERR(machine->dout);
		goto err_put_din;
	}

	ret = gpiod_set_debounce(machine->din->desc[0], config->din_debounce);
	if (ret)
		dev_err(piDev_g.dev, "cannot set din debounce\n");

	machine->din_dev = bus_find_device(&spi_bus_type, NULL, "max31913",
					   match_name);
	if (!machine->din_dev) {
		dev_err(piDev_g.dev, "cannot find digital input device\n");
		ret = -ENODEV;
		goto err_put_dout;
	}

	dev = bus_find_device(&spi_bus_type, NULL, "74hc595", match_name);
	machine->dout_fault = gpiod_get(dev, "kunbus,fault", GPIOD_IN);
	put_device(dev);
	if (IS_ERR(machine->dout_fault)) {
		dev_err(piDev_g.dev, "cannot acquire digital output fault pin\n");
		ret = PTR_ERR(machine->dout_fault);
		goto err_put_din_dev;
	}

	dev = bus_find_device(&iio_bus_type, NULL, "ain_muxed", match_name);
	if (!dev) {
		dev_err(piDev_g.dev, "cannot find analog input device\n");
		ret = -ENODEV;
		goto err_put_dout_fault;
	}
	machine->ain_dev = dev_to_iio_dev(dev);
	ret = iio_map_array_register(machine->ain_dev,
				     (struct iio_map *)revpi_compact_ain);
	put_device(dev);
	if (ret)
		goto err_put_dout_fault;

	machine->ain = iio_channel_get_all(piDev_g.dev);
	if (IS_ERR(machine->ain)) {
		dev_err(piDev_g.dev, "cannot acquire analog input chans\n");
		ret = PTR_ERR(machine->ain);
		goto err_unregister_ain;
	}

	dev = bus_find_device(&iio_bus_type, NULL, "dac082s085", match_name);
	if (!dev) {
		dev_err(piDev_g.dev, "cannot find analog output device\n");
		ret = -ENODEV;
		goto err_release_ain;
	}
	machine->aout_dev = dev_to_iio_dev(dev);
	ret = iio_map_array_register(machine->aout_dev,
				     (struct iio_map *)revpi_compact_aout);
	put_device(dev);
	if (ret)
		goto err_release_ain;

	machine->aout[0] = iio_channel_get(piDev_g.dev, "aout0");
	machine->aout[1] = iio_channel_get(piDev_g.dev, "aout1");
	if (IS_ERR(machine->aout[0]) || IS_ERR(machine->aout[1])) {
		dev_err(piDev_g.dev, "cannot acquire analog output chans\n");
		ret = PTR_ERR(machine->aout[0]) | PTR_ERR(machine->aout[1]);
		goto err_unregister_aout;
	}

	machine->io_thread = kthread_create(&revpi_compact_poll_io, machine,
					    "piControl i/o");
	if (IS_ERR(machine->io_thread)) {
		dev_err(piDev_g.dev, "cannot create i/o thread\n");
		ret = PTR_ERR(machine->io_thread);
		goto err_release_aout;
	}

	param.sched_priority = RT_PRIO_BRIDGE;
	ret = sched_setscheduler(machine->io_thread, SCHED_FIFO, &param);
	if (ret) {
		dev_err(piDev_g.dev, "cannot upgrade i/o thread priority\n");
		goto err_stop_io_thread;
	}

	machine->ain_thread = kthread_create(&revpi_compact_poll_ain, machine,
					     "piControl ain");
	if (IS_ERR(machine->ain_thread)) {
		dev_err(piDev_g.dev, "cannot create ain thread\n");
		ret = PTR_ERR(machine->ain_thread);
		goto err_stop_io_thread;
	}

	ret = sched_setscheduler(machine->ain_thread, SCHED_FIFO, &param);
	if (ret) {
		dev_err(piDev_g.dev, "cannot upgrade ain thread priority\n");
		goto err_stop_ain_thread;
	}

	/*
	 * ain_thread may return immediately if no channels are enabled,
	 * so upgrade thread priority *before* waking it up.
	 */
	wake_up_process(machine->io_thread);
	wake_up_process(machine->ain_thread);

	return 0;

err_stop_ain_thread:
	kthread_stop(machine->ain_thread);
err_stop_io_thread:
	kthread_stop(machine->io_thread);
err_release_aout:
	iio_channel_release(machine->aout[0]);
	iio_channel_release(machine->aout[1]);
err_unregister_aout:
	iio_map_array_unregister(machine->aout_dev);
err_release_ain:
	iio_channel_release_all(machine->ain);
err_unregister_ain:
	iio_map_array_unregister(machine->ain_dev);
err_put_dout_fault:
	gpiod_put(machine->dout_fault);
err_put_din_dev:
	put_device(machine->din_dev);
err_put_dout:
	gpiod_put_array(machine->dout);
err_put_din:
	gpiod_put_array(machine->din);
err_remove_table:
	gpiod_remove_lookup_table(&revpi_compact_gpios);
	piDev_g.machine = NULL;
	return ret;
}

void revpi_compact_fini(void)
{
	struct revpi_compact *machine = (struct revpi_compact *)piDev_g.machine;

	if (!machine)
		return;

	if (!IS_ERR_OR_NULL(machine->ain_thread))
		kthread_stop(machine->ain_thread);
	if (!IS_ERR_OR_NULL(machine->io_thread))
		kthread_stop(machine->io_thread);

	iio_channel_release(machine->aout[0]);
	iio_channel_release(machine->aout[1]);
	iio_map_array_unregister(machine->aout_dev);
	iio_channel_release_all(machine->ain);
	iio_map_array_unregister(machine->ain_dev);
	gpiod_put(machine->dout_fault);
	put_device(machine->din_dev);
	gpiod_put_array(machine->dout);
	gpiod_put_array(machine->din);
	gpiod_remove_lookup_table(&revpi_compact_gpios);
	piDev_g.machine = NULL;
}

int revpi_compact_reset(struct revpi_compact_config *config)
{
	struct revpi_compact *machine = piDev_g.machine;
	struct sched_param param;
	int ret;

	if (!IS_ERR_OR_NULL(machine->ain_thread))
		kthread_stop(machine->ain_thread);

	machine->config = *config;

	ret = gpiod_set_debounce(machine->din->desc[0], config->din_debounce);
	if (ret)
		dev_err(piDev_g.dev, "cannot set din debounce\n");

	machine->ain_thread = kthread_create(&revpi_compact_poll_ain, machine,
					     "piControl ain");
	if (IS_ERR(machine->ain_thread)) {
		dev_err(piDev_g.dev, "cannot create ain thread\n");
		return PTR_ERR(machine->ain_thread);
	}

	param.sched_priority = RT_PRIO_BRIDGE;
	ret = sched_setscheduler(machine->ain_thread, SCHED_FIFO, &param);
	if (ret) {
		dev_err(piDev_g.dev, "cannot upgrade ain thread priority\n");
		return ret;
	}

	wake_up_process(machine->ain_thread);

	return 0;
}

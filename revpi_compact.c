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
#include <linux/ktime.h>

#include "project.h"
#include "common_define.h"
#include "ModGateComMain.h"
#include "PiBridgeMaster.h"
#include "piControlMain.h"
#include "process_image.h"
#include "pt100.h"
#include "revpi_common.h"
#include "revpi_compact.h"

//#define REVPI_COMPACT_IO_CYCLE	(1000 * NSEC_PER_USEC)		//   1 msec
#define REVPI_COMPACT_IO_CYCLE		( 250 * NSEC_PER_USEC)		// 250 usec
#define REVPI_COMPACT_AIN_CYCLE		( 125 * NSEC_PER_MSEC)		// 125 msec

typedef struct _SRevPiCompact {
	SRevPiCompactImage image;
	SRevPiCompactConfig config;
	struct task_struct *io_thread;
	struct task_struct *ain_thread;
	struct device *din_dev;
	struct gpio_desc *dout_fault;
	struct gpio_descs *din;
	struct gpio_descs *dout;
	struct iio_dev *ain_dev, *aout_dev;
	struct iio_channel *ain;
	struct iio_channel *aout[2];
	bool ain_should_reset;
	struct completion ain_reset;
} SRevPiCompact;

static SRevPiCompactConfig revpi_compact_config_g;

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

//#define BENCH

static int revpi_compact_poll_io(void *data)
{
	SRevPiCompact *machine = (SRevPiCompact *)data;
	SRevPiCompactImage *image = &machine->image;
	SRevPiCompactImage prev = { };
	struct cycletimer ct;
	int ret, i, val[8];
	bool err;
#ifdef BENCH
	ktime_t t[7];
	s64 d[7];
	s64 m[7];
	int loop = 0;
	memset(m, 0, sizeof(m));
#define MEASSURE(i)   t[i] = ktime_get()
#else
#define MEASSURE(i)
#endif
	/* force write of aout channels on first cycle */
	for (i = 0; i < ARRAY_SIZE(prev.usr.aout); i++)
		prev.usr.aout[i] = -1;

	cycletimer_init_on_stack(&ct, REVPI_COMPACT_IO_CYCLE);

	while (!kthread_should_stop()) {
		MEASSURE(0);
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

		MEASSURE(1);
		/* poll dout fault pin */
		image->drv.dout_status =
			!!gpiod_get_value_cansleep(machine->dout_fault) << 5;

		MEASSURE(2);
		flip_process_image(image, machine->config.offset);

		MEASSURE(3);
		/* write dout on every cycle to feed watchdog */
		/* FIXME: GPIO core should return non-void for set() */
		for (i = 0; i < ARRAY_SIZE(val); i++)
			val[i] = image->usr.dout & BIT(i);
		gpiod_set_array_value_cansleep(machine->dout->ndescs,
					       machine->dout->desc, val);

		MEASSURE(4);
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

		MEASSURE(5);
		/* update LEDs if changed by user */
		revpi_led_trigger_event(&prev.usr.led, image->usr.led);
		MEASSURE(6);
#ifdef BENCH
		for (i=0; i<6; i++) {
			d[i] = ktime_to_us(ktime_sub(t[i+1], t[i]));
			if (m[i] < d[i])
				m[i] = d[i];
		}
		d[6] = ktime_to_us(ktime_sub(t[6], t[0]));
		if (m[6] < d[6])
			m[6] = d[6];

		if ((loop % 4000) == 0) {
			pr_info("ioThread: d %3d %3d %3d %3d %3d %3d s %3d\n",
				(int)d[0], (int)d[1], (int)d[2], (int)d[3], (int)d[4], (int)d[5], (int)d[6]);
			pr_info("ioThread: m %3d %3d %3d %3d %3d %3d s %3d\n",
				(int)m[0], (int)m[1], (int)m[2], (int)m[3], (int)m[4], (int)m[5], (int)m[6]);
		}
		loop++;
#endif
		cycletimer_sleep(&ct);
	}

	cycletimer_destroy(&ct);
	return 0;
}

static int revpi_compact_poll_ain(void *data)
{
	SRevPiCompact *machine = (SRevPiCompact *)data;
	SRevPiCompactImage *image = &machine->image;
	bool  rtd[ARRAY_SIZE(machine->config.ain)];
	bool pt1k[ARRAY_SIZE(machine->config.ain)];
	int   mux[ARRAY_SIZE(machine->config.ain)];
	int  chan[ARRAY_SIZE(machine->config.ain)];
	int i = 0, numchans = 0, ret, raw;
	struct cycletimer ct;

	cycletimer_init_on_stack(&ct, REVPI_COMPACT_AIN_CYCLE);

	while (!kthread_should_stop()) {
		unsigned long long tmp;

		if (machine->ain_should_reset) {
			/* determine which channels are enabled */
			pr_info_aio("AIn Reset: config %d %d %d %d %d %d %d %d\n",
				machine->config.ain[0], machine->config.ain[1], machine->config.ain[2], machine->config.ain[3],
				machine->config.ain[4], machine->config.ain[5], machine->config.ain[6], machine->config.ain[7]);

			for (i = 0, numchans = 0; i < ARRAY_SIZE(chan); i++) {
				unsigned long config = machine->config.ain[i];

				if (!test_bit(AIN_ENABLED, &config))
					continue;

				/* pre-calculate channel parameters */
				rtd[numchans]  = test_bit(AIN_RTD, &config);
				pt1k[numchans] = test_bit(AIN_PT1K, &config);
				mux[numchans]  = i + rtd[numchans] *
						 ARRAY_SIZE(chan);
				chan[numchans] = i;
				numchans++;
			}

			dev_info(piDev_g.dev, "ain thread reset to %d chans\n",
				 numchans);

			if (!numchans) {
				complete(&machine->ain_reset);
				set_current_state(TASK_IDLE);
				schedule();
				continue;
			}

			i = 0;
			cycletimer_change(&ct, NSEC_PER_SEC / numchans);
			WRITE_ONCE(machine->ain_should_reset, false);
			complete(&machine->ain_reset);
			pr_info_aio("AIn Reset: ct %dms, %d active: %d %d %d %d %d %d %d %d    %d %d %d %d %d %d %d %d\n",
				(1000 / numchans), numchans,
				mux[0], mux[1], mux[2], mux[3], mux[4], mux[5], mux[6], mux[7],
				chan[0], chan[1], chan[2], chan[3], chan[4], chan[5], chan[6], chan[7]
				);
		}

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

INT32U revpi_compact_config(uint8_t i8uAddress, uint16_t i16uNumEntries, SEntryInfo * pEnt)
{
	uint16_t i;

	for (i = 0; i < i16uNumEntries; i++) {
		pr_info_aio("addr %2d  type %d  len %3d  offset %3d  value %d 0x%x\n",
			    pEnt[i].i8uAddress, pEnt[i].i8uType, pEnt[i].i16uBitLength, pEnt[i].i16uOffset,
			    pEnt[i].i32uDefault, pEnt[i].i32uDefault);

		switch (pEnt[i].i16uOffset) {
		case RevPi_Compact_OFFSET_DInDebounce:
			revpi_compact_config_g.din_debounce = pEnt[i].i32uDefault;
			break;
		case RevPi_Compact_OFFSET_AInMode1:
			revpi_compact_config_g.ain[0] = pEnt[i].i32uDefault;
			break;
		case RevPi_Compact_OFFSET_AInMode2:
			revpi_compact_config_g.ain[1] = pEnt[i].i32uDefault;
			break;
		case RevPi_Compact_OFFSET_AInMode3:
			revpi_compact_config_g.ain[2] = pEnt[i].i32uDefault;
			break;
		case RevPi_Compact_OFFSET_AInMode4:
			revpi_compact_config_g.ain[3] = pEnt[i].i32uDefault;
			break;
		case RevPi_Compact_OFFSET_AInMode5:
			revpi_compact_config_g.ain[4] = pEnt[i].i32uDefault;
			break;
		case RevPi_Compact_OFFSET_AInMode6:
			revpi_compact_config_g.ain[5] = pEnt[i].i32uDefault;
			break;
		case RevPi_Compact_OFFSET_AInMode7:
			revpi_compact_config_g.ain[6] = pEnt[i].i32uDefault;
			break;
		case RevPi_Compact_OFFSET_AInMode8:
			revpi_compact_config_g.ain[7] = pEnt[i].i32uDefault;
			break;
		default:
			// nothing to do
			break;
		}
	}
	return 0;
}

int revpi_compact_init(void)
{
	SRevPiCompact *machine;
	struct sched_param param = { .sched_priority = RT_PRIO_BRIDGE };
	struct device *dev;
	int ret;

	machine = devm_kzalloc(piDev_g.dev, sizeof(*machine), GFP_KERNEL);
	if (!machine)
		return -ENOMEM;

	piDev_g.machine = machine;
	machine->config = revpi_compact_config_g;
	machine->ain_should_reset = true;
	init_completion(&machine->ain_reset);
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

	ret = gpiod_set_debounce(machine->din->desc[0], machine->config.din_debounce);
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
	SRevPiCompact *machine = (SRevPiCompact *)piDev_g.machine;

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

int revpi_compact_reset()
{
	SRevPiCompact *machine = piDev_g.machine;
	int ret;

	machine->config = revpi_compact_config_g;

	ret = gpiod_set_debounce(machine->din->desc[0], machine->config.din_debounce);
	if (ret)
		dev_err(piDev_g.dev, "cannot set din debounce\n");

	reinit_completion(&machine->ain_reset);
	WRITE_ONCE(machine->ain_should_reset, true);
	wake_up_process(machine->ain_thread);
	wait_for_completion(&machine->ain_reset);

	return 0;
}

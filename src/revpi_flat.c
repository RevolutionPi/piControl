// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: 2020-2024 KUNBUS GmbH

#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/iio/consumer.h>
#include <linux/iio/iio.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/thermal.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/platform_device.h>

#include "piControlMain.h"
#include "process_image.h"
#include "revpi_common.h"
#include "revpi_flat.h"
#include "RevPiDevice.h"

#if KERNEL_VERSION(6, 6, 0) > LINUX_VERSION_CODE
/* kernel before 6.6 has a custom RPi patch, so offset started at 0 */
#define GPIOCHIP0_OFFSET	0
#else
#define GPIOCHIP0_OFFSET	512
#endif

/* relais gpio num */
#define REVPI_FLAT_RELAIS_GPIO			(28 + GPIOCHIP0_OFFSET)

/* button gpio num */
#define REVPI_FLAT_BUTTON_GPIO			(13 + GPIOCHIP0_OFFSET)
#define REVPI_FLAT_S_BUTTON_GPIO		(23 + GPIOCHIP0_OFFSET)

#define REVPI_FLAT_DOUT_THREAD_PRIO		(MAX_RT_PRIO / 2 + 8)
#define REVPI_FLAT_AIN_THREAD_PRIO		(MAX_RT_PRIO / 2 + 6)
/* ain resistor (Ohm) */
#define REVPI_FLAT_AIN_RESISTOR			240
/* This value is a correction factor which takes the currency loss caused
   by resistors into account. See the flat schematics for details. */
#define REVPI_FLAT_AIN_CORRECTION		1986582478
#define REVPI_FLAT_AIN_POLL_INTERVAL		85

#define REVPI_FLAT_CONFIG_OFFSET(member) offsetof(struct revpi_flat_image, usr.member)

#define REVPI_FLAT_CONFIG_OFFSET_LEDS		REVPI_FLAT_CONFIG_OFFSET(leds)
#define REVPI_FLAT_CONFIG_OFFSET_AOUT		REVPI_FLAT_CONFIG_OFFSET(aout)
#define REVPI_FLAT_CONFIG_OFFSET_DOUT		REVPI_FLAT_CONFIG_OFFSET(dout)
#define REVPI_FLAT_CONFIG_OFFSET_AIN_MODE	REVPI_FLAT_CONFIG_OFFSET(ain_mode_current)
/*
 * Delay in usecs before the next AIN value can safely be retrieved (see tCSHSD
 * in MCP datasheet)
 */
#define REVPI_FLAT_AIN_DELAY			10

struct revpi_flat_image {
	struct {
		s16 ain;
#define REVPI_FLAT_AIN_TX_ERR  			7
		u8 ain_status;
#define REVPI_FLAT_AOUT_TX_ERR			7
		u8 aout_status;
		u8 cpu_temp;
		u8 cpu_freq;
		u8 button;
	} __attribute__ ((__packed__)) drv;
	struct {
		u16 leds;
		u16 aout;
		u8 dout;
		u8 ain_mode_current;
	} __attribute__ ((__packed__)) usr;
} __attribute__ ((__packed__));

struct revpi_flat {
	struct revpi_flat_image image;
	struct task_struct *dout_thread;
	struct task_struct *ain_thread;
	struct device *din_dev;
	struct gpio_desc *dout_fault;
	struct gpio_desc *digout;
	struct gpio_desc *button_desc;
	struct gpio_descs *dout;
	struct iio_channel ain;
	struct iio_channel aout;
};

static int revpi_flat_poll_dout(void *data)
{
	struct revpi_flat *flat = (struct revpi_flat *) data;
	struct revpi_flat_image *image = &flat->image;
	struct revpi_flat_image *usr_image;
	int dout_val = -1;
	int aout_val = -1;
	int raw_out;

	usr_image = (struct revpi_flat_image *) piDev_g.ai8uPI;
	while (!kthread_should_stop()) {
		my_rt_mutex_lock(&piDev_g.lockPI);
		image->drv.button = gpiod_get_value_cansleep(flat->button_desc);
		usr_image->drv = image->drv;

		if (usr_image->usr.dout != image->usr.dout)
			dout_val = usr_image->usr.dout;

		if (usr_image->usr.aout != image->usr.aout)
			aout_val = usr_image->usr.aout;

		image->usr = usr_image->usr;
		rt_mutex_unlock(&piDev_g.lockPI);

		if (dout_val != -1) {
			gpiod_set_value_cansleep(flat->digout, !!dout_val);
			dout_val = -1;
		}

		if (aout_val != -1) {
			int ret;

			raw_out = (image->usr.aout * 4095) / 10000;

			ret = iio_write_channel_raw(&flat->aout,
						    min(raw_out, 4095));
			if (ret)
				dev_err(piDev_g.dev, "failed to write value to "
					"analog ouput: %i\n", ret);

			assign_bit_in_byte(REVPI_FLAT_AOUT_TX_ERR,
					   &image->drv.aout_status, ret < 0);
			aout_val = -1;
		}

		usleep_range(100, 150);
	}

	return 0;
}

static int revpi_flat_handle_ain(struct revpi_flat *flat, bool mode_current)
{
	struct revpi_flat_image *image = &flat->image;
	unsigned long long ain_val;
	int raw_val;
	int ret;


	ret = iio_read_channel_raw(&flat->ain, &raw_val);
	assign_bit_in_byte(REVPI_FLAT_AIN_TX_ERR, &image->drv.ain_status,
			   ret < 0);
	if (ret < 0) {
		dev_err(piDev_g.dev, "failed to read from analog "
			"channel: %i\n", ret);
		return ret;
	}
	/* AIN value in mV = ((raw * 12.5V) >> 21 bit) + 6.25V */
	ain_val = shift_right((s64) raw_val * 12500, 21) + 6250;

	if (mode_current) {
		ain_val *= 1000000000;
		ain_val = div_s64(ain_val, REVPI_FLAT_AIN_RESISTOR);
	} else
		ain_val *= REVPI_FLAT_AIN_CORRECTION;

	ain_val = (int) div_s64(ain_val, 1000000000LL);

	my_rt_mutex_lock(&piDev_g.lockPI);
	image->drv.ain = ain_val;
	rt_mutex_unlock(&piDev_g.lockPI);

	return 0;
}

static int revpi_flat_poll_ain(void *data)
{
	struct revpi_flat *flat = (struct revpi_flat *) data;
	struct revpi_flat_image *image = &flat->image;
	bool ain_mode_current = false;
	u16 prev_leds = 0;
	int temperature;
	int freq;
	u16 leds;
	int ret;

	while (!kthread_should_stop()) {
		ret = revpi_flat_handle_ain(flat, ain_mode_current);
		if (ret)
			msleep(REVPI_FLAT_AIN_POLL_INTERVAL);
		/*
		 * Wait a minimum timespan before requesting the next AIN
		 * value.
		 */
		usleep_range(REVPI_FLAT_AIN_DELAY, REVPI_FLAT_AIN_DELAY + 10);
		/* read cpu temperature */
		if (piDev_g.thermal_zone != NULL) {
			ret = thermal_zone_get_temp(piDev_g.thermal_zone,
						    &temperature);
			if (ret)
				dev_err(piDev_g.dev,"Failed to get cpu "
					"temperature");
		}

		/*
		   Get the CPU clock from CPU0 in kHz
		   and divide it down to MHz.
		*/
		freq = cpufreq_quick_get(0);

		my_rt_mutex_lock(&piDev_g.lockPI);
		if ((piDev_g.thermal_zone != NULL) && !ret)
			image->drv.cpu_temp = temperature / 1000;
		image->drv.cpu_freq = freq / 10;
		leds = image->usr.leds;
		ain_mode_current = !!image->usr.ain_mode_current;
		rt_mutex_unlock(&piDev_g.lockPI);

		if (prev_leds != leds)
			revpi_led_trigger_event(prev_leds, leds);

		prev_leds = leds;
	}

	return 0;
}

static int revpi_flat_match_iio_name(struct device *dev, const void *data)
{
	return !strcmp(data, dev_to_iio_dev(dev)->name);
}

static void revpi_flat_adjust_config(void)
{
	SDeviceInfo *dev_info = piDev_g.devs->dev;
	SDevice *dev;
	int i;

	/* Check if there are any valid parsing results at all. This might
	   not be the case if an invalid config file was provided. */
	if (piDev_g.devs == NULL)
		return;

	/* Add all virtual devices to list of known devices. The first device is
	   the flat, so skip it. */
	for (i = 1; i < piDev_g.devs->i16uNumDevices; i++) {
		dev_info = &piDev_g.devs->dev[i];
		dev = RevPiDevice_getDev(i);

		if (dev_info->i16uModuleType >= PICONTROL_SW_OFFSET) {
			/* virtual device are always active */
			dev->i8uActive = 1;
			dev->sId.i16uModulType = dev_info->i16uModuleType;
		} else {
			pr_err("Additional module type %d is not allowed on "
			       "RevPi Flat. Only sw modules are allowed.\n",
			       dev_info->i16uModuleType);

			RevPiDevice_setStatus(0, PICONTROL_STATUS_MISSING_MODULE);
			dev->i8uActive = 0;
			dev->sId.i16uModulType = dev_info->i16uModuleType |
						 PICONTROL_NOT_CONNECTED;
		}
		dev->i8uAddress = dev_info->i8uAddress;
		dev->i8uScan = 0;
		dev->i16uInputOffset = dev_info->i16uInputOffset;
		dev->i16uOutputOffset = dev_info->i16uOutputOffset;
		dev->i16uConfigOffset = dev_info->i16uConfigOffset;
		dev->i16uConfigLength = dev_info->i16uConfigLength;
		dev->sId.i32uSerialnumber = dev_info->i32uSerialnumber;
		dev->sId.i16uHW_Revision = dev_info->i16uHW_Revision;
		dev->sId.i16uSW_Major = dev_info->i16uSW_Major;
		dev->sId.i16uSW_Minor = dev_info->i16uSW_Minor;
		dev->sId.i32uSVN_Revision = dev_info->i32uSVN_Revision;
		dev->sId.i16uFBS_InputLength = dev_info->i16uInputLength;
		dev->sId.i16uFBS_OutputLength = dev_info->i16uOutputLength;
		dev->sId.i16uFeatureDescriptor = 0;

		RevPiDevice_incDevCnt();
	}
}

static void revpi_flat_set_defaults(void)
{
	my_rt_mutex_lock(&piDev_g.lockPI);
	memset(piDev_g.ai8uPI, 0, sizeof(piDev_g.ai8uPI));
	if (piDev_g.ent)
		revpi_set_defaults(piDev_g.ai8uPI, piDev_g.ent);
	rt_mutex_unlock(&piDev_g.lockPI);
}

int revpi_flat_reset(void)
{
	dev_dbg(piDev_g.dev, "Resetting RevPi Flat control\n");

	RevPiDevice_init();

	revpi_flat_adjust_config();
	revpi_flat_set_defaults();

	return 0;
}

int revpi_flat_probe(struct platform_device *pdev)
{
	struct revpi_flat *flat;
	unsigned int button_gpio;
	struct device *dev;
	int ret;

	flat = devm_kzalloc(piDev_g.dev, sizeof(*flat), GFP_KERNEL);
	if (!flat)
		return -ENOMEM;

	piDev_g.machine = flat;

	flat->digout = gpio_to_desc(REVPI_FLAT_RELAIS_GPIO);
	if (!flat->digout) {
		dev_err(piDev_g.dev, "no gpio desc for digital output found\n");
		return -ENXIO;
	}

	ret = gpiod_direction_output(flat->digout, 0);
	if (ret) {
		dev_err(piDev_g.dev, "Failed to set direction for relais "
			"gpio %i\n", ret);
		return -ENXIO;
	}

	button_gpio = of_machine_is_compatible("kunbus,revpi-flat-s-2022") ?
		REVPI_FLAT_S_BUTTON_GPIO : REVPI_FLAT_BUTTON_GPIO;

	flat->button_desc = gpio_to_desc(button_gpio);
	if (!flat->button_desc) {
		dev_err(piDev_g.dev, "no gpio desc for button found\n");
		return -ENXIO;
	}

	ret = gpiod_direction_input(flat->button_desc);
	if (ret) {
		dev_err(piDev_g.dev, "Failed to set direction for button "
			"gpio %i\n", ret);
		return -ENXIO;
	}

	dev = bus_find_device(&iio_bus_type, NULL, "mcp3550-50",
			      revpi_flat_match_iio_name);
	if (!dev) {
		dev_err(piDev_g.dev, "cannot find analog input device\n");
		return -ENODEV;
	}

	flat->ain.indio_dev = dev_to_iio_dev(dev);
	flat->ain.channel = &(flat->ain.indio_dev->channels[0]);


	dev = bus_find_device(&iio_bus_type, NULL, "dac7512",
			      revpi_flat_match_iio_name);
	if (!dev) {
		dev_err(piDev_g.dev, "cannot find analog output device\n");
		ret = -ENODEV;
		goto err_put_ain;
	}

	flat->aout.indio_dev = dev_to_iio_dev(dev);
	flat->aout.channel = &(flat->aout.indio_dev->channels[0]);

	flat->dout_thread = kthread_create(&revpi_flat_poll_dout, flat,
					   "piControl dout");
	if (IS_ERR(flat->dout_thread)) {
		dev_err(piDev_g.dev, "cannot create dout thread\n");
		ret = PTR_ERR(flat->dout_thread);
		goto err_put_aout;
	}

	ret = set_rt_priority(flat->dout_thread, REVPI_FLAT_DOUT_THREAD_PRIO);
	if (ret) {
		dev_err(piDev_g.dev, "cannot upgrade dout thread priority\n");
		goto err_stop_dout_thread;
	}

	flat->ain_thread = kthread_create(&revpi_flat_poll_ain, flat,
					  "piControl ain");
	if (IS_ERR(flat->ain_thread)) {
		dev_err(piDev_g.dev, "cannot create ain thread\n");
		ret = PTR_ERR(flat->ain_thread);
		goto err_stop_dout_thread;
	}

	ret = set_rt_priority(flat->ain_thread, REVPI_FLAT_AIN_THREAD_PRIO);
	if (ret) {
		dev_err(piDev_g.dev, "cannot upgrade ain thread priority\n");
		goto err_stop_ain_thread;
	}

	revpi_flat_reset();

	wake_up_process(flat->dout_thread);
	wake_up_process(flat->ain_thread);

	return 0;

err_stop_ain_thread:
	kthread_stop(flat->ain_thread);
err_stop_dout_thread:
	kthread_stop(flat->dout_thread);
err_put_aout:
	iio_device_put(flat->aout.indio_dev);
err_put_ain:
	iio_device_put(flat->ain.indio_dev);

	return ret;
}

void revpi_flat_remove(struct platform_device *pdev)
{
	struct revpi_flat *flat = (struct revpi_flat *) piDev_g.machine;

	kthread_stop(flat->ain_thread);
	kthread_stop(flat->dout_thread);
	iio_device_put(flat->aout.indio_dev);
	iio_device_put(flat->ain.indio_dev);
}

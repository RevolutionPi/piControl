// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: 2017-2024 KUNBUS GmbH

// revpi_compact.c - RevPi Compact specific handling

#include <linux/cpufreq.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio/machine.h>
#include <linux/iio/consumer.h>
#include <linux/iio/driver.h>
#include <linux/iio/iio.h>
#include <linux/iio/machine.h>
#include <linux/ktime.h>
#include <linux/spi/max3191x.h>
#include <linux/spi/spi.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>

#include "piControlMain.h"
#include "process_image.h"
#include "pt100.h"
#include "revpi_common.h"
#include "revpi_compact.h"
#include "RevPiDevice.h"

#define REVPI_COMPACT_IO_CYCLE		( 250 * NSEC_PER_USEC)		// 250 usec
#define REVPI_COMPACT_AIN_CYCLE		( 125 * NSEC_PER_MSEC)		// 125 msec

#define IO_THREAD_PRIO	MAX_RT_PRIO/2 + 8
#define AIN_THREAD_PRIO MAX_RT_PRIO/2 + 6

static const struct kthread_prio revpi_compact_kthread_prios[] = {
	/* spi pump to I/O chips */
	{ .comm = "spi2",		.prio = MAX_RT_PRIO/2 + 10 },
	{ }
};

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
	struct revpi_compact_stats stats;
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
		    {}
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

#define revpi_compact_descriptor_attr(field, format_string)		\
static ssize_t								\
field##_show(struct device *dev, struct device_attribute *attr,		\
               char *buf)						\
{									\
       struct revpi_compact_stats *stats;				\
       unsigned int seq;						\
       u64 lost_cycles;							\
									\
       stats = &((struct _SRevPiCompact*) piDev_g.machine)->stats;	\
									\
       do {								\
               seq = read_seqbegin(&stats->lock);			\
               lost_cycles = stats->lost_cycles;			\
       } while (read_seqretry(&stats->lock, seq));			\
									\
       return sysfs_emit(buf, format_string, lost_cycles);		\
}

revpi_compact_descriptor_attr(lost_cycles, "%llu\n");

static DEVICE_ATTR(lost_cycles, S_IRUGO, lost_cycles_show, NULL);
//#define BENCH

static int revpi_compact_poll_io(void *data)
{
	SRevPiCompact *machine = (SRevPiCompact *)data;
	SRevPiCompactImage *image = &machine->image;
	SRevPiCompactImage prev = { };
	struct cycletimer ct;
	int ret, i;
	DECLARE_BITMAP(val, 8);
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
		                                     machine->din->desc,
		                                     machine->din->info,
		                                     val);
		image->drv.din_status = max3191x_get_status(machine->din_dev);
		image->drv.din = 0;
		if (ret)
			image->drv.din_status |= BIT(7);
		else
			image->drv.din = (u8)val[0] & 0xff;

		MEASSURE(1);
		/* poll dout fault pin */
		image->drv.dout_status =
			!!gpiod_get_value_cansleep(machine->dout_fault) << 5;

		MEASSURE(2);
		flip_process_image(image, machine->config.offset);
		revpi_check_timeout();

		MEASSURE(3);
		/* write dout on every cycle to feed watchdog */
		/* FIXME: GPIO core should return non-void for set() */
		val[0] = image->usr.dout & 0xff;
		gpiod_set_array_value_cansleep(machine->dout->ndescs,
		                               machine->dout->desc,
		                               machine->dout->info,
		                               val);

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
		revpi_led_trigger_event(prev.usr.led, image->usr.led);
		prev.usr.led = image->usr.led;
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
		cycletimer_sleep(&ct, &machine->stats);
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

		smp_rmb();
		if (machine->ain_should_reset) {
			/* determine which channels are enabled */
			pr_info_aio("AIn Reset: config %d %d %d %d %d %d %d %d\n",
				machine->config.ain[0], machine->config.ain[1], machine->config.ain[2], machine->config.ain[3],
				machine->config.ain[4], machine->config.ain[5], machine->config.ain[6], machine->config.ain[7]);

			for (i = 0, numchans = 0; i < ARRAY_SIZE(chan); i++) {
				unsigned long config = machine->config.ain[i];

				if (!test_bit(AIN_ENABLED, &config)) {
					my_rt_mutex_lock(&piDev_g.lockPI);
					image->drv.ain[i] = 0;
					rt_mutex_unlock(&piDev_g.lockPI);
					continue;
				}

				/* pre-calculate channel parameters */
				rtd[numchans]  = test_bit(AIN_RTD, &config);
				pt1k[numchans] = test_bit(AIN_PT1K, &config);
				mux[numchans]  = i + rtd[numchans] *
						 ARRAY_SIZE(chan);
				chan[numchans] = i;
				numchans++;
			}

			pr_info("ain thread reset to %d chans\n",
				 numchans);

			/*
			 * If numchans is 0, still need to wake up once per sec
			 * to update core frequency and temperature.
			 */
			cycletimer_change(&ct, NSEC_PER_SEC / max(numchans, 1));

			i = 0;
			smp_store_release(&machine->ain_should_reset, false);
			complete(&machine->ain_reset);
			pr_info_aio("AIn Reset: ct %dms, %d active: %d %d %d %d %d %d %d %d    %d %d %d %d %d %d %d %d\n",
				(1000 / numchans), numchans,
				mux[0], mux[1], mux[2], mux[3], mux[4], mux[5], mux[6], mux[7],
				chan[0], chan[1], chan[2], chan[3], chan[4], chan[5], chan[6], chan[7]
				);
		}

		if (!numchans)
			goto next_chan; /* only update core freq and temp */

		/* poll ain */
		ret = iio_read_channel_raw(&machine->ain[mux[i]], &raw);

		my_rt_mutex_lock(&piDev_g.lockPI);
		assign_bit_in_byte(AIN_TX_ERR, &image->drv.ain_status, ret < 0);
		if (ret < 0) {
			image->drv.ain[chan[i]] = 0;
			rt_mutex_unlock(&piDev_g.lockPI);
			goto next_chan;
		}
		rt_mutex_unlock(&piDev_g.lockPI);

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

		my_rt_mutex_lock(&piDev_g.lockPI);
		image->drv.ain[chan[i]] = raw;
		rt_mutex_unlock(&piDev_g.lockPI);

next_chan:
		if (++i >= numchans) {
			int ret;
			int freq;
			int temp;
			i = 0;

			// update every 1 sec
			if (piDev_g.thermal_zone != NULL) {
				ret = thermal_zone_get_temp(piDev_g.thermal_zone, &temp);
				if (ret)
					pr_err("could not read cpu temperature");
			}

			/*
			   Get the CPU clock from CPU0 in kHz
			   and divide it down to MHz.
			*/
			freq = cpufreq_quick_get(0);

			my_rt_mutex_lock(&piDev_g.lockPI);
			if (piDev_g.thermal_zone != NULL && !ret)
				image->drv.i8uCPUTemperature = temp / 1000;
			image->drv.i8uCPUFrequency = freq / 10;
			rt_mutex_unlock(&piDev_g.lockPI);
		}

		cycletimer_sleep(&ct, &machine->stats);
	}

	cycletimer_destroy(&ct);
	return 0;
}

static int match_name(struct device *dev, const void *data)
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

void revpi_compact_adjust_config(void)
{
	int i, j;
	int result = 0, found;
	uint8_t *state;

	RevPiDevice_init();

	if (piDev_g.devs == NULL) {
		// config file could not be read, do nothing
		return;
	}

	state = kcalloc(piDev_g.devs->i16uNumDevices, sizeof(uint8_t), GFP_KERNEL);

	// Schleife über alle Module die automatisch erkannt wurden
	for (j = 0; j < RevPiDevice_getDevCnt(); j++) {
		// Suche diese Module in der Konfigurationsdatei
		for (i = 0, found = 0; found == 0 && i < piDev_g.devs->i16uNumDevices; i++) {
			// Grundvoraussetzung ist, dass die Adresse gleich ist.
			if (RevPiDevice_getDev(j)->i8uAddress == piDev_g.devs->dev[i].i8uAddress) {
				// Außerdem muss ModuleType, InputLength und OutputLength gleich sein.
				if (RevPiDevice_getDev(j)->sId.i16uModulType != piDev_g.devs->dev[i].i16uModuleType) {
					pr_warn("## address %d: incorrect module type %d != %d\n",
						RevPiDevice_getDev(j)->i8uAddress, RevPiDevice_getDev(j)->sId.i16uModulType,
						piDev_g.devs->dev[i].i16uModuleType);
					result = PICONTROL_CONFIG_ERROR_WRONG_MODULE_TYPE;
					RevPiDevice_setStatus(0, PICONTROL_STATUS_SIZE_MISMATCH);
					break;
				}
				if (RevPiDevice_getDev(j)->sId.i16uFBS_InputLength != piDev_g.devs->dev[i].i16uInputLength) {
					pr_warn("## address %d: incorrect input length %d != %d\n",
						RevPiDevice_getDev(j)->i8uAddress, RevPiDevice_getDev(j)->sId.i16uFBS_InputLength,
						piDev_g.devs->dev[i].i16uInputLength);
					result = PICONTROL_CONFIG_ERROR_WRONG_INPUT_LENGTH;
					RevPiDevice_setStatus(0, PICONTROL_STATUS_SIZE_MISMATCH);
					break;
				}
				if (RevPiDevice_getDev(j)->sId.i16uFBS_OutputLength != piDev_g.devs->dev[i].i16uOutputLength) {
					pr_warn("## address %d: incorrect output length %d != %d\n",
						RevPiDevice_getDev(j)->i8uAddress,
						RevPiDevice_getDev(j)->sId.i16uFBS_OutputLength,
						piDev_g.devs->dev[i].i16uOutputLength);
					result = PICONTROL_CONFIG_ERROR_WRONG_OUTPUT_LENGTH;
					RevPiDevice_setStatus(0, PICONTROL_STATUS_SIZE_MISMATCH);
					break;
				}
				// we found the device in the configuration file
				// -> adjust offsets
				pr_info_master("Adjust: base %d in %d out %d conf %d\n",
					       piDev_g.devs->dev[i].i16uBaseOffset,
					       piDev_g.devs->dev[i].i16uInputOffset,
					       piDev_g.devs->dev[i].i16uOutputOffset,
					       piDev_g.devs->dev[i].i16uConfigOffset);

				RevPiDevice_getDev(j)->i16uInputOffset = piDev_g.devs->dev[i].i16uInputOffset;
				RevPiDevice_getDev(j)->i16uOutputOffset = piDev_g.devs->dev[i].i16uOutputOffset;
				RevPiDevice_getDev(j)->i16uConfigOffset = piDev_g.devs->dev[i].i16uConfigOffset;
				RevPiDevice_getDev(j)->i16uConfigLength = piDev_g.devs->dev[i].i16uConfigLength;
				if (j == 0) {
					SRevPiCompact *machine = (SRevPiCompact *)piDev_g.machine;
					machine->config.offset = RevPiDevice_getDev(0)->i16uInputOffset;
				}

				state[i] = 1;	// dieser Konfigeintrag wurde übernommen
				found = 1;	// innere For-Schrleife verlassen
			}
		}
		if (found == 0) {
			// Falls ein autom. erkanntes Modul in der Konfiguration nicht vorkommt, wird es deakiviert
			RevPiDevice_getDev(j)->i8uActive = 0;
			RevPiDevice_setStatus(0, PICONTROL_STATUS_EXTRA_MODULE);
		}
	}

	// nun wird die Liste der automatisch erkannten Module um die ergänzt, die nur in der Konfiguration vorkommen.
	for (i = 0; i < piDev_g.devs->i16uNumDevices; i++) {
		if (state[i] == 0) {
			j = RevPiDevice_getDevCnt();
			if (piDev_g.devs->dev[i].i16uModuleType >= PICONTROL_SW_OFFSET) {
				// if a module is already defined as software module in the RAP file,
				// it is handled by user space software and therefore always active
				RevPiDevice_getDev(j)->i8uActive = 1;
				RevPiDevice_getDev(j)->sId.i16uModulType = piDev_g.devs->dev[i].i16uModuleType;
			} else {
				pr_err("module type %d is not allowed on a RevPi Compact. Only sw modules are allowed.\n", piDev_g.devs->dev[i].i16uModuleType);
				RevPiDevice_setStatus(0, PICONTROL_STATUS_MISSING_MODULE);
				RevPiDevice_getDev(j)->i8uActive = 0;
				RevPiDevice_getDev(j)->sId.i16uModulType =
				    piDev_g.devs->dev[i].i16uModuleType | PICONTROL_NOT_CONNECTED;
			}
			RevPiDevice_getDev(j)->i8uAddress = piDev_g.devs->dev[i].i8uAddress;
			RevPiDevice_getDev(j)->i8uScan = 0;
			RevPiDevice_getDev(j)->i16uInputOffset = piDev_g.devs->dev[i].i16uInputOffset;
			RevPiDevice_getDev(j)->i16uOutputOffset = piDev_g.devs->dev[i].i16uOutputOffset;
			RevPiDevice_getDev(j)->i16uConfigOffset = piDev_g.devs->dev[i].i16uConfigOffset;
			RevPiDevice_getDev(j)->i16uConfigLength = piDev_g.devs->dev[i].i16uConfigLength;
			RevPiDevice_getDev(j)->sId.i32uSerialnumber = piDev_g.devs->dev[i].i32uSerialnumber;
			RevPiDevice_getDev(j)->sId.i16uHW_Revision = piDev_g.devs->dev[i].i16uHW_Revision;
			RevPiDevice_getDev(j)->sId.i16uSW_Major = piDev_g.devs->dev[i].i16uSW_Major;
			RevPiDevice_getDev(j)->sId.i16uSW_Minor = piDev_g.devs->dev[i].i16uSW_Minor;
			RevPiDevice_getDev(j)->sId.i32uSVN_Revision = piDev_g.devs->dev[i].i32uSVN_Revision;
			RevPiDevice_getDev(j)->sId.i16uFBS_InputLength = piDev_g.devs->dev[i].i16uInputLength;
			RevPiDevice_getDev(j)->sId.i16uFBS_OutputLength = piDev_g.devs->dev[i].i16uOutputLength;
			RevPiDevice_getDev(j)->sId.i16uFeatureDescriptor = 0;	// not used
			RevPiDevice_incDevCnt();
		}
	}

	kfree(state);
}

int revpi_compact_probe(struct platform_device *pdev)
{
	SRevPiCompact *machine;
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
	seqlock_init(&machine->stats.lock);

	machine->din =  gpiod_get_array(piDev_g.dev, "din", GPIOD_ASIS);
	if (IS_ERR(machine->din)) {
		pr_err("cannot acquire digital input pins\n");
		ret = PTR_ERR(machine->din);
		goto err_remove_table;
	}

	machine->dout = gpiod_get_array(piDev_g.dev, "dout", GPIOD_ASIS);
	if (IS_ERR(machine->dout)) {
		pr_err("cannot acquire digital output pins\n");
		ret = PTR_ERR(machine->dout);
		goto err_put_din;
	}

	ret = gpiod_set_debounce(machine->din->desc[0], machine->config.din_debounce);
	if (ret)
		pr_err("cannot set din debounce\n");

	machine->din_dev = bus_find_device(&spi_bus_type, NULL, "max31913",
					   match_name);
	if (!machine->din_dev) {
		pr_err("cannot find digital input device\n");
		ret = -ENODEV;
		goto err_put_dout;
	}

	dev = bus_find_device(&spi_bus_type, NULL, "74hc595", match_name);
	if (!dev) {
		pr_err("cannot find digital output device\n");
		ret = -ENODEV;
		goto err_put_din_dev;
	}

	machine->dout_fault = gpiod_get(dev, "kunbus,fault", GPIOD_IN);
	put_device(dev);
	if (IS_ERR(machine->dout_fault)) {
		pr_err("cannot acquire digital output fault pin\n");
		ret = PTR_ERR(machine->dout_fault);
		goto err_put_din_dev;
	}

	dev = bus_find_device(&iio_bus_type, NULL, "ain_muxed", match_name);
	if (!dev) {
		pr_err("cannot find analog input device\n");
		ret = -ENODEV;
		goto err_put_dout_fault;
	}
	machine->ain_dev = dev_to_iio_dev(dev);
	ret = iio_map_array_register(machine->ain_dev,
				     (struct iio_map *)revpi_compact_ain);
	if (ret) {
		put_device(dev);
		goto err_put_dout_fault;
	}

	machine->ain = iio_channel_get_all(piDev_g.dev);
	put_device(dev);
	if (IS_ERR(machine->ain)) {
		pr_err("cannot acquire analog input chans\n");
		ret = PTR_ERR(machine->ain);
		goto err_unregister_ain;
	}

	dev = bus_find_device(&iio_bus_type, NULL, "dac082s085", match_name);
	if (!dev) {
		pr_err("cannot find analog output device\n");
		ret = -ENODEV;
		goto err_release_ain;
	}
	machine->aout_dev = dev_to_iio_dev(dev);
	ret = iio_map_array_register(machine->aout_dev,
				     (struct iio_map *)revpi_compact_aout);
	if (ret) {
		put_device(dev);
		goto err_release_ain;
	}

	machine->aout[0] = iio_channel_get(piDev_g.dev, "aout0");
	put_device(dev);
	if (IS_ERR(machine->aout[0])) {
		pr_err("cannot acquire analog output chan 0\n");
		ret = PTR_ERR(machine->aout[0]);
		goto err_unregister_aout;
	}

	machine->aout[1] = iio_channel_get(piDev_g.dev, "aout1");
	if (IS_ERR(machine->aout[1])) {
		pr_err("cannot acquire analog output chan 1\n");
		ret = PTR_ERR(machine->aout[1]);
		goto err_release_aout0;
	}

	machine->io_thread = kthread_create(&revpi_compact_poll_io, machine,
					    "piControl i/o");
	if (IS_ERR(machine->io_thread)) {
		pr_err("cannot create i/o thread\n");
		ret = PTR_ERR(machine->io_thread);
		goto err_release_aout1;
	}

	ret = set_rt_priority(machine->io_thread, IO_THREAD_PRIO);
	if (ret) {
		pr_err("cannot upgrade i/o thread priority\n");
		goto err_stop_io_thread;
	}

	machine->ain_thread = kthread_create(&revpi_compact_poll_ain, machine,
					     "piControl ain");
	if (IS_ERR(machine->ain_thread)) {
		pr_err("cannot create ain thread\n");
		ret = PTR_ERR(machine->ain_thread);
		goto err_stop_io_thread;
	}

	ret = set_rt_priority(machine->ain_thread, AIN_THREAD_PRIO);
	if (ret) {
		pr_err("cannot upgrade ain thread priority\n");
		goto err_stop_ain_thread;
	}

	ret = set_kthread_prios(revpi_compact_kthread_prios);
	if (ret)
		goto err_stop_ain_thread;

	ret = device_create_file(piDev_g.dev, &dev_attr_lost_cycles);
	if (ret) {
		pr_err("failed to create device file: %i\n", ret);
		goto err_stop_ain_thread;
	}

	revpi_compact_reset();

	wake_up_process(machine->io_thread);
	wake_up_process(machine->ain_thread);

	return 0;

err_stop_ain_thread:
	kthread_stop(machine->ain_thread);
err_stop_io_thread:
	kthread_stop(machine->io_thread);
err_release_aout1:
	iio_channel_release(machine->aout[1]);
err_release_aout0:
	iio_channel_release(machine->aout[0]);
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

void revpi_compact_remove(struct platform_device *pdev)
{
	SRevPiCompact *machine = (SRevPiCompact *)piDev_g.machine;

	if (!machine)
		return;

	device_remove_file(piDev_g.dev, &dev_attr_lost_cycles);

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

int revpi_compact_reset(void)
{
	SRevPiCompact *machine = piDev_g.machine;
	SRevPiCompactImage *image = (SRevPiCompactImage *)piDev_g.ai8uPI +
				    machine->config.offset;
	int ret;

	/* disallow access to process image while offsets are changed */
	my_rt_mutex_lock(&piDev_g.lockPI);
	revpi_compact_adjust_config();
	memset(&image->usr, 0, sizeof(image->usr));
	if (piDev_g.ent)
		revpi_set_defaults(piDev_g.ai8uPI, piDev_g.ent);
	rt_mutex_unlock(&piDev_g.lockPI);

	machine->config = revpi_compact_config_g;

	ret = gpiod_set_debounce(machine->din->desc[0], machine->config.din_debounce);
	if (ret)
		pr_err("cannot set din debounce\n");

	reinit_completion(&machine->ain_reset);
	smp_store_release(&machine->ain_should_reset, true);
	wake_up_process(machine->ain_thread);
	wait_for_completion(&machine->ain_reset);

	return 0;
}

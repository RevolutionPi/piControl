// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: 2016-2023 KUNBUS GmbH

/******************************************************************************/
/********************************  Includes  **********************************/
/******************************************************************************/
//#define DEBUG


#include <linux/fs.h>
#include <linux/list.h>
#include <linux/semaphore.h>
#include <linux/thermal.h>
#include <linux/version.h>
#include <linux/wait.h>
#include <linux/firmware.h>
#include <linux/platform_device.h>

#include "IoProtocol.h"
#include "ModGateRS485.h"
#include "piConfig.h"
#include "piControlMain.h"
#include "piFirmwareUpdate.h"
#include "PiBridgeMaster.h"
#include "revpi_flat.h"
#include "revpi_compact.h"
#include "revpi_common.h"
#include "revpi_core.h"
#include "RevPiDevice.h"

#define FIRMWARE_FILENAME_LEN			32

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Christof Vogt, Mathias Duckeck, Lukas Wunner");
MODULE_DESCRIPTION("piControl Driver");
MODULE_VERSION("2.3.7");
MODULE_SOFTDEP("pre: bcm2835-thermal "	/* cpu temp in process image */
	       "ks8851 "		/* core eth gateways */
	       "spi-bcm2835 "		/* core spi0 eth gateways */
	       "spi-bcm2835aux "	/* compact spi2 i/o */
	       "gpio-max3191x "		/* compact din */
	       "gpio-74x164 "		/* compact dout */
	       "fixed "			/* compact ain/aout vref */
	       "mux_gpio "		/* compact ain mux */
	       "iio_mux "		/* compact ain mux */
	       "mcp320x "		/* compact ain */
	       "ti-dac082s085 "		/* compact aout */
	       "ad5446");		/* flat aout */

static unsigned int picontrol_max_cycle_deviation;
static unsigned int picontrol_cycle_duration;

module_param(picontrol_max_cycle_deviation, uint, S_IRUSR);
MODULE_PARM_DESC(picontrol_max_cycle_deviation,
	"Specify the max tolerated deviation from a fixed io-cycle in usecs.");

module_param(picontrol_cycle_duration, uint, S_IRUSR);
MODULE_PARM_DESC(picontrol_cycle_duration, "Specify a fixed io-cycle duration in usecs. "
					   "Use 0 to use the fastest possible io-cycle duration.");
/******************************************************************************/
/******************************  Prototypes  **********************************/
/******************************************************************************/

static int piControlOpen(struct inode *inode, struct file *file);
static int piControlRelease(struct inode *inode, struct file *file);
static ssize_t piControlRead(struct file *file, char __user * pBuf, size_t count, loff_t * ppos);
static ssize_t piControlWrite(struct file *file, const char __user * pBuf, size_t count, loff_t * ppos);
static loff_t piControlSeek(struct file *file, loff_t off, int whence);
static long piControlIoctl(struct file *file, unsigned int prg_nr, unsigned long usr_addr);

/******************************************************************************/
/******************************  Global Vars  *********************************/
/******************************************************************************/

static struct file_operations piControlFops = {
owner:	THIS_MODULE,
read:	piControlRead,
write:	piControlWrite,
llseek:piControlSeek,
open:	piControlOpen,
unlocked_ioctl:piControlIoctl,
release:piControlRelease
};

tpiControlDev piDev_g = {
	.power_red.name = "power_red",
	.a1_green.name = "a1_green",
	.a1_red.name = "a1_red",
	.a1_blue.name = "a1_blue",
	.a2_green.name = "a2_green",
	.a2_red.name = "a2_red",
	.a2_blue.name = "a2_blue",
	.a3_green.name = "a3_green",
	.a3_red.name = "a3_red",
	.a3_blue.name = "a3_blue",
	.a4_green.name = "a4_green",
	.a4_red.name = "a4_red",
	.a4_blue.name = "a4_blue",
	.a5_green.name = "a5_green",
	.a5_red.name = "a5_red",
	.a5_blue.name = "a5_blue",
};

static dev_t piControlMajor;
static struct class *piControlClass;

/******************************************************************************/
/*******************************  Functions  **********************************/
/******************************************************************************/

static bool waitRunning(int timeout);	// ms

/*****************************************************************************/
/*       I N I T                                                             */
/*****************************************************************************/
#ifdef UART_TEST
void piControlDummyReceive(INT8U i8uChar_p)
{
	pr_info("Got character %c\n", i8uChar_p);
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 2, 0)
static char *piControlClass_devnode(struct device *dev, umode_t * mode)
#else
static char *piControlClass_devnode(const struct device *dev, umode_t * mode)
#endif
{
	if (mode)
		*mode = S_IRUGO | S_IWUGO;

	return NULL;
}

unsigned int piControl_get_cycle_duration(void)
{
	struct picontrol_cycle *cycle = &piDev_g.cycle;
	unsigned int duration;
	unsigned int seq;

	do {
		seq = read_seqbegin(&cycle->lock);
		duration = cycle->duration;
	} while (read_seqretry(&cycle->lock, seq));

	return duration;
}

static ssize_t last_cycle_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct picontrol_cycle *cycle = &piDev_g.cycle;
	unsigned int last;
	unsigned int seq;

	do {
		seq = read_seqbegin(&cycle->lock);
		last = cycle->last;
	} while (read_seqretry(&cycle->lock, seq));

	return sprintf(buf, "%u\n", last);
}

static ssize_t min_cycle_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct picontrol_cycle *cycle = &piDev_g.cycle;
	unsigned int min;
	unsigned int seq;

	do {
		seq = read_seqbegin(&cycle->lock);
		min = cycle->min;
	} while (read_seqretry(&cycle->lock, seq));

	return sprintf(buf, "%u\n", min);
}

static ssize_t min_cycle_store(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{
	struct picontrol_cycle *cycle = &piDev_g.cycle;
	unsigned long val;

	if (kstrtoul(buf, 10, &val))
		return -EINVAL;

	if (val != 0)
		return -EINVAL;

	write_seqlock(&cycle->lock);
	cycle->min = UINT_MAX;
	write_sequnlock(&cycle->lock);

	return count;
}

static ssize_t max_cycle_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct picontrol_cycle *cycle = &piDev_g.cycle;
	unsigned int max;
	unsigned int seq;

	do {
		seq = read_seqbegin(&cycle->lock);
		max = cycle->max;
	} while (read_seqretry(&cycle->lock, seq));

	return sprintf(buf, "%u\n", max);
}

static ssize_t max_cycle_store(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{
	struct picontrol_cycle *cycle = &piDev_g.cycle;
	unsigned long val;

	if (kstrtoul(buf, 10, &val))
		return -EINVAL;

	if (val != 0)
		return -EINVAL;

	write_seqlock(&cycle->lock);
	cycle->max = val;
	write_sequnlock(&cycle->lock);

	return count;
}

static ssize_t cycle_duration_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", piControl_get_cycle_duration());
}

static ssize_t cycle_duration_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct picontrol_cycle *cycle = &piDev_g.cycle;
	unsigned long val;

	if (kstrtoul(buf, 10, &val))
		return -EINVAL;

	val = min(val, PICONTROL_CYCLE_MAX_DURATION);
	val = max(val, PICONTROL_CYCLE_MIN_DURATION);

	write_seqlock(&cycle->lock);
	cycle->duration = val;
	write_sequnlock(&cycle->lock);

	return count;
}

static ssize_t max_cycle_deviation_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct picontrol_cycle *cycle = &piDev_g.cycle;
	unsigned int max;
	unsigned int seq;

	do {
		seq = read_seqbegin(&cycle->lock);
		max = cycle->max_deviation;
	} while (read_seqretry(&cycle->lock, seq));

	return sprintf(buf, "%u\n", max);
}

static ssize_t max_cycle_deviation_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	struct picontrol_cycle *cycle = &piDev_g.cycle;
	unsigned long val;

	if (kstrtoul(buf, 10, &val))
		return -EINVAL;

	write_seqlock(&cycle->lock);
	cycle->max_deviation = val;
	write_sequnlock(&cycle->lock);

	return count;
}

static ssize_t cycles_exceeded_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct picontrol_cycle *cycle = &piDev_g.cycle;
	u64 exceeded;

	write_seqlock(&cycle->lock);
	exceeded = cycle->exceeded;
	write_sequnlock(&cycle->lock);

	return sprintf(buf, "%llu\n", exceeded);
}

static ssize_t cycles_exceeded_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct picontrol_cycle *cycle = &piDev_g.cycle;
	unsigned long val;

	if (kstrtoul(buf, 10, &val))
		return -EINVAL;

	if (val != 0)
		return -EINVAL;

	write_seqlock(&cycle->lock);
	cycle->exceeded = 0;
	write_sequnlock(&cycle->lock);

	return count;
}

static ssize_t cycles_missed_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct picontrol_cycle *cycle = &piDev_g.cycle;
	u64 missed;

	write_seqlock(&cycle->lock);
	missed = cycle->missed;
	write_sequnlock(&cycle->lock);

	return sprintf(buf, "%llu\n", missed);
}

static ssize_t cycles_missed_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct picontrol_cycle *cycle = &piDev_g.cycle;
	unsigned long val;

	if (kstrtoul(buf, 10, &val))
		return -EINVAL;

	if (val != 0)
		return -EINVAL;

	write_seqlock(&cycle->lock);
	cycle->missed = 0;
	write_sequnlock(&cycle->lock);

	return count;
}

static DEVICE_ATTR_RW(cycle_duration);
static DEVICE_ATTR_RW(max_cycle);
static DEVICE_ATTR_RW(min_cycle);
static DEVICE_ATTR_RO(last_cycle);
static DEVICE_ATTR_RW(max_cycle_deviation);
static DEVICE_ATTR_RW(cycles_exceeded);
static DEVICE_ATTR_RW(cycles_missed);

static int piControl_init_sysfs(void)
{
	int ret;

	ret = sysfs_create_file(&piDev_g.dev->kobj, &dev_attr_cycle_duration.attr);
	if (ret)
		return ret;

	ret = sysfs_create_file(&piDev_g.dev->kobj, &dev_attr_max_cycle.attr);
	if (ret)
		goto remove_cycle_duration_file;

	ret = sysfs_create_file(&piDev_g.dev->kobj, &dev_attr_min_cycle.attr);
	if (ret)
		goto remove_max_cycle_file;

	ret = sysfs_create_file(&piDev_g.dev->kobj, &dev_attr_last_cycle.attr);
	if (ret)
		goto remove_min_cycle_file;

	ret = sysfs_create_file(&piDev_g.dev->kobj, &dev_attr_max_cycle_deviation.attr);
	if (ret)
		goto remove_last_cycle_file;

	ret = sysfs_create_file(&piDev_g.dev->kobj, &dev_attr_cycles_exceeded.attr);
	if (ret)
		goto remove_max_cycle_deviation_file;

	ret = sysfs_create_file(&piDev_g.dev->kobj, &dev_attr_cycles_missed.attr);
	if (ret)
		goto remove_exceeded_cycles_file;

	return 0;

remove_exceeded_cycles_file:
	sysfs_remove_file(&piDev_g.dev->kobj, &dev_attr_cycles_exceeded.attr);
remove_max_cycle_deviation_file:
	sysfs_remove_file(&piDev_g.dev->kobj, &dev_attr_max_cycle_deviation.attr);
remove_last_cycle_file:
	sysfs_remove_file(&piDev_g.dev->kobj, &dev_attr_last_cycle.attr);
remove_min_cycle_file:
	sysfs_remove_file(&piDev_g.dev->kobj, &dev_attr_min_cycle.attr);
remove_max_cycle_file:
	sysfs_remove_file(&piDev_g.dev->kobj, &dev_attr_max_cycle.attr);
remove_cycle_duration_file:
	sysfs_remove_file(&piDev_g.dev->kobj, &dev_attr_cycle_duration.attr);

	return ret;
}

static void piControl_deinit_sysfs(void)
{
	sysfs_remove_file(&piDev_g.dev->kobj, &dev_attr_cycles_missed.attr);
	sysfs_remove_file(&piDev_g.dev->kobj, &dev_attr_cycles_exceeded.attr);
	sysfs_remove_file(&piDev_g.dev->kobj, &dev_attr_max_cycle_deviation.attr);
	sysfs_remove_file(&piDev_g.dev->kobj, &dev_attr_last_cycle.attr);
	sysfs_remove_file(&piDev_g.dev->kobj, &dev_attr_min_cycle.attr);
	sysfs_remove_file(&piDev_g.dev->kobj, &dev_attr_max_cycle.attr);
	sysfs_remove_file(&piDev_g.dev->kobj, &dev_attr_cycle_duration.attr);
}

static int pibridge_probe(struct platform_device *pdev)
{
	int devindex = 0;
	dev_t curdev;
	int res;

	if (of_machine_is_compatible("kunbus,revpi-compact")) {
		piDev_g.machine_type = REVPI_COMPACT;
		pr_info("running on RevPi Compact\n");
	} else if (of_machine_is_compatible("kunbus,revpi-connect")) {
		piDev_g.machine_type = REVPI_CONNECT;
		piDev_g.pibridge_supported = 1;
		piDev_g.only_left_pibridge = 1;
		piDev_g.revpi_gate_supported = 1;
		pr_info("running on RevPi Connect\n");
	} else if (of_machine_is_compatible("kunbus,revpi-connect-se")) {
		piDev_g.machine_type = REVPI_CONNECT_SE;
		piDev_g.pibridge_supported = 1;
		piDev_g.only_left_pibridge = 1;
		pr_info("running on RevPi Connect SE\n");
	} else if (of_machine_is_compatible("kunbus,revpi-connect4")) {
		piDev_g.machine_type = REVPI_CONNECT_4;
		piDev_g.pibridge_supported = 1;
		pr_info("running on RevPi Connect 4\n");
	} else if (of_machine_is_compatible("kunbus,revpi-connect5")) {
		piDev_g.machine_type = REVPI_CONNECT_5;
		piDev_g.pibridge_supported = 1;
		piDev_g.revpi_gate_supported = 1;
		pr_info("running on RevPi Connect 5\n");
	} else if (of_machine_is_compatible("kunbus,revpi-pibridge")) {
		piDev_g.machine_type = REVPI_GENERIC_PB;
		piDev_g.pibridge_supported = 1;
		piDev_g.revpi_gate_supported = 1;
		pr_info("running on RevPi Generic PiBridge\n");
	} else if (of_machine_is_compatible("kunbus,revpi-flat")) {
		piDev_g.machine_type = REVPI_FLAT;
		pr_info("running on RevPi Flat\n");
	} else if (of_machine_is_compatible("kunbus,revpi-core-se")) {
		piDev_g.machine_type = REVPI_CORE_SE;
		piDev_g.pibridge_supported = 1;
		pr_info("running on RevPi Core SE\n");
	} else {
		piDev_g.machine_type = REVPI_CORE;
		piDev_g.pibridge_supported = 1;
		piDev_g.revpi_gate_supported = 1;
		pr_info("running on RevPi Core\n");
	}

	// alloc_chrdev_region return 0 on success
	res = alloc_chrdev_region(&piControlMajor, 0, 2, "piControl");
	if (res) {
		pr_err("cannot allocate char device numbers\n");
		return res;
	}

	seqlock_init(&piDev_g.cycle.lock);

	piDev_g.cycle.duration = PICONTROL_DEFAULT_CYCLE_DURATION;
	if (picontrol_cycle_duration) {
		if (picontrol_cycle_duration > PICONTROL_CYCLE_MAX_DURATION) {
			pr_warn("Invalid cycle duration %u specified (max=%u)\n",
				picontrol_cycle_duration, PICONTROL_CYCLE_MAX_DURATION);
		} else {
			piDev_g.cycle.duration = picontrol_cycle_duration;

			if (piDev_g.cycle.duration < PICONTROL_CYCLE_MIN_DURATION)
				piDev_g.cycle.duration = PICONTROL_CYCLE_MIN_DURATION;
			pr_info("Using cycle duration %u\n",
				piDev_g.cycle.duration);
		}

	}

	if (picontrol_max_cycle_deviation) {
		piDev_g.cycle.max_deviation = picontrol_max_cycle_deviation;
		pr_info("Using max cycle deviation %u\n",
			piDev_g.cycle.max_deviation);
	}

	piDev_g.cycle.last = 0;
	piDev_g.cycle.max = 0;
	/*
	 * Initialize with the highest possible value to make sure it is
	 * overwritten with the next measured time span.
	 */
	piDev_g.cycle.min = UINT_MAX;

	pr_debug("MAJOR-No.  : %d\n", MAJOR(piControlMajor));

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 4, 0)
	piControlClass = class_create(THIS_MODULE, "piControl");
#else
	piControlClass = class_create("piControl");
#endif
	if (IS_ERR(piControlClass)) {
		pr_err("cannot create class\n");
		res = PTR_ERR(piControlClass);
		goto err_unreg_chrdev_region;
	}
	piControlClass->devnode = piControlClass_devnode;

	/* create device node */
	curdev = MKDEV(MAJOR(piControlMajor), MINOR(piControlMajor) + devindex);

	INIT_LIST_HEAD(&piDev_g.listCon);
	rt_mutex_init(&piDev_g.lockListCon);

	cdev_init(&piDev_g.cdev, &piControlFops);
	piDev_g.cdev.owner = THIS_MODULE;
	piDev_g.cdev.ops = &piControlFops;

	piDev_g.dev = device_create(piControlClass, NULL, curdev, NULL, "piControl%d", devindex);
	if (IS_ERR(piDev_g.dev)) {
		pr_err("cannot create device\n");
		res = PTR_ERR(piDev_g.dev);
		goto err_class_destroy;
	}

	pr_debug("MAJOR-No.  : %d  MINOR-No.  : %d\n", MAJOR(curdev), MINOR(curdev));

	res = piControl_init_sysfs();
	if (res) {
		pr_err("Failed to create sysfs entries: %i\n", res);
		goto err_dev_destroy;
	}

	res = devm_led_trigger_register(piDev_g.dev, &piDev_g.power_red)
	   || devm_led_trigger_register(piDev_g.dev, &piDev_g.a1_green)
	   || devm_led_trigger_register(piDev_g.dev, &piDev_g.a1_red)
	   || devm_led_trigger_register(piDev_g.dev, &piDev_g.a2_green)
	   || devm_led_trigger_register(piDev_g.dev, &piDev_g.a2_red);

	if (piDev_g.machine_type == REVPI_CONNECT ||
	    piDev_g.machine_type == REVPI_CONNECT_SE ||
	    piDev_g.machine_type == REVPI_CONNECT_4 ||
	    piDev_g.machine_type == REVPI_CONNECT_5 ||
	    piDev_g.machine_type == REVPI_FLAT) {
		res = res
		   || devm_led_trigger_register(piDev_g.dev, &piDev_g.a3_green)
		   || devm_led_trigger_register(piDev_g.dev, &piDev_g.a3_red);
	}

	if (piDev_g.machine_type == REVPI_FLAT ||
	    piDev_g.machine_type == REVPI_CONNECT_4 ||
	    piDev_g.machine_type == REVPI_CONNECT_5) {
		res = res
		   || devm_led_trigger_register(piDev_g.dev, &piDev_g.a4_green)
		   || devm_led_trigger_register(piDev_g.dev, &piDev_g.a4_red)
		   || devm_led_trigger_register(piDev_g.dev, &piDev_g.a5_green)
		   || devm_led_trigger_register(piDev_g.dev, &piDev_g.a5_red);
	}

	if (piDev_g.machine_type == REVPI_CONNECT_4 ||
	    piDev_g.machine_type == REVPI_CONNECT_5) {
		res = res
		   || devm_led_trigger_register(piDev_g.dev, &piDev_g.a1_blue)
		   || devm_led_trigger_register(piDev_g.dev, &piDev_g.a2_blue)
		   || devm_led_trigger_register(piDev_g.dev, &piDev_g.a3_blue)
		   || devm_led_trigger_register(piDev_g.dev, &piDev_g.a4_blue)
		   || devm_led_trigger_register(piDev_g.dev, &piDev_g.a5_blue);
	}

	if (res) {
		pr_err("cannot register LED triggers\n");
		res = -ENXIO;
		goto err_sysfs_remove;
	}

	/* init some data */
	rt_mutex_init(&piDev_g.lockPI);
	rt_mutex_init(&piDev_g.lockIoctl);
	clear_bit(PICONTROL_DEV_FLAG_STOP_IO, &piDev_g.flags);

	piDev_g.tLastOutput1 = ktime_set(0, 0);
	piDev_g.tLastOutput2 = ktime_set(0, 0);

	/* start application */
	piConfigParse(PICONFIG_FILE, &piDev_g.devs, &piDev_g.ent, &piDev_g.cl,
		      &piDev_g.connl);

	if (piDev_g.pibridge_supported) {
		res = revpi_core_probe(pdev);
	} else { // standalone device
		if (piDev_g.machine_type == REVPI_COMPACT)
			res = revpi_compact_probe(pdev);
		else if (piDev_g.machine_type == REVPI_FLAT)
			res = revpi_flat_probe(pdev);
	}

	if (res)
		goto err_free_config;

	piDev_g.thermal_zone = thermal_zone_get_zone_by_name("cpu-thermal");
	if (IS_ERR(piDev_g.thermal_zone)) {
		pr_err("cannot find thermal zone\n");
		piDev_g.thermal_zone = NULL;
	}

	res = cdev_add(&piDev_g.cdev, curdev, 1);
	if (res) {
		pr_err("cannot add cdev\n");
		goto err_revpi_fini;
	}

	pr_debug("piControlInit done\n");
	return 0;

err_revpi_fini:
	if (piDev_g.pibridge_supported) {
		if (isRunning())
			PiBridgeMaster_Stop();
		revpi_core_remove(pdev);
	} else { // standalone devices
		if (piDev_g.machine_type == REVPI_COMPACT)
			revpi_compact_remove(pdev);
		else if (piDev_g.machine_type == REVPI_FLAT)
			revpi_flat_remove(pdev);
	}
err_free_config:
	kfree(piDev_g.ent);
	kfree(piDev_g.devs);
err_sysfs_remove:
	piControl_deinit_sysfs();
err_dev_destroy:
	device_destroy(piControlClass, curdev);
err_class_destroy:
	class_destroy(piControlClass);
err_unreg_chrdev_region:
	unregister_chrdev_region(piControlMajor, 2);
	return res;
}

/*****************************************************************************/
/*       C L E A N U P                                                       */
/*****************************************************************************/
static int piControlReset(tpiControlInst * priv)
{
	int status = -EFAULT;
	int timeout = 10000;	// ms

	kfree(piDev_g.ent);
	piDev_g.ent = NULL;

	kfree(piDev_g.devs);
	piDev_g.devs = NULL;

	kfree(piDev_g.cl);
	piDev_g.cl = NULL;

	/* start application */
	piConfigParse(PICONFIG_FILE, &piDev_g.devs, &piDev_g.ent, &piDev_g.cl,
		      &piDev_g.connl);

	if (piDev_g.machine_type == REVPI_COMPACT) {
		revpi_compact_reset();
	} else if (piDev_g.machine_type == REVPI_FLAT) {
		revpi_flat_reset();
	} else if (piDev_g.pibridge_supported) {
		PiBridgeMaster_Reset();
	}

	if (!waitRunning(timeout)) {
		status = -ETIMEDOUT;
	} else {
		struct list_head *pCon;

		my_rt_mutex_lock(&piDev_g.lockListCon);
		list_for_each(pCon, &piDev_g.listCon) {
			tpiControlInst *pos_inst;
			pos_inst = list_entry(pCon, tpiControlInst, list);
			if (pos_inst != priv) {
				struct list_head *pEv;
				tpiEventEntry *pEntry;
				bool found = false;

				// add the event to the list only, if it not already there
				my_rt_mutex_lock(&pos_inst->lockEventList);
				list_for_each(pEv, &pos_inst->piEventList) {
					pEntry = list_entry(pEv, tpiEventEntry, list);
					if (pEntry->event == piEvReset) {
						found = true;
						break;
					}
				}

				if (!found) {
					pEntry = kmalloc(sizeof(tpiEventEntry), GFP_KERNEL);
					pEntry->event = piEvReset;
					list_add_tail(&pEntry->list, &pos_inst->piEventList);
					rt_mutex_unlock(&pos_inst->lockEventList);
					wake_up(&pos_inst->wq);
				} else {
					rt_mutex_unlock(&pos_inst->lockEventList);
				}
			}
		}
		rt_mutex_unlock(&piDev_g.lockListCon);

		status = 0;
	}
	return status;
}

#if KERNEL_VERSION(6, 11, 0) <= LINUX_VERSION_CODE
static void pibridge_remove(struct platform_device *pdev)
#else
static int pibridge_remove(struct platform_device *pdev)
#endif
{
	dev_t curdev;

	pr_debug("piControlCleanup\n");

	cdev_del(&piDev_g.cdev);

	if (piDev_g.pibridge_supported) {
		if (isRunning())
			PiBridgeMaster_Stop();
		revpi_core_remove(pdev);
	} else { // standalone devices
		if (piDev_g.machine_type == REVPI_COMPACT)
			revpi_compact_remove(pdev);
		else if (piDev_g.machine_type == REVPI_FLAT)
			revpi_flat_remove(pdev);
	}

	kfree(piDev_g.ent);
	kfree(piDev_g.devs);
	piControl_deinit_sysfs();
	curdev = MKDEV(MAJOR(piControlMajor), MINOR(piControlMajor));
	device_destroy(piControlClass, curdev);
	class_destroy(piControlClass);
	unregister_chrdev_region(piControlMajor, 2);

	pr_debug("driver stopped with MAJOR-No. %d\n\n ", MAJOR(piControlMajor));
	pr_debug("piControlCleanup done\n");

#if KERNEL_VERSION(6, 11, 0) > LINUX_VERSION_CODE
	return 0;
#endif
}

// true: system is running
// false: system is not operational
bool isRunning(void)
{
	if (!piDev_g.pibridge_supported)
		return true;

	return test_bit(PICONTROL_DEV_FLAG_RUNNING, &piDev_g.flags);
}

// true: system is running
// false: system is not operational
static bool waitRunning(int timeout)	// ms
{
	if (!piDev_g.pibridge_supported)
		return true;

	timeout /= 100;
	timeout++;

	while (timeout > 0 && !isRunning()) {
		msleep(100);
		timeout--;
	}
	if (timeout <= 0)
		return false;

	return true;
}

void printUserMsg(tpiControlInst *priv, const char *s, ...)
{
	va_list argp;

	va_start(argp, s);

	vsnprintf(priv->pcErrorMessage, sizeof(priv->pcErrorMessage), s, argp);

	va_end(argp);

	pr_info("%s", priv->pcErrorMessage);
}

/*****************************************************************************/
/*              O P E N                                                      */
/*****************************************************************************/
static int piControlOpen(struct inode *inode, struct file *file)
{
	tpiControlInst *priv;

	priv = (tpiControlInst *) kzalloc(sizeof(tpiControlInst), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	file->private_data = priv;

	/* initalize instance variables */
	priv->dev = piDev_g.dev;
	INIT_LIST_HEAD(&priv->piEventList);
	rt_mutex_init(&priv->lockEventList);

	init_waitqueue_head(&priv->wq);

	my_rt_mutex_lock(&piDev_g.lockListCon);
	list_add(&priv->list, &piDev_g.listCon);
	rt_mutex_unlock(&piDev_g.lockListCon);

	return 0;
}

/*****************************************************************************/
/*       C L O S E                                                           */
/*****************************************************************************/
static int piControlRelease(struct inode *inode, struct file *file)
{
	tpiControlInst *priv;
	struct list_head *pos, *n;

	priv = (tpiControlInst *) file->private_data;

	if (priv->tTimeoutDurationMs > 0) {
		// if the watchdog is active, set all outputs to 0
		int i;
		my_rt_mutex_lock(&piDev_g.lockPI);
		for (i = 0; i < RevPiDevice_getDevCnt(); i++) {
			if (RevPiDevice_getDev(i)->i8uActive) {
				memset(piDev_g.ai8uPI + RevPiDevice_getDev(i)->i16uOutputOffset, 0, RevPiDevice_getDev(i)->sId.i16uFBS_OutputLength);
			}
		}
		rt_mutex_unlock(&piDev_g.lockPI);
	}

	my_rt_mutex_lock(&piDev_g.lockListCon);
	list_del(&priv->list);
	rt_mutex_unlock(&piDev_g.lockListCon);

	list_for_each_safe(pos, n, &priv->piEventList) {
		tpiEventEntry *pos_inst;
		pos_inst = list_entry(pos, tpiEventEntry, list);
		list_del(pos);
		kfree(pos_inst);
	}

	kfree(priv);

	return 0;
}

/*****************************************************************************/
/*    R E A D                                                                */
/*****************************************************************************/
static ssize_t piControlRead(struct file *file, char __user * pBuf, size_t count, loff_t * ppos)
{
	tpiControlInst *priv;
	INT8U *pPd;
	size_t nread = count;

	if (file == 0 || pBuf == 0 || ppos == 0)
		return -EINVAL;

	if (!isRunning())
		return -EAGAIN;

	priv = (tpiControlInst *) file->private_data;

	dev_dbg(priv->dev, "piControlRead Count: %zu, Pos: %llu", count, *ppos);

	if (*ppos < 0 || *ppos >= KB_PI_LEN) {
		return 0;	// end of file
	}

	if (nread + *ppos > KB_PI_LEN) {
		nread = KB_PI_LEN - *ppos;
	}

	pPd = piDev_g.ai8uPI + *ppos;

	my_rt_mutex_lock(&piDev_g.lockPI);
	if (copy_to_user(pBuf, pPd, nread) != 0) {
		rt_mutex_unlock(&piDev_g.lockPI);
		pr_err("piControlRead: copy_to_user failed");
		return -EFAULT;
	}
	rt_mutex_unlock(&piDev_g.lockPI);

	*ppos += nread;

	return nread;		// length read
}

/*****************************************************************************/
/*    W R I T E                                                              */
/*****************************************************************************/
static ssize_t piControlWrite(struct file *file, const char __user * pBuf, size_t count, loff_t * ppos)
{
	tpiControlInst *priv;
	INT8U *pPd;
	size_t nwrite = count;

	if (!isRunning())
		return -EAGAIN;

	priv = (tpiControlInst *) file->private_data;

	dev_dbg(priv->dev, "piControlWrite Count: %zu, Pos: %llu", count, *ppos);

	if (*ppos < 0 || *ppos >= KB_PI_LEN) {
		return 0;	// end of file
	}

	if (nwrite + *ppos > KB_PI_LEN) {
		nwrite = KB_PI_LEN - *ppos;
	}

	pPd = piDev_g.ai8uPI + *ppos;

	my_rt_mutex_lock(&piDev_g.lockPI);
	if (copy_from_user(pPd, pBuf, nwrite) != 0) {
		rt_mutex_unlock(&piDev_g.lockPI);
		pr_err("piControlWrite: copy_from_user failed");
		return -EFAULT;
	}
	rt_mutex_unlock(&piDev_g.lockPI);
	*ppos += nwrite;

	if (priv->tTimeoutDurationMs > 0) {
		priv->tTimeoutTS = ktime_add_ms(ktime_get(), priv->tTimeoutDurationMs);
	}

	return nwrite;		// length written
}

/*****************************************************************************/
/*    S E E K                                                           */
/*****************************************************************************/
static loff_t piControlSeek(struct file *file, loff_t off, int whence)
{
	tpiControlInst *priv;
	loff_t newpos;

	if (!isRunning())
		return -EAGAIN;

	priv = (tpiControlInst *) file->private_data;

	switch (whence) {
	case 0:		/* SEEK_SET */
		newpos = off;
		break;

	case 1:		/* SEEK_CUR */
		newpos = file->f_pos + off;
		break;

	case 2:		/* SEEK_END */
		newpos = KB_PI_LEN + off;
		break;

	default:		/* can't happen */
		return -EINVAL;
	}

	if (newpos < 0 || newpos >= KB_PI_LEN)
		return -EINVAL;

	file->f_pos = newpos;
	return newpos;
}

static int picontrol_upload_firmware(struct picontrol_firmware_upload *fwu,
				     tpiControlInst *priv)
{
	char fw_filename[FIRMWARE_FILENAME_LEN];
	const struct firmware *fw;
	unsigned int module_type;
	bool in_rescue_mode;
	unsigned int hw_rev;
	SDevice *sdev;
	int ret;
	int i;

	in_rescue_mode = !!(fwu->flags & PICONTROL_FIRMWARE_RESCUE_MODE);

	if (!isRunning()) {
		pr_err("PiBridge communication halted, not updating firmware\n");
		return -ENXIO;
	}

	for (i = 0; i < RevPiDevice_getDevCnt(); i++) {
		sdev = RevPiDevice_getDev(i);
		if (fwu->addr == sdev->i8uAddress)
			break;
	}
	if (i == RevPiDevice_getDevCnt())
		return -ENODEV;

	module_type = sdev->sId.i16uModulType;
	hw_rev = sdev->sId.i16uHW_Revision;

	if (in_rescue_mode) {
		module_type &= ~PICONTROL_NOT_CONNECTED;
		hw_rev = fwu->rescue_mode_hw_revision;
		pr_info("Using firmware rescue mode with hardware revision %u\n",
			hw_rev);
	}

	if ((module_type & PICONTROL_NOT_CONNECTED)) {
		pr_err("Disconnected modules can't be updated\n");
		return -ENODEV;
	}

	if (module_type >= PICONTROL_SW_OFFSET) {
		pr_err("Virtual modules don't have firmware to update\n");
		return -EOPNOTSUPP;
	}

	snprintf(fw_filename, sizeof(fw_filename), "revpi/fw_%05d_%03d.fwu",
		 module_type, hw_rev);

	ret = request_firmware(&fw, fw_filename, piDev_g.dev);
	if (ret) {
		pr_err("Failed to load firmware %s: %i\n", fw_filename, ret);
		return -EIO;
	}

	PiBridgeMaster_Stop();
	msleep(50);

	pr_info("Uploading firmware %s to module %u\n", fw_filename,
		sdev->i8uAddress);
	ret = upload_firmware(sdev, fw, fwu->flags, module_type, hw_rev);
	release_firmware(fw);

	/* firmware already up to date */
	if (ret < 0)
		pr_err("Errors during firmware upload\n");

	if (piControlReset(priv) < 0)
		pr_err("Failed to reset piControl\n");

	return ret;
}

static void picontrol_set_device_info(SDeviceInfo *out, SDevice *dev)
{
	out->i8uAddress = dev->i8uAddress;
	out->i8uActive = dev->i8uActive;
	out->i32uSerialnumber = dev->sId.i32uSerialnumber;
	out->i16uModuleType = dev->sId.i16uModulType;
	out->i16uHW_Revision = dev->sId.i16uHW_Revision;
	out->i16uSW_Major = dev->sId.i16uSW_Major;
	out->i16uSW_Minor = dev->sId.i16uSW_Minor;
	out->i32uSVN_Revision = dev->sId.i32uSVN_Revision;
	out->i16uInputLength = dev->sId.i16uFBS_InputLength;
	out->i16uInputOffset = dev->i16uInputOffset;
	out->i16uOutputLength = dev->sId.i16uFBS_OutputLength;
	out->i16uOutputOffset = dev->i16uOutputOffset;
	out->i16uConfigLength = dev->i16uConfigLength;
	out->i16uConfigOffset = dev->i16uConfigOffset;
	out->i8uModuleState = dev->i8uModuleState;
}

static int picontrol_get_device_info(SDeviceInfo *dev_info)
{
	int i;
	bool found;
	SDevice *dev;

	for (i = 0, found = false; i < RevPiDevice_getDevCnt(); i++) {
		dev = RevPiDevice_getDev(i);
		if ((dev_info->i8uAddress == dev->i8uAddress) ||
		    (dev_info->i16uModuleType == dev->sId.i16uModulType)) {
			found = true;
			break;
		}
	}

	if (found) {
		picontrol_set_device_info(dev_info, dev);
		return i;
	}

	/* nothing found */
	return -ENXIO;
}

static int send_internal_gate_telegram(struct modgate_telegram *req_tel,
				       struct modgate_telegram *resp_tel)
{
	struct pibridge_gate_datagram *req_dgram = &piCore_g.gate_req_dgram;
	struct pibridge_gate_datagram *resp_dgram = &piCore_g.gate_resp_dgram;
	int ret;

	my_rt_mutex_lock(&piCore_g.lockGateTel);
	req_dgram->hdr.dst = req_tel->dest;
	req_dgram->hdr.src = req_tel->src;
	req_dgram->hdr.cmd = req_tel->command;
	req_dgram->hdr.seq = req_tel->sequence;
	req_dgram->hdr.len = req_tel->datalen;
	memcpy(req_dgram->data, req_tel->data, req_tel->datalen);
	piCore_g.pendingGateTel = true;
	rt_mutex_unlock(&piCore_g.lockGateTel);

	/* Wait for response */
	down(&piCore_g.semGateTel);

	my_rt_mutex_lock(&piCore_g.lockGateTel);
	ret = piCore_g.statusGateTel;
	if (ret <= 0) { /* No response datagram available */
		rt_mutex_unlock(&piCore_g.lockGateTel);
		return ret;
	}

	if (resp_tel) {
		resp_tel->dest = resp_dgram->hdr.dst;
		resp_tel->src = resp_dgram->hdr.src;
		resp_tel->command = resp_dgram->hdr.cmd;
		resp_tel->sequence = resp_dgram->hdr.seq;
		resp_tel->datalen = resp_dgram->hdr.len;

		if (resp_dgram->hdr.len)
			memcpy(resp_tel->data, resp_dgram->data,
			       resp_dgram->hdr.len);
	}
	rt_mutex_unlock(&piCore_g.lockGateTel);

	return ret;
}

static int send_config(unsigned long usr_addr)
{
	SConfigData __user *cfg_user = (SConfigData __user *) usr_addr;
	struct modgate_telegram *resp;
	struct modgate_telegram *req;
	SConfigData cfg;
	int ret;

	if (!piDev_g.revpi_gate_supported)
		return -EPERM;

	if (isRunning())
		return -ECANCELED;

	if (copy_from_user(&cfg, cfg_user, sizeof(cfg)))
		return -EFAULT;

	if (cfg.i16uLen > MAX_TELEGRAM_DATA_SIZE)
		return -EINVAL;

	req = kmalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kmalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		ret = -ENOMEM;
		goto free_req;
	}

	req->dest = cfg.bLeft ? RevPiDevice_getDev(piCore_g.i8uLeftMGateIdx)->i8uAddress :
				RevPiDevice_getDev(piCore_g.i8uRightMGateIdx)->i8uAddress;
	req->src = 0;
	req->command = eCmdRAPIMessage;
	req->sequence = 0;
	req->datalen = cfg.i16uLen;
	if (req->datalen)
		memcpy(req->data, cfg.acData, cfg.i16uLen);

	ret = send_internal_gate_telegram(req, resp);
	if (ret > 0) {
		put_user(resp->datalen, &cfg_user->i16uLen);

		if (resp->datalen && copy_to_user(cfg_user, resp->data,
						  resp->datalen))
			ret = -EFAULT;
	}

	kfree(resp);
free_req:
	kfree(req);

	return ret;
}

static int send_internal_gate_msg(unsigned long usr_addr)
{
	struct modgate_telegram __user *tel = (struct modgate_telegram __user *) usr_addr;
	struct modgate_telegram *resp;
	struct modgate_telegram *req;
	int ret;

	if (!piDev_g.pibridge_supported)
		return -EOPNOTSUPP;

	req = kmalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kmalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		ret = -ENOMEM;
		goto free_req;
	}

	if (copy_from_user(req, tel, sizeof(*tel))) {
		ret = -EFAULT;
		goto free_resp;
	}

	ret = send_internal_gate_telegram(req, resp);
	if (ret > 0) {
		if (copy_to_user(tel, resp, sizeof(*tel)))
			ret = -EFAULT;
	}

free_resp:
	kfree(resp);
free_req:
	kfree(req);

	return ret;
}

static int send_internal_io_telegram(void *req, unsigned int reqlen,
				     SIOGeneric *resp)
{
	int ret;

	my_rt_mutex_lock(&piCore_g.lockUserTel);
	memcpy(&piCore_g.requestUserTel, req, reqlen);
	piCore_g.pendingUserTel = true;
	rt_mutex_unlock(&piCore_g.lockUserTel);

	/* Wait for response */
	down(&piCore_g.semUserTel);

	my_rt_mutex_lock(&piCore_g.lockUserTel);
	ret = piCore_g.statusUserTel;
	if (ret) {
		rt_mutex_unlock(&piCore_g.lockUserTel);
		return ret;
	}
	if (resp)
		memcpy(resp, &piCore_g.responseUserTel, sizeof(*resp));
	rt_mutex_unlock(&piCore_g.lockUserTel);

	return 0;
}

static int reset_dio_counter(unsigned long usr_addr)
{
	SDIOResetCounter res_cnt;
	SDioCounterReset tel;
	bool found = false;
	int i;

	if (!piDev_g.pibridge_supported)
		return -EPERM;

	if (!isRunning())
		return -EFAULT;

	if (copy_from_user(&res_cnt, (const void __user *) usr_addr,
			   sizeof(res_cnt))) {
		pr_err("failed to copy reset counter from user\n");
		return -EFAULT;
	}

	for (i = 0; i < RevPiDevice_getDevCnt() && !found; i++) {
		if (RevPiDevice_getDev(i)->i8uAddress == res_cnt.i8uAddress &&
		    RevPiDevice_getDev(i)->i8uActive &&
		    (RevPiDevice_getDev(i)->sId.i16uModulType == KUNBUS_FW_DESCR_TYP_PI_MIO ||
		    RevPiDevice_getDev(i)->sId.i16uModulType == KUNBUS_FW_DESCR_TYP_PI_DIO_14 ||
		    RevPiDevice_getDev(i)->sId.i16uModulType == KUNBUS_FW_DESCR_TYP_PI_DI_16)) {
			found = true;
			break;
		}
	}

	if (!found || res_cnt.i16uBitfield == 0) {
		pr_err("%s: resetCounter failed, bitfield: 0x%x\n",
			__func__,
			res_cnt.i16uBitfield);
		return -EINVAL;
	}

	tel.uHeader.sHeaderTyp1.bitAddress = res_cnt.i8uAddress;
	tel.uHeader.sHeaderTyp1.bitIoHeaderType = 0;
	tel.uHeader.sHeaderTyp1.bitReqResp = 0;
	tel.uHeader.sHeaderTyp1.bitLength = sizeof(SDioCounterReset) - IOPROTOCOL_HEADER_LENGTH - 1;
	tel.uHeader.sHeaderTyp1.bitCommand = IOP_TYP1_CMD_DATA3;
	tel.i16uChannels = res_cnt.i16uBitfield;

	return send_internal_io_telegram(&tel, sizeof(tel), NULL);
}

static int get_ro_counter(unsigned long usr_addr)
{
	struct revpi_ro_ioctl_counters __user *ioctl_get_counters;
	UIoProtocolHeader hdr;
	SIOGeneric resp;
	SDevice *sdev;
	u8 addr;
	int ret;
	int i;

	if (!piDev_g.pibridge_supported)
		return -EOPNOTSUPP;

	if (!isRunning())
		return -EIO;

	ioctl_get_counters = (struct revpi_ro_ioctl_counters __user *) usr_addr;

	/* copy device address */
	if (copy_from_user(&addr, &ioctl_get_counters->addr,
			   sizeof(addr))) {
		pr_err("failed to copy device address from user\n");
		return -EFAULT;
	}

	for (i = 0; i < RevPiDevice_getDevCnt(); i++) {
		sdev = RevPiDevice_getDev(i);

		if ((sdev->i8uAddress == addr) &&
		    sdev->i8uActive &&
		    (sdev->sId.i16uModulType == KUNBUS_FW_DESCR_TYP_PI_RO))
			break;
	}

	if (i == RevPiDevice_getDevCnt()) {
		pr_err("Getting relay counters failed: device 0x%02x not found\n",
		       addr);
		return -EINVAL;
	}

	hdr.sHeaderTyp1.bitAddress = addr;
	hdr.sHeaderTyp1.bitIoHeaderType = 0;
	hdr.sHeaderTyp1.bitReqResp = 0;
	hdr.sHeaderTyp1.bitLength = 0;
	hdr.sHeaderTyp1.bitCommand = IOP_TYP1_CMD_DATA2;

	ret = send_internal_io_telegram(&hdr, sizeof(hdr), &resp);
	if (!ret) {
		if (copy_to_user(ioctl_get_counters->counter,
				 resp.ai8uData,
				 sizeof(ioctl_get_counters->counter)))
			ret = -EFAULT;
	} else {
		pr_err("Getting relay counters failed: %d\n", ret);
	}

	return ret;
}

static int calibrate_aio(unsigned long usr_addr)
{
	struct pictl_calibrate cali;
	SMioCalibrationRequest req;
	bool found = false;
	int ret;
	int i;

	if (!piDev_g.pibridge_supported)
		return -EPERM;

	if (!isRunning())
		return -EFAULT;

	if (copy_from_user(&cali, (const void __user *) usr_addr,
				sizeof(cali))) {
		pr_err("failed to copy calibrate request from user\n");
		return -EFAULT;
	}

	for (i = 0; i < RevPiDevice_getDevCnt() && !found; i++) {
		if (RevPiDevice_getDev(i)->i8uAddress == cali.address
			&& RevPiDevice_getDev(i)->i8uActive
			&& RevPiDevice_getDev(i)->sId.i16uModulType
				== KUNBUS_FW_DESCR_TYP_PI_MIO) {
			found = true;
			break;
		}
	}

	if (!found || cali.channels == 0) {
		pr_err("calibrate failed, channel 0x%x\n",
					cali.channels);
		return -EINVAL;
	}

	revpi_io_build_header(&req.uHeader, cali.address,
			      sizeof(SMioCalibrationRequestData),
			      IOP_TYP1_CMD_DATA6);
	req.sData.i8uCalibrationMode = cali.mode;
	req.sData.i8uChannels = cali.channels;
	req.sData.i8uPoint = cali.x_val;
	req.sData.i16sCalibrationValue = cali.y_val;
	/*the crc calculation will be done by Tel sending*/
	ret = send_internal_io_telegram(&req, sizeof(req), NULL);

	pr_info("MIO calibrate header:0x%x, data:0x%x, status %d\n",
		*(unsigned short *)&req.uHeader,
		*(unsigned short *)&req.sData,
		ret);

	return ret;
}

static int send_internal_io_msg(unsigned long usr_addr)
{
	SIOGeneric resp;
	SIOGeneric req;
	int ret;

	if (!piDev_g.pibridge_supported)
		return -EPERM;

	if (!isRunning())
		return -EFAULT;

	if (copy_from_user(&req, (const void __user *) usr_addr, sizeof(req)))
		return -EFAULT;

	ret = send_internal_io_telegram(&req, sizeof(req), &resp);
	if (!ret) {
		if (copy_to_user((void __user *) usr_addr, &resp, sizeof(resp)))
			ret = -EFAULT;
	}

	return ret;
}

/*****************************************************************************/
/*    I O C T L                                                           */
/*****************************************************************************/
static long piControlIoctl(struct file *file, unsigned int prg_nr, unsigned long usr_addr)
{
	int status = -EFAULT;
	tpiControlInst *priv;
	int timeout = 10000;	// ms

	if (prg_nr != KB_RESET && prg_nr != KB_CONFIG_SEND
		&& prg_nr != KB_CONFIG_START && !isRunning()) {
		return -EAGAIN;
	}

	priv = (tpiControlInst *) file->private_data;

	if (prg_nr != KB_GET_LAST_MESSAGE) {
		// clear old message
		priv->pcErrorMessage[0] = 0;
	}

	switch (prg_nr) {
	case KB_RESET:
		rt_mutex_lock(&piDev_g.lockIoctl);
		pr_info("driver reset requested\n");
		pr_debug("BridgeState=%d\n", piCore_g.eBridgeState);

		if (piDev_g.pibridge_supported && isRunning()) {
			PiBridgeMaster_Stop();
		}
		status = piControlReset(priv);
		rt_mutex_unlock(&piDev_g.lockIoctl);
		break;

	case KB_GET_DEVICE_INFO:
		{
			SDeviceInfo dev_info;
			int ret;

			if (copy_from_user(&dev_info, (const void __user *) usr_addr, sizeof(dev_info))) {
				pr_err("failed to copy dev info from user\n");
				return -EFAULT;
			}

			ret = picontrol_get_device_info(&dev_info);
			if (ret < 0)
				return ret;

			status = ret;
			if (copy_to_user((void * __user) usr_addr, &dev_info, sizeof(dev_info))) {
				pr_err("failed to copy dev info to user\n");
				return -EFAULT;
			}
		}
		break;

	case KB_GET_DEVICE_INFO_LIST:
		{
			SDeviceInfo *dev_infos;
			unsigned int num_devs = RevPiDevice_getDevCnt();
			bool firmware_update = false;
			int i;

			dev_infos = kcalloc(num_devs, sizeof(SDeviceInfo),
					    GFP_KERNEL);
			if (!dev_infos)
				return -ENOMEM;

			for (i = 0; i < num_devs; i++) {
				picontrol_set_device_info(&dev_infos[i], RevPiDevice_getDev(i));

				if ((dev_infos[i].i16uModuleType == KUNBUS_FW_DESCR_TYP_PI_DIO_14) ||
				    (dev_infos[i].i16uModuleType == KUNBUS_FW_DESCR_TYP_PI_DO_16) ||
				    (dev_infos[i].i16uModuleType == KUNBUS_FW_DESCR_TYP_PI_DI_16)) {
					/*
					 * DIO with firmware older than 1.4
					 * should be updated
					 */
					if ((dev_infos[i].i16uSW_Major < 1) ||
					    ((dev_infos[i].i16uSW_Major == 1) &&
					     (dev_infos[i].i16uSW_Minor < 4))) {
						firmware_update = true;
					}
				}
				if (dev_infos[i].i16uModuleType == KUNBUS_FW_DESCR_TYP_PI_AIO) {
					/*
					 * AIO with firmware older than 1.3
					 * should be updated
					 */
					if ((dev_infos[i].i16uSW_Major < 1) ||
					    ((dev_infos[i].i16uSW_Major == 1) &&
					     (dev_infos[i].i16uSW_Minor < 3))) {
						firmware_update = true;
					}
				}
			}
			if (firmware_update) {
				printUserMsg(priv, "The firmware of some I/O modules must be updated.\n"
					     "Please connect only one module to the RevPi and call 'piTest -f'");
			}
			if (copy_to_user((void __user *) usr_addr, dev_infos,
					 sizeof(SDeviceInfo) * num_devs)) {
				pr_err("failed to copy device list to user\n");
				kfree(dev_infos);
				return -EFAULT;
			}
			kfree(dev_infos);
			status = RevPiDevice_getDevCnt();
		}
		break;

	case KB_GET_VALUE:
		{
			SPIValue spi_val;
			u8 val;

			if (!isRunning())
				return -EFAULT;

			if (copy_from_user(&spi_val, (const void __user *) usr_addr,
					     sizeof(spi_val))) {
				pr_err("failed to copy spi value from user\n");
				return -EFAULT;
			}

			if (spi_val.i16uAddress >= KB_PI_LEN) {
				status = -EFAULT;
			} else {
				rt_mutex_lock(&piDev_g.lockPI);
				val = piDev_g.ai8uPI[spi_val.i16uAddress];
				rt_mutex_unlock(&piDev_g.lockPI);

				if (spi_val.i8uBit >= 8) {
					spi_val.i8uValue = val;
				} else {
					spi_val.i8uValue = (val & (1 << spi_val.i8uBit))
					    ? 1 : 0;
				}

				if (copy_to_user((void __user *) usr_addr, &spi_val,
						   sizeof(spi_val))) {
					pr_err("failed to copy spi value to user\n");
					return -EFAULT;
				}

				status = 0;
			}
		}
		break;

	case KB_SET_VALUE:
		{
			SPIValue spi_val;

			if (!isRunning())
				return -EFAULT;

			if (copy_from_user(&spi_val, (const void __user *) usr_addr,
					   sizeof(spi_val))) {
				pr_err("failed to copy spi value from user\n");
				return -EFAULT;
			}

			if (spi_val.i16uAddress >= KB_PI_LEN) {
				status = -EFAULT;
			} else {
				INT8U i8uValue_l;
				my_rt_mutex_lock(&piDev_g.lockPI);
				i8uValue_l = piDev_g.ai8uPI[spi_val.i16uAddress];

				if (spi_val.i8uBit >= 8) {
					i8uValue_l = spi_val.i8uValue;
				} else {
					if (spi_val.i8uValue)
						i8uValue_l |= (1 << spi_val.i8uBit);
					else
						i8uValue_l &= ~(1 << spi_val.i8uBit);
				}

				piDev_g.ai8uPI[spi_val.i16uAddress] = i8uValue_l;
				rt_mutex_unlock(&piDev_g.lockPI);

				if (priv->tTimeoutDurationMs > 0) {
					priv->tTimeoutTS = ktime_add_ms(ktime_get(), priv->tTimeoutDurationMs);
				}

				status = 0;
			}
		}
		break;

	case KB_FIND_VARIABLE:
		{
			int i;
			SPIVariable spi_var;
			int namelen;
			const char __user *usr_name;

			if (!isRunning())
				return -EFAULT;

			if (!piDev_g.ent) {
				status = -ENOENT;
				break;
			}

			usr_name = ((SPIVariable *) usr_addr)->strVarName;

			namelen = strncpy_from_user(spi_var.strVarName, usr_name,
						    sizeof(spi_var.strVarName) - 1);
			if (namelen < 0) {
				pr_err("failed to copy spi variable from user\n");
				return -EFAULT;
			}
			/* make sure we have a valid string */
			spi_var.strVarName[namelen] = '\0';
			/* set default */
			spi_var.i16uAddress = 0xffff;
			spi_var.i8uBit = 0xff;
			spi_var.i16uLength = 0xffff;

			for (i = 0; i < piDev_g.ent->i16uNumEntries; i++) {
				if (strcmp(piDev_g.ent->ent[i].strVarName, spi_var.strVarName) == 0) {
					spi_var.i16uAddress = piDev_g.ent->ent[i].i16uOffset;
					spi_var.i8uBit = piDev_g.ent->ent[i].i8uBitPos;
					spi_var.i16uLength = piDev_g.ent->ent[i].i16uBitLength;
					status = 0;
					break;
				}
			}

			if (copy_to_user((void __user *) usr_addr, &spi_var, sizeof(spi_var))) {
				pr_err("failed to copy spi variable to user\n");
				return -EFAULT;
			}

		}
		break;

	case KB_SET_EXPORTED_OUTPUTS:
		{
			int i;
			ktime_t now;

			if (!isRunning())
				return -EFAULT;

			if (usr_addr == 0) {
				pr_err("piControlIoctl: illegal parameter\n");
				return -EINVAL;
			}

			if (piDev_g.cl == 0 || piDev_g.cl->i16uNumEntries == 0)
				return 0;	// nothing to do

			status = 0;
			now = ktime_get();

			my_rt_mutex_lock(&piDev_g.lockPI);
			piDev_g.tLastOutput2 = piDev_g.tLastOutput1;
			piDev_g.tLastOutput1 = now;

			for (i = 0; i < piDev_g.cl->i16uNumEntries; i++) {
				uint16_t len = piDev_g.cl->ent[i].i16uLength;
				uint16_t addr = piDev_g.cl->ent[i].i16uAddr;

				if (len >= 8) {
					len /= 8;
					if (copy_from_user(piDev_g.ai8uPI + addr, (void *)(usr_addr + addr), len) != 0) {
						status = -EFAULT;
						break;
					}
				} else {
					uint8_t val1, val2;
					uint8_t mask = piDev_g.cl->ent[i].i8uBitMask;

					if (get_user(val1, (u8 __user*) (usr_addr + addr))) {
						pr_err("failed to copy byte from user\n");
						status = -EFAULT;
						break;
					}

					val1 &= mask;

					val2 = piDev_g.ai8uPI[addr];
					val2 &= ~mask;
					val2 |= val1;
					piDev_g.ai8uPI[addr] = val2;
				}
			}
			rt_mutex_unlock(&piDev_g.lockPI);

			if (priv->tTimeoutDurationMs > 0) {
				priv->tTimeoutTS = ktime_add_ms(ktime_get(), priv->tTimeoutDurationMs);
			}
		}
		break;

	case KB_DIO_RESET_COUNTER:
		my_rt_mutex_lock(&piDev_g.lockIoctl);
		status = reset_dio_counter(usr_addr);
		rt_mutex_unlock(&piDev_g.lockIoctl);
		pr_debug("%s: resetCounter result %d\n", __func__, status);
		break;
	case KB_RO_GET_COUNTER:
		my_rt_mutex_lock(&piDev_g.lockIoctl);
		status = get_ro_counter(usr_addr);
		rt_mutex_unlock(&piDev_g.lockIoctl);
		break;
	case KB_AIO_CALIBRATE:
		my_rt_mutex_lock(&piDev_g.lockIoctl);
		status = calibrate_aio(usr_addr);
		rt_mutex_unlock(&piDev_g.lockIoctl);
		break;
	case KB_INTERN_SET_SERIAL_NUM:
		{
			u32 snum_data[2]; 	// snum_data is an array containing the module address and the serial number

			if (!piDev_g.pibridge_supported) {
				return -EPERM;
			}

			if (copy_from_user(snum_data, (const void __user *) usr_addr,
					   sizeof(snum_data))) {
				pr_err("failed to copy serial num from user\n");
				return -EFAULT;
			}

			rt_mutex_lock(&piDev_g.lockIoctl);
			if (!isRunning()) {
				rt_mutex_unlock(&piDev_g.lockIoctl);
				return -EFAULT;
			}

			PiBridgeMaster_Stop();
			msleep(500);

			if (PiBridgeMaster_FWUModeEnter(snum_data[0], 1) == 0) {

				if (PiBridgeMaster_FWUsetSerNum(snum_data[1]) == 0) {
					pr_info("piControlIoctl: set serial number to %u in module %u", snum_data[1], snum_data[0]);
				}

				PiBridgeMaster_FWUReset();
				msleep(1000);
			}

			PiBridgeMaster_Reset();
			msleep(500);
			PiBridgeMaster_Reset();

			if (!waitRunning(timeout)) {
				status = -ETIMEDOUT;
			} else {
				status = 0;
			}
			rt_mutex_unlock(&piDev_g.lockIoctl);
		}
		break;

	case KB_UPDATE_DEVICE_FIRMWARE:
		{
			int i, ret, cnt;
			u32 data;
			INT32U *pData = NULL;	// pData is null or points to the module address


			pr_notice("Note: ioctl KB_UPDATE_DEVICE_FIRMWARE is deprecated. Use PICONTROL_UPLOAD_FIRMWARE instead\n");

			if (!piDev_g.pibridge_supported) {
				return -EPERM;
			}

			if (usr_addr) {
				if (get_user(data, (u32 __user *) usr_addr)) {
					pr_err("failed to copy update info from user\n");
					return -EFAULT;
				}

				pData = &data;
			}

			rt_mutex_lock(&piDev_g.lockIoctl);
			if (!isRunning()) {
				printUserMsg(priv, "piControl is not running");
				rt_mutex_unlock(&piDev_g.lockIoctl);
				return -EFAULT;
			}

			PiBridgeMaster_Stop();
			msleep(50);

			cnt = 0;
			for (i = 0; i < RevPiDevice_getDevCnt(); i++) {
				if (pData != 0 && RevPiDevice_getDev(i)->i8uAddress != *pData) {
					// if pData is not 0, we want to update one specific module
					// -> update all others
					pr_info("skip %d addr %d\n", i, RevPiDevice_getDev(i)->i8uAddress);
				} else {
					ret = FWU_update(priv, RevPiDevice_getDev(i));
					pr_info("update %d addr %d ret %d\n", i, RevPiDevice_getDev(i)->i8uAddress, ret);
					if (ret > 0) {
						cnt++;
						// update only one device per call
						break;
					} else if (ret == -EOPNOTSUPP) {
						status = 0;
					} else if (ret < 0) {
						/* firmware update failed, return error code to user space  */
						status = ret;
						break;
					}
				}
			}

			if (cnt) {
				// at least one module was updated, make a reset
				status = piControlReset(priv);
			} else {
				PiBridgeMaster_Continue();
			}
			rt_mutex_unlock(&piDev_g.lockIoctl);
		}
		break;

	case PICONTROL_UPLOAD_FIRMWARE:
		{
			struct picontrol_firmware_upload fwu;

			if (!piDev_g.pibridge_supported)
				return -EOPNOTSUPP;

			if (copy_from_user(&fwu, (const void __user *) usr_addr,
					   sizeof(fwu))) {
				pr_err("failed to copy firmware upload request from user\n");
				return -EFAULT;
			}

			if (!fwu.addr) {
				pr_err("Module with address 0 has no firmware to update");
				return -EOPNOTSUPP;
			}

			rt_mutex_lock(&piDev_g.lockIoctl);
			status = picontrol_upload_firmware(&fwu, priv);
			rt_mutex_unlock(&piDev_g.lockIoctl);
		}
		break;

	case KB_INTERN_IO_MSG:
		my_rt_mutex_lock(&piDev_g.lockIoctl);
		status = send_internal_io_msg(usr_addr);
		rt_mutex_unlock(&piDev_g.lockIoctl);
		break;

	case KB_INTERN_GATE_MSG:
		my_rt_mutex_lock(&piDev_g.lockIoctl);
		status = send_internal_gate_msg(usr_addr);
		rt_mutex_unlock(&piDev_g.lockIoctl);
		break;

	case KB_WAIT_FOR_EVENT:
		{
			tpiEventEntry *pEntry;

			if (wait_event_interruptible(priv->wq, !list_empty(&priv->piEventList)) == 0) {
				my_rt_mutex_lock(&priv->lockEventList);
				pEntry = list_first_entry(&priv->piEventList, tpiEventEntry, list);

				list_del(&pEntry->list);
				rt_mutex_unlock(&priv->lockEventList);

				if (put_user(pEntry->event, (u32 __user *) usr_addr)) {
					status = -EFAULT;
				} else {
					status = 0;
				}

				kfree(pEntry);
			}
		}
		break;

	case KB_GET_LAST_MESSAGE:
		{
			if (copy_to_user((void *)usr_addr, priv->pcErrorMessage, sizeof(priv->pcErrorMessage))) {
				pr_err("piControlIoctl: copy_to_user failed");
				return -EFAULT;
			}
			status = 0;
		}
		break;

	case KB_STOP_IO:
		{
			// parameter = 0: start I/Os
			// parameter = 1: stop I/Os
			// parameter = 2: toggle I/Os
			u32 data;

			if (!isRunning())
				return -EFAULT;

			if (get_user(data, (u32 __user *) usr_addr)) {
				pr_err("Failed to copy stop command from user\n");
				return -EFAULT;
			}

			if (data == 2) {
				change_bit(PICONTROL_DEV_FLAG_STOP_IO,
					   &piDev_g.flags);
			} else {
				if (data)
					set_bit(PICONTROL_DEV_FLAG_STOP_IO,
						&piDev_g.flags);
				else
					clear_bit(PICONTROL_DEV_FLAG_STOP_IO,
						  &piDev_g.flags);
			}
			status = test_bit(PICONTROL_DEV_FLAG_STOP_IO,
					  &piDev_g.flags) ? 1 : 0;
		}
		break;

	case KB_CONFIG_STOP:	// for download of configuration to Master Gateway: stop IO communication completely
		{
			if (!piDev_g.revpi_gate_supported) {
				return -EPERM;
			}

			rt_mutex_lock(&piDev_g.lockIoctl);
			if (!isRunning()) {
				printUserMsg(priv, "piControl is not running");
				rt_mutex_unlock(&piDev_g.lockIoctl);
				return -EFAULT;
			}
			PiBridgeMaster_Stop();
			rt_mutex_unlock(&piDev_g.lockIoctl);
			msleep(50);
			status = 0;
		}
		break;

	case KB_CONFIG_SEND:	// for download of configuration to Master Gateway: download config data
		my_rt_mutex_lock(&piDev_g.lockIoctl);
		status = send_config(usr_addr);
		rt_mutex_unlock(&piDev_g.lockIoctl);
		break;

	case KB_CONFIG_START:	// for download of configuration to Master Gateway: restart IO communication
		{
			if (!piDev_g.revpi_gate_supported) {
				return -EPERM;
			}
			if (isRunning()) {
				return -ECANCELED;
			}

			PiBridgeMaster_Continue();
			status = 0;
		}
		break;

	case KB_SET_OUTPUT_WATCHDOG:
		{
			if (get_user(priv->tTimeoutDurationMs,
				     (unsigned long __user *) usr_addr)) {
				pr_err("failed to copy timeout from user\n");
				return -EFAULT;
			}

			priv->tTimeoutTS = ktime_add_ms(ktime_get(), priv->tTimeoutDurationMs);
			status = 0;
		}
		break;

	case KB_SET_POS:
		{
			loff_t off = 0;

			if (usr_addr != 0) {
				if (get_user(off, (unsigned int __user *) usr_addr)) {
					pr_err("failed to copy offset from user\n");
					return -EFAULT;
				}
			}

			status = piControlSeek(file, off, 0);
			if (status > 0)
				status = 0;
		}
		break;

	default:
		pr_debug("Invalid Ioctl from %s", current->comm);
		return (-EINVAL);
		break;

	}

	return status;
}

static const struct of_device_id of_pibridge_match[] = {
	{ .compatible = "kunbus,pibridge", },
	{},
};

static struct platform_driver pibridge_driver = {
	.probe		= pibridge_probe,
	.remove		= pibridge_remove,
	.driver		= {
		.name	= "pibridge",
		.of_match_table = of_pibridge_match,
	},
};

static int __init piControlInit(void)
{
	int ret;

	wait_for_device_probe();
	ret = platform_driver_register(&pibridge_driver);
	if (ret)
		pr_err("failed to register pibridge driver: %i\n", ret);

	return ret;
}


static void __exit piControlCleanup(void)
{
	platform_driver_unregister(&pibridge_driver);
}

module_init(piControlInit);
module_exit(piControlCleanup);

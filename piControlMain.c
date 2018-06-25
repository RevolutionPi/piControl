/*=======================================================================================
 *
 *	       KK    KK   UU    UU   NN    NN   BBBBBB    UU    UU    SSSSSS
 *	       KK   KK    UU    UU   NNN   NN   BB   BB   UU    UU   SS
 *	       KK  KK     UU    UU   NNNN  NN   BB   BB   UU    UU   SS
 *	+----- KKKKK      UU    UU   NN NN NN   BBBBB     UU    UU    SSSSS
 *	|      KK  KK     UU    UU   NN  NNNN   BB   BB   UU    UU        SS
 *	|      KK   KK    UU    UU   NN   NNN   BB   BB   UU    UU        SS
 *	|      KK    KKK   UUUUUU    NN    NN   BBBBBB     UUUUUU    SSSSSS     GmbH
 *	|
 *	|            [#]  I N D U S T R I A L   C O M M U N I C A T I O N
 *	|             |
 *	+-------------+
 *
 *---------------------------------------------------------------------------------------
 *
 * (C) KUNBUS GmbH, Heerweg 15C, 73770 Denkendorf, Germany
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License V2 as published by
 * the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *  For licencing details see COPYING
 *
 *=======================================================================================
 */

/******************************************************************************/
/********************************  Includes  **********************************/
/******************************************************************************/
//#define DEBUG

#include <common_define.h>
#include <project.h>

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
#include <linux/spi/spi.h>
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
#include <asm/div64.h>
#include <linux/syscalls.h>

#include "revpi_common.h"
#include "revpi_core.h"
#include "revpi_compact.h"

#include "piFirmwareUpdate.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Christof Vogt, Mathias Duckeck, Lukas Wunner");
MODULE_DESCRIPTION("piControl Driver");
MODULE_VERSION("1.4.0");
MODULE_SOFTDEP("pre: bcm2835-thermal "	/* cpu temp in process image */
	       "spi-bcm2835 "		/* core spi0 gateways */
	       "spi-bcm2835aux "	/* compact spi2 i/o */
	       "gpio-74x164 "		/* compact dout */
	       "fixed "			/* compact ain/aout vref */
	       "mux_gpio "		/* compact ain mux */
	       "iio_mux "		/* compact ain mux */
	       "mcp320x "		/* compact ain */
	       "ti-dac082s085");	/* compact aout */

/******************************************************************************/
/******************************  Prototypes  **********************************/
/******************************************************************************/

static int piControlOpen(struct inode *inode, struct file *file);
static int piControlRelease(struct inode *inode, struct file *file);
static ssize_t piControlRead(struct file *file, char __user * pBuf, size_t count, loff_t * ppos);
static ssize_t piControlWrite(struct file *file, const char __user * pBuf, size_t count, loff_t * ppos);
static loff_t piControlSeek(struct file *file, loff_t off, int whence);
static long piControlIoctl(struct file *file, unsigned int prg_nr, unsigned long usr_addr);
static void cleanup(void);
static void __exit piControlCleanup(void);

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
	.a2_green.name = "a2_green",
	.a2_red.name = "a2_red",
	.a3_green.name = "a3_green",
	.a3_red.name = "a3_red",
};

static dev_t piControlMajor;
static struct class *piControlClass;

/******************************************************************************/
/*******************************  Functions  **********************************/
/******************************************************************************/

bool waitRunning(int timeout);	// ms

/******************************************************************************/
/***************************** CS Functions  **********************************/
/******************************************************************************/

static void showPADS(void)
{
	void __iomem *base;

	base = devm_ioremap(piDev_g.dev, 0x3f10002c, 12);
	if (IS_ERR(base)) {
		pr_err("cannot map PADS");
	} else {
		u32 v;
		int i;

		for (i=0; i<3; i++)
		{
			v = readl(base + 4*i);
			pr_info("PADS %d = %#x   slew=%d  hyst=%d  drive=%d",
			       i,
			       v,
			       (v & 0x10) ? 1 : 0,
			       (v & 0x08) ? 1 : 0,
			       (v & 0x07));
		}
		devm_iounmap(piDev_g.dev, base);
	}
}

/*****************************************************************************/
/*       I N I T                                                             */
/*****************************************************************************/
#ifdef UART_TEST
void piControlDummyReceive(INT8U i8uChar_p)
{
	pr_info("Got character %c\n", i8uChar_p);
}
#endif

#include "compiletime.h"

static char *piControlClass_devnode(struct device *dev, umode_t * mode)
{
	if (mode)
		*mode = S_IRUGO | S_IWUGO;

	return NULL;
}

static int __init piControlInit(void)
{
	int devindex = 0;
	dev_t curdev;
	int res;

	pr_info("built: %s\n", COMPILETIME);

	if (of_machine_is_compatible("kunbus,revpi-compact")) {
		piDev_g.machine_type = REVPI_COMPACT;
		pr_info("RevPi Compact\n");
	} else if (of_machine_is_compatible("kunbus,revpi-connect")) {
		piDev_g.machine_type = REVPI_CONNECT;
		pr_info("RevPi Connect\n");
	} else {
		piDev_g.machine_type = REVPI_CORE;
		pr_info("RevPi Core\n");
	}

	// alloc_chrdev_region return 0 on success
	res = alloc_chrdev_region(&piControlMajor, 0, 2, "piControl");

	if (res) {
		pr_info("ERROR: PN Driver not registered \n");
		cleanup();
		return res;
	} else {
		pr_info("MAJOR-No.  : %d\n", MAJOR(piControlMajor));
	}
	piDev_g.init_step = 1;

	piControlClass = class_create(THIS_MODULE, "piControl");
	if (IS_ERR(piControlClass)) {
		pr_err("cannot create class\n");
		cleanup();
		return PTR_ERR(piControlClass);
	}
	piControlClass->devnode = piControlClass_devnode;

	/* create device node */
	curdev = MKDEV(MAJOR(piControlMajor), MINOR(piControlMajor) + devindex);

	piDev_g.PnAppCon = 0;
	INIT_LIST_HEAD(&piDev_g.listCon);
	rt_mutex_init(&piDev_g.lockListCon);

	cdev_init(&piDev_g.cdev, &piControlFops);
	piDev_g.cdev.owner = THIS_MODULE;
	piDev_g.cdev.ops = &piControlFops;

	piDev_g.dev = device_create(piControlClass, NULL, curdev, NULL, "piControl%d", devindex);
	if (IS_ERR(piDev_g.dev)) {
		pr_err("cannot create device\n");
		cleanup();
		return PTR_ERR(piDev_g.dev);
	}

	if (cdev_add(&piDev_g.cdev, curdev, 1)) {
		pr_info("Add Cdev failed\n");
		cleanup();
		return -EFAULT;
	} else {
		pr_info("MAJOR-No.  : %d  MINOR-No.  : %d\n", MAJOR(curdev), MINOR(curdev));
	}
	piDev_g.init_step = 2;

	res = devm_led_trigger_register(piDev_g.dev, &piDev_g.power_red)
	   || devm_led_trigger_register(piDev_g.dev, &piDev_g.a1_green)
	   || devm_led_trigger_register(piDev_g.dev, &piDev_g.a1_red)
	   || devm_led_trigger_register(piDev_g.dev, &piDev_g.a2_green)
	   || devm_led_trigger_register(piDev_g.dev, &piDev_g.a2_red);

	if (piDev_g.machine_type == REVPI_CONNECT) {
		res = res
		   || devm_led_trigger_register(piDev_g.dev, &piDev_g.a3_green)
		   || devm_led_trigger_register(piDev_g.dev, &piDev_g.a3_red);
	}

	if (res) {
		pr_err("cannot register LED triggers\n");
		cleanup();
		return res;
	}
	piDev_g.init_step = 7;

	/* init some data */
	rt_mutex_init(&piDev_g.lockPI);
	piDev_g.stopIO = false;

	piDev_g.tLastOutput1 = ktime_set(0, 0);
	piDev_g.tLastOutput2 = ktime_set(0, 0);

	/* start application */
	if (piConfigParse(PICONFIG_FILE, &piDev_g.devs, &piDev_g.ent, &piDev_g.cl, &piDev_g.connl) == 2) {
		// file not found, try old location
		piConfigParse(PICONFIG_FILE_WHEEZY, &piDev_g.devs, &piDev_g.ent, &piDev_g.cl, &piDev_g.connl);
		// ignore errors
	}
	piDev_g.init_step = 8;

	if (piDev_g.machine_type == REVPI_CORE) {
		res = revpi_core_init();
	} else if (piDev_g.machine_type == REVPI_CONNECT) {
		res = revpi_core_init();
	} else if (piDev_g.machine_type == REVPI_COMPACT) {
		res = revpi_compact_init();
	}
	if (res) {
		cleanup();
		return res;
	}
	showPADS();

	piDev_g.init_step = 17;

	piDev_g.thermal_zone = thermal_zone_get_zone_by_name("bcm2835_thermal");
	if (IS_ERR(piDev_g.thermal_zone)) {
		pr_err("could not find thermal zone 'bcm2835_thermal'\n");
		piDev_g.thermal_zone = NULL;
	}

	pr_info("piControlInit done\n");
	return 0;
}

/*****************************************************************************/
/*       C L E A N U P                                                       */
/*****************************************************************************/
static void cleanup(void)
{
	if (piDev_g.machine_type == REVPI_CORE) {
		revpi_core_fini();
	} else if (piDev_g.machine_type == REVPI_CONNECT) {
		revpi_core_fini();
	} else if (piDev_g.machine_type == REVPI_COMPACT) {
		revpi_compact_fini();
	}

	if (piDev_g.init_step >= 8) {
		if (piDev_g.ent != NULL)
			kfree(piDev_g.ent);
		piDev_g.ent = NULL;

		if (piDev_g.devs != NULL)
			kfree(piDev_g.devs);
		piDev_g.devs = NULL;
	}
	if (piDev_g.init_step >= 2) {
		cdev_del(&piDev_g.cdev);
	}

	if (!IS_ERR_OR_NULL(piDev_g.dev)) {
		int devindex = 0;
		dev_t curdev;

		curdev = MKDEV(MAJOR(piControlMajor), MINOR(piControlMajor) + devindex);
		pr_info("Remove MINOR-No.  : %d\n", MINOR(curdev));
		device_destroy(piControlClass, curdev);
		piDev_g.dev = NULL;
	}

	if (!IS_ERR_OR_NULL(piControlClass)) {
		class_destroy(piControlClass);
		piControlClass = NULL;
	}
	unregister_chrdev_region(piControlMajor, 2);
}

/*****************************************************************************/
static int piControlReset(tpiControlInst * priv)
{
	int status = -EFAULT;
	void *vptr;
	int timeout = 10000;	// ms

	if (piDev_g.ent != NULL) {
		vptr = piDev_g.ent;
		piDev_g.ent = NULL;
		kfree(vptr);
	}
	if (piDev_g.devs != NULL) {
		vptr = piDev_g.devs;
		piDev_g.devs = NULL;
		kfree(vptr);
	}
	if (piDev_g.cl != NULL) {
		vptr = piDev_g.cl;
		piDev_g.cl = NULL;
		kfree(vptr);
	}

	/* start application */
	if (piConfigParse(PICONFIG_FILE, &piDev_g.devs, &piDev_g.ent, &piDev_g.cl, &piDev_g.connl) == 2) {
		// file not found, try old location
		piConfigParse(PICONFIG_FILE_WHEEZY, &piDev_g.devs, &piDev_g.ent, &piDev_g.cl, &piDev_g.connl);
		// ignore errors
	}

	if (piDev_g.machine_type == REVPI_CORE) {
		PiBridgeMaster_Reset();
	} else if (piDev_g.machine_type == REVPI_CONNECT) {
		PiBridgeMaster_Reset();
	} else if (piDev_g.machine_type == REVPI_COMPACT) {
		int ret = revpi_compact_reset();
		if (ret) {
			cleanup();
			return ret;
		}
	}
	showPADS();

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
					pr_info_drv("reset(%d): add tail %d %x", priv->instNum, pos_inst->instNum, (unsigned int)pEntry);
					list_add_tail(&pEntry->list, &pos_inst->piEventList);
					rt_mutex_unlock(&pos_inst->lockEventList);
					pr_info_drv("reset(%d): inform instance %d\n", priv->instNum, pos_inst->instNum);
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

static void __exit piControlCleanup(void)
{
	pr_info_drv("piControlCleanup\n");

	cleanup();

	pr_info_drv("driver stopped with MAJOR-No. %d\n\n ", MAJOR(piControlMajor));
	pr_info_drv("piControlCleanup done\n");
}

// true: system is initialized
// false: system is not operational
bool isInitialized(void)
{
	if (piDev_g.init_step < 17) {
		return false;
	}
	return true;
}

// true: system is running
// false: system is not operational
bool isRunning(void)
{
	if (piDev_g.init_step < 17 || ((piDev_g.machine_type == REVPI_CORE || piDev_g.machine_type == REVPI_CONNECT)
				       && (piCore_g.eBridgeState != piBridgeRun))) {
		return false;
	}
	return true;
}

// true: system is running
// false: system is not operational
bool waitRunning(int timeout)	// ms
{
	if (piDev_g.init_step < 17) {
		pr_info_drv("waitRunning: init_step=%d\n", piDev_g.init_step);
		return false;
	}

	if (piDev_g.machine_type == REVPI_COMPACT)
		return true;

	timeout /= 100;
	timeout++;

	while (timeout > 0 && piCore_g.eBridgeState != piBridgeRun) {
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

	pr_info("%s", priv->pcErrorMessage);
}

/*****************************************************************************/
/*              O P E N                                                      */
/*****************************************************************************/
static int piControlOpen(struct inode *inode, struct file *file)
{
	tpiControlInst *priv;

	priv = (tpiControlInst *) kzalloc(sizeof(tpiControlInst), GFP_KERNEL);
	file->private_data = priv;

	if (priv == NULL) {
		pr_err("Private Allocation failed");
		return -EINVAL;
	}

	/* initalize instance variables */
	priv->dev = piDev_g.dev;
	INIT_LIST_HEAD(&priv->piEventList);
	rt_mutex_init(&priv->lockEventList);

	init_waitqueue_head(&priv->wq);

	//pr_info("piControlOpen");
	if (!waitRunning(3000)) {
		int status;
		pr_err("problem at driver initialization\n");
		status = piControlReset(priv);
		if (status) {
			kfree(priv);
			return status;
		}
		//return -EINVAL;
	}

	piDev_g.PnAppCon++;
	priv->instNum = piDev_g.PnAppCon;

	my_rt_mutex_lock(&piDev_g.lockListCon);
	list_add(&priv->list, &piDev_g.listCon);
	rt_mutex_unlock(&piDev_g.lockListCon);

	pr_info_drv("opened instance %d\n", piDev_g.PnAppCon);

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

	pr_info_drv("close instance %d/%d\n", priv->instNum, piDev_g.PnAppCon);
	piDev_g.PnAppCon--;

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

	dev_dbg(priv->dev, "piControlRead Count: %u, Pos: %llu", count, *ppos);

	if (*ppos < 0 || *ppos >= KB_PI_LEN) {
		return 0;	// end of file
	}

	if (nread + *ppos > KB_PI_LEN) {
		nread = KB_PI_LEN - *ppos;
	}

	pPd = piDev_g.ai8uPI + *ppos;

#ifdef VERBOSE
	pr_info("piControlRead inst %d Count=%u, Pos=%llu: %02x %02x\n", priv->instNum, count, *ppos, pPd[0], pPd[1]);
#endif

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

	dev_dbg(priv->dev, "piControlWrite Count: %u, Pos: %llu", count, *ppos);

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
#ifdef VERBOSE
	pr_info("piControlWrite Count=%u, Pos=%llu: %02x %02x\n", count, *ppos, pPd[0], pPd[1]);
#endif
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

/*****************************************************************************/
/*    I O C T L                                                           */
/*****************************************************************************/
static long piControlIoctl(struct file *file, unsigned int prg_nr, unsigned long usr_addr)
{
	int status = -EFAULT;
	tpiControlInst *priv;
	int timeout = 10000;	// ms

	if (prg_nr != KB_CONFIG_SEND && prg_nr != KB_CONFIG_START && !isRunning()) {
		return -EAGAIN;
	}

	priv = (tpiControlInst *) file->private_data;

	if (prg_nr != KB_GET_LAST_MESSAGE) {
		// clear old message
		priv->pcErrorMessage[0] = 0;
	}

	switch (prg_nr) {
	case KB_RESET:
		pr_info("Reset: BridgeState=%d \n", piCore_g.eBridgeState);

		if (!isInitialized()) {
			return -EFAULT;
		}
		if (isRunning()) {
			PiBridgeMaster_Stop();
		}
		status = piControlReset(priv);
		break;

	case KB_GET_DEVICE_INFO:
		{
			SDeviceInfo *pDev = (SDeviceInfo *) usr_addr;
			int i, found;
			for (i = 0, found = 0; i < RevPiDevice_getDevCnt(); i++) {
				if (pDev->i8uAddress != 0 && pDev->i8uAddress == RevPiDevice_getDev(i)->i8uAddress) {
					found = 1;
				}
				if (pDev->i16uModuleType != 0 && pDev->i16uModuleType == RevPiDevice_getDev(i)->sId.i16uModulType) {
					found = 1;
				}
				if (found) {
					pDev->i8uAddress = RevPiDevice_getDev(i)->i8uAddress;
					pDev->i8uActive = RevPiDevice_getDev(i)->i8uActive;
					pDev->i32uSerialnumber = RevPiDevice_getDev(i)->sId.i32uSerialnumber;
					pDev->i16uModuleType = RevPiDevice_getDev(i)->sId.i16uModulType;
					pDev->i16uHW_Revision = RevPiDevice_getDev(i)->sId.i16uHW_Revision;
					pDev->i16uSW_Major = RevPiDevice_getDev(i)->sId.i16uSW_Major;
					pDev->i16uSW_Minor = RevPiDevice_getDev(i)->sId.i16uSW_Minor;
					pDev->i32uSVN_Revision = RevPiDevice_getDev(i)->sId.i32uSVN_Revision;
					pDev->i16uInputLength = RevPiDevice_getDev(i)->sId.i16uFBS_InputLength;
					pDev->i16uInputOffset = RevPiDevice_getDev(i)->i16uInputOffset;
					pDev->i16uOutputLength = RevPiDevice_getDev(i)->sId.i16uFBS_OutputLength;
					pDev->i16uOutputOffset = RevPiDevice_getDev(i)->i16uOutputOffset;
					pDev->i16uConfigLength = RevPiDevice_getDev(i)->i16uConfigLength;
					pDev->i16uConfigOffset = RevPiDevice_getDev(i)->i16uConfigOffset;
					if (piCore_g.i8uRightMGateIdx == i)
						pDev->i8uModuleState = MODGATECOM_GetOtherFieldbusState(0);
					if (piCore_g.i8uLeftMGateIdx == i)
						pDev->i8uModuleState = MODGATECOM_GetOtherFieldbusState(1);
					status = i;
				}
			}
		}
		break;

	case KB_GET_DEVICE_INFO_LIST:
		{
			SDeviceInfo *pDev = (SDeviceInfo *) usr_addr;
			bool firmware_update = false;
			int i;

			for (i = 0; i < RevPiDevice_getDevCnt(); i++) {
				pDev[i].i8uAddress = RevPiDevice_getDev(i)->i8uAddress;
				pDev[i].i8uActive = RevPiDevice_getDev(i)->i8uActive;
				pDev[i].i32uSerialnumber = RevPiDevice_getDev(i)->sId.i32uSerialnumber;
				pDev[i].i16uModuleType = RevPiDevice_getDev(i)->sId.i16uModulType;
				pDev[i].i16uHW_Revision = RevPiDevice_getDev(i)->sId.i16uHW_Revision;
				pDev[i].i16uSW_Major = RevPiDevice_getDev(i)->sId.i16uSW_Major;
				pDev[i].i16uSW_Minor = RevPiDevice_getDev(i)->sId.i16uSW_Minor;
				pDev[i].i32uSVN_Revision = RevPiDevice_getDev(i)->sId.i32uSVN_Revision;
				pDev[i].i16uInputLength = RevPiDevice_getDev(i)->sId.i16uFBS_InputLength;
				pDev[i].i16uInputOffset = RevPiDevice_getDev(i)->i16uInputOffset;
				pDev[i].i16uOutputLength = RevPiDevice_getDev(i)->sId.i16uFBS_OutputLength;
				pDev[i].i16uOutputOffset = RevPiDevice_getDev(i)->i16uOutputOffset;
				pDev[i].i16uConfigLength = RevPiDevice_getDev(i)->i16uConfigLength;
				pDev[i].i16uConfigOffset = RevPiDevice_getDev(i)->i16uConfigOffset;
				if (piCore_g.i8uRightMGateIdx == i)
					pDev[i].i8uModuleState = MODGATECOM_GetOtherFieldbusState(0);
				if (piCore_g.i8uLeftMGateIdx == i)
					pDev[i].i8uModuleState = MODGATECOM_GetOtherFieldbusState(1);

				if (	pDev[i].i16uModuleType == KUNBUS_FW_DESCR_TYP_PI_DIO_14
				||	pDev[i].i16uModuleType == KUNBUS_FW_DESCR_TYP_PI_DO_16
				||	pDev[i].i16uModuleType == KUNBUS_FW_DESCR_TYP_PI_DI_16) {
					// DIO with firmware older than 1.4 should be updated
					if (	pDev[i].i16uSW_Major < 1
					    || (pDev[i].i16uSW_Major == 1 && pDev[i].i16uSW_Minor < 4)) {
						firmware_update = true;
					}
				}
				if (	pDev[i].i16uModuleType == KUNBUS_FW_DESCR_TYP_PI_AIO) {
					// AIO with firmware older than 1.3 should be updated
					if (	pDev[i].i16uSW_Major < 1
					    || (pDev[i].i16uSW_Major == 1 && pDev[i].i16uSW_Minor < 3)) {
						firmware_update = true;
					}
				}
			}
			if (firmware_update) {
				printUserMsg(priv, "The firmware of some I/O modules must be updated.\n"
					     "Please connect only one module to the RevPi and call 'piTest -f'");
			}
			status = RevPiDevice_getDevCnt();
		}
		break;

	case KB_GET_VALUE:
		{
			SPIValue *pValue = (SPIValue *) usr_addr;

			if (!isRunning())
				return -EFAULT;

			if (pValue->i16uAddress >= KB_PI_LEN) {
				status = -EFAULT;
			} else {
				INT8U i8uValue_l;
				// bei einem Byte braucht man keinen Lock rt_mutex_lock(&piDev_g.lockPI);
				i8uValue_l = piDev_g.ai8uPI[pValue->i16uAddress];
				// bei einem Byte braucht man keinen Lock rt_mutex_unlock(&piDev_g.lockPI);

				if (pValue->i8uBit >= 8) {
					pValue->i8uValue = i8uValue_l;
				} else {
					pValue->i8uValue = (i8uValue_l & (1 << pValue->i8uBit))
					    ? 1 : 0;
				}

				status = 0;
			}
		}
		break;

	case KB_SET_VALUE:
		{
			SPIValue *pValue = (SPIValue *) usr_addr;

			if (!isRunning())
				return -EFAULT;

			if (pValue->i16uAddress >= KB_PI_LEN) {
				status = -EFAULT;
			} else {
				INT8U i8uValue_l;
				my_rt_mutex_lock(&piDev_g.lockPI);
				i8uValue_l = piDev_g.ai8uPI[pValue->i16uAddress];

				if (pValue->i8uBit >= 8) {
					i8uValue_l = pValue->i8uValue;
				} else {
					if (pValue->i8uValue)
						i8uValue_l |= (1 << pValue->i8uBit);
					else
						i8uValue_l &= ~(1 << pValue->i8uBit);
				}

				piDev_g.ai8uPI[pValue->i16uAddress] = i8uValue_l;
				rt_mutex_unlock(&piDev_g.lockPI);

				if (priv->tTimeoutDurationMs > 0) {
					priv->tTimeoutTS = ktime_add_ms(ktime_get(), priv->tTimeoutDurationMs);
				}

#ifdef VERBOSE
				pr_info("piControlIoctl Addr=%u, bit=%u: %02x %02x\n", pValue->i16uAddress, pValue->i8uBit, pValue->i8uValue, i8uValue_l);
#endif

				status = 0;
			}
		}
		break;

	case KB_FIND_VARIABLE:
		{
			int i;
			SPIVariable *var = (SPIVariable *) usr_addr;

			if (!isRunning())
				return -EFAULT;

			if (!piDev_g.ent) {
				status = -ENOENT;
				break;
			}

			for (i = 0; i < piDev_g.ent->i16uNumEntries; i++) {
				//pr_info("strcmp(%s, %s)\n", piDev_g.ent->ent[i].strVarName, var->strVarName);
				if (strcmp(piDev_g.ent->ent[i].strVarName, var->strVarName) == 0) {
					var->i16uAddress = piDev_g.ent->ent[i].i16uOffset;
					var->i8uBit = piDev_g.ent->ent[i].i8uBitPos;
					var->i16uLength = piDev_g.ent->ent[i].i16uBitLength;
					status = 0;
					return status;
				}
			}

			var->i16uAddress = 0xffff;
			var->i8uBit = 0xff;
			var->i16uLength = 0xffff;

		}
		break;

	case KB_SET_EXPORTED_OUTPUTS:
		{
			int i;
			ktime_t now;

			if (!isRunning())
				return -EFAULT;

			if (file == 0 || usr_addr == 0) {
				pr_info("piControlIoctl: illegal parameter\n");
				return -EINVAL;
			}

			if (piDev_g.cl == 0 || piDev_g.cl->ent == 0 || piDev_g.cl->i16uNumEntries == 0)
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

					val1 = *(uint8_t *) (usr_addr + addr);
					val1 &= mask;

					val2 = piDev_g.ai8uPI[addr];
					val2 &= ~mask;
					val2 |= val1;
					piDev_g.ai8uPI[addr] = val2;

					//pr_info("piControlIoctl copy mask %02x at offset %d\n", mask, addr);
				}
#if 0
				pr_info("piControlIoctl inst %d time %5d: copy %d bits at offset %d: %x %x %x %x\n",
					priv->instNum,
					(int)(ktime_to_ms(now)) % 10000,
					len, addr,
					((unsigned char *)(usr_addr + addr))[0],
					((unsigned char *)(usr_addr + addr))[1],
					((unsigned char *)(usr_addr + addr))[2],
					((unsigned char *)(usr_addr + addr))[3]);
#endif
			}
			rt_mutex_unlock(&piDev_g.lockPI);

			if (priv->tTimeoutDurationMs > 0) {
				priv->tTimeoutTS = ktime_add_ms(ktime_get(), priv->tTimeoutDurationMs);
			}
		}
		break;

	case KB_DIO_RESET_COUNTER:
		{
			SDioCounterReset tel;
			SDIOResetCounter *pPar = (SDIOResetCounter *) usr_addr;
			int i;
			bool found = false;

			if (piDev_g.machine_type != REVPI_CORE && piDev_g.machine_type != REVPI_CONNECT) {
				return -EPERM;
			}

			if (!isRunning()) {
				return -EFAULT;
			}

			for (i = 0; i < RevPiDevice_getDevCnt() && !found; i++) {
				if (RevPiDevice_getDev(i)->i8uAddress == pPar->i8uAddress
				    && RevPiDevice_getDev(i)->i8uActive
				    && (RevPiDevice_getDev(i)->sId.i16uModulType == KUNBUS_FW_DESCR_TYP_PI_DIO_14
					|| RevPiDevice_getDev(i)->sId.i16uModulType == KUNBUS_FW_DESCR_TYP_PI_DI_16)) {
					found = true;
					break;
				}
			}

			if (!found || pPar->i16uBitfield == 0) {
				pr_info("piControlIoctl: resetCounter failed bitfield 0x%x", pPar->i16uBitfield);
				return -EINVAL;
			}

			tel.uHeader.sHeaderTyp1.bitAddress = pPar->i8uAddress;
			tel.uHeader.sHeaderTyp1.bitIoHeaderType = 0;
			tel.uHeader.sHeaderTyp1.bitReqResp = 0;
			tel.uHeader.sHeaderTyp1.bitLength = sizeof(SDioCounterReset) - IOPROTOCOL_HEADER_LENGTH - 1;
			tel.uHeader.sHeaderTyp1.bitCommand = IOP_TYP1_CMD_DATA3;
			tel.i16uChannels = pPar->i16uBitfield;

			my_rt_mutex_lock(&piCore_g.lockUserTel);
			memcpy(&piCore_g.requestUserTel, &tel, sizeof(tel));

			piCore_g.pendingUserTel = true;
			down(&piCore_g.semUserTel);
			status = piCore_g.statusUserTel;
			pr_info("piControlIoctl: resetCounter result %d", status);
			rt_mutex_unlock(&piCore_g.lockUserTel);
		}
		break;

	case KB_INTERN_SET_SERIAL_NUM:
		{
			INT32U *pData = (INT32U *) usr_addr;	// pData is an array containing the module address and the serial number

			if (piDev_g.machine_type != REVPI_CORE && piDev_g.machine_type != REVPI_CONNECT) {
				return -EPERM;
			}

			if (!isRunning())
				return -EFAULT;

			PiBridgeMaster_Stop();
			msleep(500);

			if (PiBridgeMaster_FWUModeEnter(pData[0], 1) == 0) {

				if (PiBridgeMaster_FWUsetSerNum(pData[1]) == 0) {
					pr_info("piControlIoctl: set serial number to %u in module %u", pData[1], pData[0]);
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
		}
		break;

	case KB_UPDATE_DEVICE_FIRMWARE:
		{
			int i, ret, cnt;
			INT32U *pData = (INT32U *) usr_addr;	// pData is null or points to the module address

			if (piDev_g.machine_type != REVPI_CORE && piDev_g.machine_type != REVPI_CONNECT) {
				return -EPERM;
			}

			if (!isRunning()) {
				printUserMsg(priv, "piControl is not running");
				return -EFAULT;
			}
			// at the moment the update works only, if there is only one module connected on the right
#if 0
#ifndef ENDTEST_DIO
			if (RevPiDevice_getDevCnt() > 2)	// RevPi + Module to update
			{
				printUserMsg(priv, "Too many modules! Firmware update is only possible, if there is exactly one module on the right of the RevPi Core.");
				return -EFAULT;
			}
#endif
			if (RevPiDevice_getDevCnt() < 2
			    || RevPiDevice_getDev(1)->i8uActive == 0 || RevPiDevice_getDev(1)->sId.i16uModulType >= PICONTROL_SW_OFFSET) {
				printUserMsg(priv, "No module to update! Firmware update is only possible, if there is exactly one module on the right of the RevPi Core.");
				return -EFAULT;
			}
#endif
			PiBridgeMaster_Stop();
			msleep(50);

			cnt = 0;
			for (i = 0; i < RevPiDevice_getDevCnt(); i++) {
				if (pData != 0 && RevPiDevice_getDev(i)->i8uAddress != *pData) {
					// if pData is not 0, we want to update one specific module
					// -> update all others
					pr_info("skip %d addr %d\n", i, RevPiDevice_getDev(i)->i8uAddress);
				} else {
					if (RevPiDevice_getDev(i)->sId.i16uModulType < PICONTROL_SW_OFFSET) {
						ret = FWU_update(priv, RevPiDevice_getDev(i));
						pr_info("update %d addr %d ret %d\n", i, RevPiDevice_getDev(i)->i8uAddress, ret);
						if (ret > 0) {
							cnt++;
							// update only one device per call
							break;
						} else {
							status = cnt;
						}
					}
				}
			}

			if (cnt) {
				// at least one module was updated, make a reset
				status = piControlReset(priv);
			} else {
				PiBridgeMaster_Continue();
			}
		}
		break;

	case KB_INTERN_IO_MSG:
		{
			SIOGeneric *tel = (SIOGeneric *) usr_addr;

			if (piDev_g.machine_type != REVPI_CORE && piDev_g.machine_type != REVPI_CONNECT) {
				return -EPERM;
			}

			if (!isRunning())
				return -EFAULT;

			my_rt_mutex_lock(&piCore_g.lockUserTel);
			if (copy_from_user(&piCore_g.requestUserTel, tel, sizeof(SIOGeneric))) {
				rt_mutex_unlock(&piCore_g.lockUserTel);
				status = -EFAULT;
				break;
			}
			piCore_g.pendingUserTel = true;
			down(&piCore_g.semUserTel);
			status = piCore_g.statusUserTel;
			if (copy_to_user(tel, &piCore_g.responseUserTel, sizeof(SIOGeneric))) {
				rt_mutex_unlock(&piCore_g.lockUserTel);
				status = -EFAULT;
				break;
			}
			rt_mutex_unlock(&piCore_g.lockUserTel);
			status = 0;
		}
		break;

	case KB_WAIT_FOR_EVENT:
		{
			tpiEventEntry *pEntry;
			int *pData = (int *)usr_addr;

			pr_info_drv("wait(%d)\n", priv->instNum);
			if (wait_event_interruptible(priv->wq, !list_empty(&priv->piEventList)) == 0) {
				my_rt_mutex_lock(&priv->lockEventList);
				pEntry = list_first_entry(&priv->piEventList, tpiEventEntry, list);

				pr_info_drv("wait(%d): got event %d\n", priv->instNum, pEntry->event);
				*pData = pEntry->event;

				list_del(&pEntry->list);
				rt_mutex_unlock(&priv->lockEventList);
				kfree(pEntry);

				status = 0;
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
			// parameter = 0: stop I/Os
			// parameter = 1: start I/Os
			// parameter = 2: toggle I/Os
			int *pData = (int *)usr_addr;

			if (!isRunning())
				return -EFAULT;

			if (*pData == 2) {
				piDev_g.stopIO = !piDev_g.stopIO;
			} else {
				piDev_g.stopIO = (*pData) ? true : false;
			}
			status = piDev_g.stopIO;
		}
		break;

	case KB_CONFIG_STOP:	// for download of configuration to Master Gateway: stop IO communication completely
		{
			if (piDev_g.machine_type != REVPI_CORE && piDev_g.machine_type != REVPI_CONNECT) {
				return -EPERM;
			}

			if (!isRunning()) {
				printUserMsg(priv, "piControl is not running");
				return -EFAULT;
			}
			PiBridgeMaster_Stop();
			msleep(50);
			status = 0;
		}
		break;

	case KB_CONFIG_SEND:	// for download of configuration to Master Gateway: download config data
		{
			SConfigData *pData = (SConfigData *) usr_addr;

			if (piDev_g.machine_type != REVPI_CORE && piDev_g.machine_type != REVPI_CONNECT) {
				return -EPERM;
			}
			if (!isInitialized()) {
				return -EFAULT;
			}
			if (isRunning()) {
				return -ECANCELED;
			}

			my_rt_mutex_lock(&piCore_g.lockGateTel);
			if (copy_from_user(piCore_g.ai8uSendDataGateTel, pData->acData, pData->i16uLen)) {
				rt_mutex_unlock(&piCore_g.lockGateTel);
				status = -EFAULT;
				break;
			}
			piCore_g.i8uSendDataLenGateTel = pData->i16uLen;

			if (pData->bLeft) {
				piCore_g.i8uAddressGateTel = RevPiDevice_getDev(piCore_g.i8uLeftMGateIdx)->i8uAddress;
			} else {
				piCore_g.i8uAddressGateTel = RevPiDevice_getDev(piCore_g.i8uRightMGateIdx)->i8uAddress;
			}
			piCore_g.i16uCmdGateTel = eCmdRAPIMessage;

			piCore_g.pendingGateTel = true;
			down(&piCore_g.semGateTel);
			status = piCore_g.statusGateTel;

			pData->i16uLen = piCore_g.i16uRecvDataLenGateTel;
			if (copy_to_user(pData->acData, &piCore_g.ai8uRecvDataGateTel, pData->i16uLen)) {
				rt_mutex_unlock(&piCore_g.lockGateTel);
				status = -EFAULT;
				break;
			}
			rt_mutex_unlock(&piCore_g.lockGateTel);
			status = 0;
		}
		break;

	case KB_CONFIG_START:	// for download of configuration to Master Gateway: restart IO communication
		{
			if (piDev_g.machine_type != REVPI_CORE && piDev_g.machine_type != REVPI_CONNECT) {
				return -EPERM;
			}
			if (!isInitialized()) {
				return -EFAULT;
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
			unsigned int *pData = (unsigned int *)usr_addr;
			priv->tTimeoutDurationMs = *pData;
			priv->tTimeoutTS = ktime_add_ms(ktime_get(), priv->tTimeoutDurationMs);
			status = 0;
		}
		break;

	default:
		pr_err("Invalid Ioctl from %s", current->comm);
		return (-EINVAL);
		break;

	}

	return status;
}

module_init(piControlInit);
module_exit(piControlCleanup);

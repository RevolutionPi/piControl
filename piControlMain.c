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
#include <linux/slab.h>

#include "revpi_common.h"
#include "revpi_core.h"
#include "revpi_compact.h"
#include "revpi_flat.h"
#include "compat.h"
#include "revpi_mio.h"

#include "piFirmwareUpdate.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Christof Vogt, Mathias Duckeck, Lukas Wunner");
MODULE_DESCRIPTION("piControl Driver");
MODULE_VERSION("1.4.0");
MODULE_SOFTDEP("pre: bcm2835-thermal "	/* cpu temp in process image */
	       "ks8851 "		/* core eth gateways */
	       "spi-bcm2835 "		/* core spi0 eth gateways */
	       "spi-bcm2835aux "	/* compact spi2 i/o */
	       "gpio-74x164 "		/* compact dout */
	       "fixed "			/* compact ain/aout vref */
	       "mux_gpio "		/* compact ain mux */
	       "iio_mux "		/* compact ain mux */
	       "mcp320x "		/* compact ain */
	       "ti-dac082s085 "		/* compact aout */
	       "ad5446");		/* flat aout */

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
	.a2_green.name = "a2_green",
	.a2_red.name = "a2_red",
	.a3_green.name = "a3_green",
	.a3_red.name = "a3_red",
	.a4_green.name = "a4_green",
	.a4_red.name = "a4_red",
	.a5_green.name = "a5_green",
	.a5_red.name = "a5_red",
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

	wait_for_device_probe();

	pr_info("built: %s\n", COMPILETIME);

	if (of_machine_is_compatible("kunbus,revpi-compact")) {
		piDev_g.machine_type = REVPI_COMPACT;
		pr_info("RevPi Compact\n");
	} else if (of_machine_is_compatible("kunbus,revpi-connect")) {
		piDev_g.machine_type = REVPI_CONNECT;
		pr_info("RevPi Connect\n");
	} else if (of_machine_is_compatible("kunbus,revpi-flat")) {
		piDev_g.machine_type = REVPI_FLAT;
		pr_info("RevPi Flat\n");
	} else {
		piDev_g.machine_type = REVPI_CORE;
		pr_info("RevPi Core\n");
	}

	// alloc_chrdev_region return 0 on success
	res = alloc_chrdev_region(&piControlMajor, 0, 2, "piControl");
	if (res) {
		pr_err("cannot allocate char device numbers\n");
		return res;
	}

	pr_info("MAJOR-No.  : %d\n", MAJOR(piControlMajor));

	piControlClass = class_create(THIS_MODULE, "piControl");
	if (IS_ERR(piControlClass)) {
		pr_err("cannot create class\n");
		res = PTR_ERR(piControlClass);
		goto err_unreg_chrdev_region;
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
		res = PTR_ERR(piDev_g.dev);
		goto err_class_destroy;
	}

	pr_info("MAJOR-No.  : %d  MINOR-No.  : %d\n", MAJOR(curdev), MINOR(curdev));

	res = devm_led_trigger_register(piDev_g.dev, &piDev_g.power_red)
	   || devm_led_trigger_register(piDev_g.dev, &piDev_g.a1_green)
	   || devm_led_trigger_register(piDev_g.dev, &piDev_g.a1_red)
	   || devm_led_trigger_register(piDev_g.dev, &piDev_g.a2_green)
	   || devm_led_trigger_register(piDev_g.dev, &piDev_g.a2_red);

	if ((piDev_g.machine_type == REVPI_CONNECT) ||
	    (piDev_g.machine_type == REVPI_FLAT)) {
		res = res
		   || devm_led_trigger_register(piDev_g.dev, &piDev_g.a3_green)
		   || devm_led_trigger_register(piDev_g.dev, &piDev_g.a3_red);
	}

	if (piDev_g.machine_type == REVPI_FLAT) {
		res = res
		   || devm_led_trigger_register(piDev_g.dev, &piDev_g.a4_green)
		   || devm_led_trigger_register(piDev_g.dev, &piDev_g.a4_red)
		   || devm_led_trigger_register(piDev_g.dev, &piDev_g.a5_green)
		   || devm_led_trigger_register(piDev_g.dev, &piDev_g.a5_red);
	}

	if (res) {
		pr_err("cannot register LED triggers\n");
		goto err_dev_destroy;
	}

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

	if (piDev_g.machine_type == REVPI_CORE) {
		res = revpi_core_init();
	} else if (piDev_g.machine_type == REVPI_CONNECT) {
		res = revpi_core_init();
	} else if (piDev_g.machine_type == REVPI_COMPACT) {
		res = revpi_compact_init();
	} else if (piDev_g.machine_type == REVPI_FLAT) {
		res = revpi_flat_init();
	}
	if (res)
		goto err_free_config;

	showPADS();

	piDev_g.thermal_zone = thermal_zone_get_zone_by_name(BCM2835_THERMAL_ZONE);
	if (IS_ERR(piDev_g.thermal_zone)) {
		pr_err("cannot find thermal zone\n");
		piDev_g.thermal_zone = NULL;
	}

	res = cdev_add(&piDev_g.cdev, curdev, 1);
	if (res) {
		pr_err("cannot add cdev\n");
		goto err_revpi_fini;
	}

	pr_info("piControlInit done\n");
	return 0;

err_revpi_fini:
	if (piDev_g.machine_type == REVPI_CORE) {
		revpi_core_fini();
	} else if (piDev_g.machine_type == REVPI_CONNECT) {
		revpi_core_fini();
	} else if (piDev_g.machine_type == REVPI_COMPACT) {
		revpi_compact_fini();
	}
err_free_config:
	kfree(piDev_g.ent);
	kfree(piDev_g.devs);
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
		revpi_compact_reset();
	} else if (piDev_g.machine_type == REVPI_FLAT) {
		revpi_flat_reset();
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
	dev_t curdev;

	pr_info_drv("piControlCleanup\n");

	cdev_del(&piDev_g.cdev);

	if (piDev_g.machine_type == REVPI_CORE) {
		revpi_core_fini();
	} else if (piDev_g.machine_type == REVPI_CONNECT) {
		revpi_core_fini();
	} else if (piDev_g.machine_type == REVPI_COMPACT) {
		revpi_compact_fini();
	} else if (piDev_g.machine_type == REVPI_FLAT) {
		revpi_flat_fini();
	}

	kfree(piDev_g.ent);
	kfree(piDev_g.devs);
	curdev = MKDEV(MAJOR(piControlMajor), MINOR(piControlMajor));
	device_destroy(piControlClass, curdev);
	class_destroy(piControlClass);
	unregister_chrdev_region(piControlMajor, 2);

	pr_info_drv("driver stopped with MAJOR-No. %d\n\n ", MAJOR(piControlMajor));
	pr_info_drv("piControlCleanup done\n");
}

// true: system is running
// false: system is not operational
bool isRunning(void)
{
	if ((piDev_g.machine_type == REVPI_CORE ||
	     piDev_g.machine_type == REVPI_CONNECT) &&
	    (piCore_g.eBridgeState != piBridgeRun)) {
		return false;
	}
	return true;
}

// true: system is running
// false: system is not operational
bool waitRunning(int timeout)	// ms
{
	if ((piDev_g.machine_type == REVPI_COMPACT) ||
	    (piDev_g.machine_type == REVPI_FLAT))
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
		if(eRunStatus_s != enPiBridgeMasterStatus_FWUMode){
			pr_err("no piControl reset possible, a firmware update is running\n");
			kfree(priv);
			return -EINVAL;
		}
		status = piControlReset(priv);
		if (status) {
			kfree(priv);
			return status;
		}
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

		if ((piDev_g.machine_type == REVPI_CORE ||
		     piDev_g.machine_type == REVPI_CONNECT) && isRunning()) {
			PiBridgeMaster_Stop();
		}
		status = piControlReset(priv);
		break;

	case KB_GET_DEVICE_INFO:
		{
			SDeviceInfo dev_info;
			int i, found;

			if (!access_ok((void __user *) usr_addr, sizeof(dev_info))) {
				pr_err("invalid address provided by user\n");
				return -EFAULT;
			}

			if (__copy_from_user(&dev_info, (const void __user *) usr_addr, sizeof(dev_info))) {
				pr_err("failed to copy dev info from user\n");
				return -EFAULT;
			}

			for (i = 0, found = 0; i < RevPiDevice_getDevCnt(); i++) {
				if (dev_info.i8uAddress != 0 && dev_info.i8uAddress == RevPiDevice_getDev(i)->i8uAddress) {
					found = 1;
					break;
				}
				if (dev_info.i16uModuleType != 0 && dev_info.i16uModuleType == RevPiDevice_getDev(i)->sId.i16uModulType) {
					found = 1;
					break;
				}
			}
			if (found) {
				dev_info.i8uAddress = RevPiDevice_getDev(i)->i8uAddress;
				dev_info.i8uActive = RevPiDevice_getDev(i)->i8uActive;
				dev_info.i32uSerialnumber = RevPiDevice_getDev(i)->sId.i32uSerialnumber;
				dev_info.i16uModuleType = RevPiDevice_getDev(i)->sId.i16uModulType;
				dev_info.i16uHW_Revision = RevPiDevice_getDev(i)->sId.i16uHW_Revision;
				dev_info.i16uSW_Major = RevPiDevice_getDev(i)->sId.i16uSW_Major;
				dev_info.i16uSW_Minor = RevPiDevice_getDev(i)->sId.i16uSW_Minor;
				dev_info.i32uSVN_Revision = RevPiDevice_getDev(i)->sId.i32uSVN_Revision;
				dev_info.i16uInputLength = RevPiDevice_getDev(i)->sId.i16uFBS_InputLength;
				dev_info.i16uInputOffset = RevPiDevice_getDev(i)->i16uInputOffset;
				dev_info.i16uOutputLength = RevPiDevice_getDev(i)->sId.i16uFBS_OutputLength;
				dev_info.i16uOutputOffset = RevPiDevice_getDev(i)->i16uOutputOffset;
				dev_info.i16uConfigLength = RevPiDevice_getDev(i)->i16uConfigLength;
				dev_info.i16uConfigOffset = RevPiDevice_getDev(i)->i16uConfigOffset;
				dev_info.i8uModuleState = RevPiDevice_getDev(i)->i8uModuleState;

				if (__copy_to_user((void * __user) usr_addr, &dev_info, sizeof(dev_info))) {
					pr_err("failed to copy dev info to user\n");
					return -EFAULT;
				}

				status = i;
				break;
			}
			/* nothing found */
			return -ENXIO;
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
				dev_infos[i].i8uAddress = RevPiDevice_getDev(i)->i8uAddress;
				dev_infos[i].i8uActive = RevPiDevice_getDev(i)->i8uActive;
				dev_infos[i].i32uSerialnumber = RevPiDevice_getDev(i)->sId.i32uSerialnumber;
				dev_infos[i].i16uModuleType = RevPiDevice_getDev(i)->sId.i16uModulType;
				dev_infos[i].i16uHW_Revision = RevPiDevice_getDev(i)->sId.i16uHW_Revision;
				dev_infos[i].i16uSW_Major = RevPiDevice_getDev(i)->sId.i16uSW_Major;
				dev_infos[i].i16uSW_Minor = RevPiDevice_getDev(i)->sId.i16uSW_Minor;
				dev_infos[i].i32uSVN_Revision = RevPiDevice_getDev(i)->sId.i32uSVN_Revision;
				dev_infos[i].i16uInputLength = RevPiDevice_getDev(i)->sId.i16uFBS_InputLength;
				dev_infos[i].i16uInputOffset = RevPiDevice_getDev(i)->i16uInputOffset;
				dev_infos[i].i16uOutputLength = RevPiDevice_getDev(i)->sId.i16uFBS_OutputLength;
				dev_infos[i].i16uOutputOffset = RevPiDevice_getDev(i)->i16uOutputOffset;
				dev_infos[i].i16uConfigLength = RevPiDevice_getDev(i)->i16uConfigLength;
				dev_infos[i].i16uConfigOffset = RevPiDevice_getDev(i)->i16uConfigOffset;
				dev_infos[i].i8uModuleState = RevPiDevice_getDev(i)->i8uModuleState;

				if (	dev_infos[i].i16uModuleType == KUNBUS_FW_DESCR_TYP_PI_DIO_14
				||	dev_infos[i].i16uModuleType == KUNBUS_FW_DESCR_TYP_PI_DO_16
				||	dev_infos[i].i16uModuleType == KUNBUS_FW_DESCR_TYP_PI_DI_16) {
					// DIO with firmware older than 1.4 should be updated
					if (	dev_infos[i].i16uSW_Major < 1
					    || (dev_infos[i].i16uSW_Major == 1 && dev_infos[i].i16uSW_Minor < 4)) {
						firmware_update = true;
					}
				}
				if (	dev_infos[i].i16uModuleType == KUNBUS_FW_DESCR_TYP_PI_AIO) {
					// AIO with firmware older than 1.3 should be updated
					if (	dev_infos[i].i16uSW_Major < 1
					    || (dev_infos[i].i16uSW_Major == 1 && dev_infos[i].i16uSW_Minor < 3)) {
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

			if (!access_ok((void __user *) usr_addr,
				       sizeof(spi_val))) {
				pr_err("invalid address provided by user\n");
				return -EFAULT;
			}

			if (__copy_from_user(&spi_val, (const void __user *) usr_addr,
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

				if (__copy_to_user((void __user *) usr_addr, &spi_val,
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

#ifdef VERBOSE
				pr_info("piControlIoctl Addr=%u, bit=%u: %02x %02x\n", spi_val.i16uAddress, spi_val.i8uBit, spi_val.i8uValue, i8uValue_l);
#endif

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
				//pr_info("strcmp(%s, %s)\n", piDev_g.ent->ent[i].strVarName, var->strVarName);
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
			SDIOResetCounter res_cnt;
			int i;
			bool found = false;

			if (piDev_g.machine_type != REVPI_CORE && piDev_g.machine_type != REVPI_CONNECT) {
				return -EPERM;
			}

			if (!isRunning()) {
				return -EFAULT;
			}

			if (copy_from_user(&res_cnt, (const void __user *) usr_addr, sizeof(res_cnt))) {
				pr_err("failed to copy reset counter from user\n");
				return -EFAULT;
			}

			for (i = 0; i < RevPiDevice_getDevCnt() && !found; i++) {
				if (RevPiDevice_getDev(i)->i8uAddress == res_cnt.i8uAddress
				    && RevPiDevice_getDev(i)->i8uActive
				    && (RevPiDevice_getDev(i)->sId.i16uModulType == KUNBUS_FW_DESCR_TYP_PI_DIO_14
					|| RevPiDevice_getDev(i)->sId.i16uModulType == KUNBUS_FW_DESCR_TYP_PI_DI_16)) {
					found = true;
					break;
				}
			}

			if (!found || res_cnt.i16uBitfield == 0) {
				pr_info("piControlIoctl: resetCounter failed bitfield 0x%x", res_cnt.i16uBitfield);
				return -EINVAL;
			}

			tel.uHeader.sHeaderTyp1.bitAddress = res_cnt.i8uAddress;
			tel.uHeader.sHeaderTyp1.bitIoHeaderType = 0;
			tel.uHeader.sHeaderTyp1.bitReqResp = 0;
			tel.uHeader.sHeaderTyp1.bitLength = sizeof(SDioCounterReset) - IOPROTOCOL_HEADER_LENGTH - 1;
			tel.uHeader.sHeaderTyp1.bitCommand = IOP_TYP1_CMD_DATA3;
			tel.i16uChannels = res_cnt.i16uBitfield;

			my_rt_mutex_lock(&piCore_g.lockUserTel);
			memcpy(&piCore_g.requestUserTel, &tel, sizeof(tel));

			piCore_g.pendingUserTel = true;
			down(&piCore_g.semUserTel);
			status = piCore_g.statusUserTel;
			pr_info("piControlIoctl: resetCounter result %d", status);
			rt_mutex_unlock(&piCore_g.lockUserTel);
		}
		break;
	case KB_AIO_CALIBRATE:
	{
		int i;
		bool found = false;
		struct pictl_calibrate cali;
		SMioCalibrationRequest req;

		if (piDev_g.machine_type != REVPI_CORE
			&& piDev_g.machine_type != REVPI_CONNECT) {
			return -EPERM;
		}

		if (!isRunning()) {
			return -EFAULT;
		}

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
			pr_info("calibrate failed, channel 0x%x\n",
						cali.channels);
			return -EINVAL;
		}

		revpi_io_build_header(&req.uHeader, cali.address,
			sizeof(SMioCalibrationRequestData), IOP_TYP1_CMD_DATA6);
		req.sData.i8uCalibrationMode = cali.mode;
		req.sData.i8uChannels = cali.channels;
		req.sData.i8uPoint = cali.x_val;
		req.sData.i16sCalibrationValue = cali.y_val;
		/*the crc calculation will be done by Tel sending*/

		my_rt_mutex_lock(&piCore_g.lockUserTel);
		memcpy(&piCore_g.requestUserTel, &req, sizeof(req));
		piCore_g.pendingUserTel = true;
		down(&piCore_g.semUserTel);
		status = piCore_g.statusUserTel;
		rt_mutex_unlock(&piCore_g.lockUserTel);

		pr_info("MIO calibrate header:0x%x, data:0x%x, status %d\n",
			*(unsigned short *)&req.uHeader,
			*(unsigned short *)&req.sData,
			status);

		break;
	}
	case KB_INTERN_SET_SERIAL_NUM:
		{
			u32 snum_data[2]; 	// snum_data is an array containing the module address and the serial number

			if (piDev_g.machine_type != REVPI_CORE && piDev_g.machine_type != REVPI_CONNECT) {
				return -EPERM;
			}

			if (!isRunning())
				return -EFAULT;

			if (copy_from_user(snum_data, (const void __user *) usr_addr,
					   sizeof(snum_data))) {
				pr_err("failed to copy serial num from user\n");
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
		}
		break;

	case KB_UPDATE_DEVICE_FIRMWARE:
		{
			int i, ret, cnt;
			u32 data;
			INT32U *pData = NULL;	// pData is null or points to the module address


			if (piDev_g.machine_type != REVPI_CORE && piDev_g.machine_type != REVPI_CONNECT) {
				return -EPERM;
			}

			if (!isRunning()) {
				printUserMsg(priv, "piControl is not running");
				return -EFAULT;
			}

			if (usr_addr) {
				if (get_user(data, (u32 __user *) usr_addr)) {
					pr_err("failed to copy update info from user\n");
					return -EFAULT;
				}

				pData = &data;
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

			pr_info_drv("wait(%d)\n", priv->instNum);
			if (wait_event_interruptible(priv->wq, !list_empty(&priv->piEventList)) == 0) {
				my_rt_mutex_lock(&priv->lockEventList);
				pEntry = list_first_entry(&priv->piEventList, tpiEventEntry, list);

				pr_info_drv("wait(%d): got event %d\n", priv->instNum, pEntry->event);

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
			// parameter = 0: stop I/Os
			// parameter = 1: start I/Os
			// parameter = 2: toggle I/Os
			u32 data;

			if (!isRunning())
				return -EFAULT;

			if (get_user(data, (u32 __user *) usr_addr)) {
				pr_err("Failed to copy stop command from user\n");
				return -EFAULT;
			}

			if (data == 2) {
				piDev_g.stopIO = !piDev_g.stopIO;
			} else {
				piDev_g.stopIO = data ? true : false;
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

module_init(piControlInit);
module_exit(piControlCleanup);

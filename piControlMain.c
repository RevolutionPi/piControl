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
#include <asm/div64.h>

#include <bsp/uart/uart.h>

#include <ModGateComMain.h>
#include <ModGateComError.h>
#include <bsp/spi/spi.h>
#include <bsp/ksz8851/ksz8851.h>

#include <kbUtilities.h>
#include <piControlMain.h>
#include <piControl.h>
#include <piConfig.h>
#include <PiBridgeMaster.h>
#include <RevPiDevice.h>
#include <piIOComm.h>
#include "piFirmwareUpdate.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Christof Vogt, Mathias Duckeck");
MODULE_DESCRIPTION("piControl Driver");
MODULE_VERSION("1.2.0");
MODULE_SOFTDEP("bcm2835-thermal");


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
llseek: piControlSeek,
open:	piControlOpen,
unlocked_ioctl:piControlIoctl,
release:piControlRelease
};

tpiControlDev piDev_g;

static dev_t piControlMajor;
static struct class *piControlClass;
static struct module *piSpiModule;
static char pcErrorMessage[200];

/******************************************************************************/
/*******************************  Functions  **********************************/
/******************************************************************************/


/******************************************************************************/
/***************************** CS Functions  **********************************/
/******************************************************************************/
enum hrtimer_restart piControlGateTimer(struct hrtimer *pTimer)
{
	up(&piDev_g.gateSem);
	return HRTIMER_NORESTART;
}

int piGateThread(void *data)
{
	//TODO int value = 0;
	ktime_t time;
	INT8U i8uLastState[2];
	int i;
	s64 interval;
#ifdef MEASURE_DURATION
	INT32U j1, j1_max = 0;
	INT32U j2, j2_max = 0;
	INT32U j3, j3_max = 0;
	INT32U j4, j4_max = 0;
#endif
#ifdef VERBOSE
	INT16U val;
	val = 0;
#endif

	hrtimer_init(&piDev_g.gateTimer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
	piDev_g.gateTimer.function = piControlGateTimer;
	i8uLastState[0] = 0;
	i8uLastState[1] = 0;

	//TODO down(&piDev.gateSem);
	pr_info("number of CPUs: %d\n", NR_CPUS);
	if (NR_CPUS == 1) {
		// use a longer interval time on CM1
		interval = INTERVAL_PI_GATE + INTERVAL_PI_GATE;
	} else {
		interval = INTERVAL_PI_GATE;
	}

	time = hrtimer_cb_get_time(&piDev_g.gateTimer);

	/* start after one second */
	time.tv64 += HZ;

	pr_info("mGate thread started\n");

	while (!kthread_should_stop()) {
		time.tv64 += interval;

		DURSTART(j4);
		hrtimer_start(&piDev_g.gateTimer, time, HRTIMER_MODE_ABS);
		down(&piDev_g.gateSem);
		DURSTOP(j4);

		if (isRunning()) {
			DURSTART(j1);
			rt_mutex_lock(&piDev_g.lockPI);
			if (piDev_g.i8uRightMGateIdx != REV_PI_DEV_UNDEF) {
				memcpy(piDev_g.ai8uOutput,
				       piDev_g.ai8uPI +
				       RevPiScan.dev[piDev_g.i8uRightMGateIdx].i16uOutputOffset,
				       RevPiScan.dev[piDev_g.i8uRightMGateIdx].sId.i16uFBS_OutputLength);
			}
			if (piDev_g.i8uLeftMGateIdx != REV_PI_DEV_UNDEF) {
				memcpy(piDev_g.ai8uOutput + KB_PD_LEN,
				       piDev_g.ai8uPI +
				       RevPiScan.dev[piDev_g.i8uLeftMGateIdx].i16uOutputOffset,
				       RevPiScan.dev[piDev_g.i8uLeftMGateIdx].sId.i16uFBS_OutputLength);
			}
			rt_mutex_unlock(&piDev_g.lockPI);
			DURSTOP(j1);
		}

		DURSTART(j2);
		MODGATECOM_run();
		DURSTOP(j2);

		if (MODGATECOM_has_fatal_error()) {
			// stop the thread if an fatal error occurred
			pr_err("mGate exit thread because of fatal error\n");
			return -1;
		}

		if (piDev_g.i8uRightMGateIdx == REV_PI_DEV_UNDEF
		    && AL_Data_s[0].i8uState >= MODGATE_ST_RUN_NO_DATA && i8uLastState[0] < MODGATE_ST_RUN_NO_DATA) {
			// das mGate wurde beim Scan nicht erkannt, PiBridgeEth Kommunikation ist aber möglich
			// suche den Konfigeintrag dazu
			pr_info("search for right mGate %d\n", AL_Data_s[0].OtherID.i16uModulType);
			for (i = 0; i < RevPiScan.i8uDeviceCount; i++) {
				if (RevPiScan.dev[i].sId.i16uModulType ==
				    (AL_Data_s[0].OtherID.i16uModulType | PICONTROL_NOT_CONNECTED)
				    && RevPiScan.dev[i].i8uAddress >= REV_PI_DEV_FIRST_RIGHT) {
					pr_info("found mGate %d\n", i);
					RevPiScan.dev[i].i8uActive = 1;
					RevPiScan.dev[i].sId.i16uModulType &= PICONTROL_NOT_CONNECTED_MASK;
					piDev_g.i8uRightMGateIdx = i;
					break;
				}
			}
		}
		if (piDev_g.i8uLeftMGateIdx == REV_PI_DEV_UNDEF
		    && AL_Data_s[1].i8uState >= MODGATE_ST_RUN_NO_DATA && i8uLastState[1] < MODGATE_ST_RUN_NO_DATA) {
			// das mGate wurde beim Scan nicht erkannt, PiBridgeEth Kommunikation ist aber möglich
			// suche den Konfigeintrag dazu
			pr_info("search for left mGate %d\n", AL_Data_s[1].OtherID.i16uModulType);
			for (i = 0; i < RevPiScan.i8uDeviceCount; i++) {

				if (RevPiScan.dev[i].sId.i16uModulType ==
				    (AL_Data_s[1].OtherID.i16uModulType | PICONTROL_NOT_CONNECTED)
				    && RevPiScan.dev[i].i8uAddress < REV_PI_DEV_FIRST_RIGHT) {
					pr_info("found mGate %d\n", i);
					RevPiScan.dev[i].i8uActive = 1;
					RevPiScan.dev[i].sId.i16uModulType &= PICONTROL_NOT_CONNECTED_MASK;
					piDev_g.i8uLeftMGateIdx = i;
					break;
				}
			}
		}

		i8uLastState[0] = AL_Data_s[0].i8uState;
		i8uLastState[1] = AL_Data_s[1].i8uState;

		DURSTART(j3);
		if (isRunning()) {
			rt_mutex_lock(&piDev_g.lockPI);
			if (piDev_g.i8uRightMGateIdx != REV_PI_DEV_UNDEF) {
				memcpy(piDev_g.ai8uPI +
				       RevPiScan.dev[piDev_g.i8uRightMGateIdx].i16uInputOffset, piDev_g.ai8uInput,
				       RevPiScan.dev[piDev_g.i8uRightMGateIdx].sId.i16uFBS_InputLength);
			}
			if (piDev_g.i8uLeftMGateIdx != REV_PI_DEV_UNDEF) {
				memcpy(piDev_g.ai8uPI +
				       RevPiScan.dev[piDev_g.i8uLeftMGateIdx].i16uInputOffset,
				       piDev_g.ai8uInput + KB_PD_LEN,
				       RevPiScan.dev[piDev_g.i8uLeftMGateIdx].sId.i16uFBS_InputLength);
			}
			rt_mutex_unlock(&piDev_g.lockPI);
		}
		DURSTOP(j3);

#ifdef VERBOSE
		val++;
		if (val >= 200) {
			val = 0;
			if (piDev_g.i8uRightMGateIdx != REV_PI_DEV_UNDEF) {
				pr_info("right  %02x %02x   %d %d\n",
					  *(piDev_g.ai8uPI +
					    RevPiDevice.dev[piDev_g.i8uRightMGateIdx].i16uInputOffset),
					  *(piDev_g.ai8uPI +
					    RevPiDevice.dev[piDev_g.i8uRightMGateIdx].i16uOutputOffset),
					  RevPiDevice.dev[piDev_g.i8uRightMGateIdx].i16uInputOffset,
					  RevPiDevice.dev[piDev_g.i8uRightMGateIdx].i16uOutputOffset);
			} else {
				pr_info("right  no device\n");
			}

			if (piDev_g.i8uLeftMGateIdx != REV_PI_DEV_UNDEF) {
				pr_info("left %02x %02x   %d %d\n",
					  *(piDev_g.ai8uPI +
					    RevPiDevice.dev[piDev_g.i8uLeftMGateIdx].i16uInputOffset),
					  *(piDev_g.ai8uPI +
					    RevPiDevice.dev[piDev_g.i8uLeftMGateIdx].i16uOutputOffset),
					  RevPiDevice.dev[piDev_g.i8uLeftMGateIdx].i16uInputOffset,
					  RevPiDevice.dev[piDev_g.i8uLeftMGateIdx].i16uOutputOffset);
			} else {
				pr_info("left   no device\n");
			}
		}
#endif
	}

	pr_info("mGate exit\n");
	return 0;
}

enum hrtimer_restart piIoTimer(struct hrtimer *pTimer)
{
	up(&piDev_g.ioSem);
	return HRTIMER_NORESTART;
}

int piIoThread(void *data)
{
	//TODO int value = 0;
	ktime_t time;
	ktime_t now;
	s64 tDiff;

	hrtimer_init(&piDev_g.ioTimer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
	piDev_g.ioTimer.function = piIoTimer;

	pr_info("piIO thread started\n");

	now = hrtimer_cb_get_time(&piDev_g.ioTimer);

	PiBridgeMaster_Reset();

	while (!kthread_should_stop()) {
		if (PiBridgeMaster_Run() < 0)
			break;

		time = now;
		now = hrtimer_cb_get_time(&piDev_g.ioTimer);

		if (RevPiScan.pCoreData != NULL) {
			time = ktime_sub(now, time);
			RevPiScan.pCoreData->i8uIOCycle = ktime_to_ms(time);
		}

		if (piDev_g.tLastOutput1.tv64 != piDev_g.tLastOutput2.tv64) {
			tDiff = piDev_g.tLastOutput1.tv64 - piDev_g.tLastOutput2.tv64;
			tDiff = tDiff << 1;	// multiply by 2
			if ((now.tv64 - piDev_g.tLastOutput1.tv64) > tDiff
			    && isRunning()) {
				int i;
				// the outputs were not written by logiCAD for more than twice the normal period
				// the logiRTS must have been stopped or crashed
				// -> set all outputs to 0
				pr_info("logiRTS timeout, set all output to 0\n");
				rt_mutex_lock(&piDev_g.lockPI);
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
				piDev_g.tLastOutput1.tv64 = 0;
				piDev_g.tLastOutput2.tv64 = 0;
			}
		}

		if (piDev_g.eBridgeState == piBridgeInit) {
			time.tv64 += INTERVAL_RS485;
		} else {
			time.tv64 += INTERVAL_IO_COMM;
		}

		if ((now.tv64 - time.tv64) > 0) {
			// the call of PiBridgeMaster_Run() needed more time than the INTERVAL
			// -> wait an additional ms
			//pr_info("%d ms too late, state %d\n", (int)((now.tv64 - time.tv64) >> 20), piDev_g.eBridgeState);
			time.tv64 = now.tv64 + INTERVAL_ADDITIONAL;
		}

		hrtimer_start(&piDev_g.ioTimer, time, HRTIMER_MODE_ABS);
		down(&piDev_g.ioSem);	// wait for timer
	}

	RevPiDevice_finish();

	pr_info("piIO exit\n");
	return 0;
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
	INT32U i32uRv;
	dev_t curdev;
	struct sched_param param;
	int res;

	pr_info("built: %s\n", COMPILETIME);

	// Uart
#ifdef UART_TEST		// test
	UART_TConfig suUartConfig_l;
	suUartConfig_l.enBaudRate = UART_enBAUD_115200;
	suUartConfig_l.enParity = UART_enPARITY_EVEN;
	suUartConfig_l.enStopBit = UART_enSTOPBIT_1;
	suUartConfig_l.enDataLen = UART_enDATA_LEN_8;
	suUartConfig_l.enFlowControl = UART_enFLOWCTRL_NONE;
	suUartConfig_l.enRS485 = UART_enRS485_ENABLE;
	suUartConfig_l.cbReceive = piControlDummyReceive;	//CbReceive;
	suUartConfig_l.cbTransmit = NULL;	//CbTransmit;
	suUartConfig_l.cbError = NULL;	//CbError;
	if (UART_init(0, &suUartConfig_l) != 0) {
		return -EFAULT;
	}
	UART_putChar(0, 'a');
	msleep(10 * 1000);
	UART_close(0);
	return -EFAULT;
#endif

	BUILD_BUG_ON(!IS_ENABLED(CONFIG_SPI_BCM2835));

	if (IS_MODULE(CONFIG_SPI_BCM2835)) {
		request_module(SPI_MODULE);

		mutex_lock(&module_mutex);
		piSpiModule = find_module(SPI_MODULE);
		if (piSpiModule && !try_module_get(piSpiModule))
			piSpiModule = NULL;
		mutex_unlock(&module_mutex);

		if (!piSpiModule) {
			pr_err("cannot load %s module", SPI_MODULE);
			return -ENODEV;
		}
	}

	memset(&piDev_g, 0, sizeof(piDev_g));

	piDev_g.i8uLeftMGateIdx = REV_PI_DEV_UNDEF;
	piDev_g.i8uRightMGateIdx = REV_PI_DEV_UNDEF;

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
	mutex_init(&piDev_g.lockListCon);

	mutex_init(&piDev_g.lockUserTel);
	sema_init(&piDev_g.semUserTel, 0);
	piDev_g.pendingUserTel = false;

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


	/* Request gpios */
	if (gpio_request(GPIO_LED_PWRRED, "GPIO_LED_PWRRED")) {
		pr_err("cannot open gpio GPIO_LED_PWRRED\n");
		cleanup();
		return -EFAULT;
	} else {
		gpio_direction_output(GPIO_LED_PWRRED, 0);
		gpio_export(GPIO_LED_PWRRED, 0);
	}
	piDev_g.init_step = 3;

	if (gpio_request(GPIO_LED_AGRN, "GPIO_LED_AGRN")) {
		pr_err("cannot open gpio GPIO_LED_AGRN\n");
		cleanup();
		return -EFAULT;
	} else {
		gpio_direction_output(GPIO_LED_AGRN, 0);
		gpio_export(GPIO_LED_AGRN, 0);
	}
	piDev_g.init_step = 4;

	if (gpio_request(GPIO_LED_ARED, "GPIO_LED_ARED")) {
		pr_err("cannot open gpio GPIO_LED_ARED\n");
		cleanup();
		return -EFAULT;
	} else {
		gpio_direction_output(GPIO_LED_ARED, 0);
		gpio_export(GPIO_LED_ARED, 0);
	}
	piDev_g.init_step = 5;

	if (gpio_request(GPIO_LED_BGRN, "GPIO_LED_BGRN")) {
		pr_err("cannot open gpio GPIO_LED_BGRN\n");
		cleanup();
		return -EFAULT;
	} else {
		gpio_direction_output(GPIO_LED_BGRN, 0);
		gpio_export(GPIO_LED_BGRN, 0);
	}
	piDev_g.init_step = 6;

	if (gpio_request(GPIO_LED_BRED, "GPIO_LED_BRED")) {
		pr_err("cannot open gpio GPIO_LED_ARED\n");
		cleanup();
		return -EFAULT;
	} else {
		gpio_direction_output(GPIO_LED_BRED, 0);
		gpio_export(GPIO_LED_BRED, 0);
	}
	piDev_g.init_step = 7;

	if (gpio_request(GPIO_SNIFF1A, "Sniff1A") < 0) {
		pr_err("Cannot open gpio GPIO_SNIFF1A\n");
		cleanup();
		return -EFAULT;
	} else {
		gpio_direction_input(GPIO_SNIFF1A);
	}
	piDev_g.init_step = 8;

	if (gpio_request(GPIO_SNIFF1B, "Sniff1B") < 0) {
		pr_err("Cannot open gpio GPIO_SNIFF1B\n");
		cleanup();
		return -EFAULT;
	} else {
		gpio_direction_input(GPIO_SNIFF1B);
	}
	piDev_g.init_step = 9;

	if (gpio_request(GPIO_SNIFF2A, "Sniff2A") < 0) {
		pr_err("Cannot open gpio GPIO_SNIFF2A\n");
		cleanup();
		return -EFAULT;
	} else {
		gpio_direction_input(GPIO_SNIFF2A);
	}
	piDev_g.init_step = 10;

	if (gpio_request(GPIO_SNIFF2B, "Sniff2B") < 0) {
		pr_err("Cannot open gpio GPIO_SNIFF2B\n");
		cleanup();
		return -EFAULT;
	} else {
		gpio_direction_input(GPIO_SNIFF2B);
	}
	piDev_g.init_step = 11;

	/* init some data */
	rt_mutex_init(&piDev_g.lockPI);
	rt_mutex_init(&piDev_g.lockBridgeState);

	piDev_g.tLastOutput1.tv64 = 0;
	piDev_g.tLastOutput2.tv64 = 0;

	sema_init(&piDev_g.ioSem, 0);
	sema_init(&piDev_g.gateSem, 0);

	if (piIoComm_init()) {
		pr_err("open serial port failed\n");
		cleanup();
		return -EFAULT;
	}
	piDev_g.init_step = 12;

	/* start application */
	if (piConfigParse(PICONFIG_FILE, &piDev_g.devs, &piDev_g.ent, &piDev_g.cl, &piDev_g.connl) == 2) {
		// file not found, try old location
		piConfigParse(PICONFIG_FILE_WHEEZY, &piDev_g.devs, &piDev_g.ent, &piDev_g.cl, &piDev_g.connl);
		// ignore errors
	}
	piDev_g.init_step = 13;

	i32uRv = MODGATECOM_init(piDev_g.ai8uInput, KB_PD_LEN, piDev_g.ai8uOutput, KB_PD_LEN, &EthDrvKSZ8851_g);
	if (i32uRv != MODGATECOM_NO_ERROR) {
		pr_err("MODGATECOM_init error %08x\n", i32uRv);
		cleanup();
		return -EFAULT;
	}
	piDev_g.init_step = 14;

	/* run threads */
	piDev_g.pGateThread = kthread_run(&piGateThread, NULL, "piGateT");
	if (piDev_g.pGateThread == NULL) {
		pr_err("kthread_run(gate) failed\n");
		cleanup();
		return -EFAULT;
	}
	param.sched_priority = RT_PRIO_GATE;
	sched_setscheduler(piDev_g.pGateThread, SCHED_FIFO, &param);
	piDev_g.init_step = 15;

	piDev_g.pUartThread = kthread_run(&UartThreadProc, (void *)NULL, "piUartThread");
	if (piDev_g.pUartThread == NULL) {
		pr_err("kthread_run(uart) failed\n");
		cleanup();
		return -EFAULT;
	}
	param.sched_priority = RT_PRIO_UART;
	sched_setscheduler(piDev_g.pUartThread, SCHED_FIFO, &param);
	piDev_g.init_step = 16;

	piDev_g.pIoThread = kthread_run(&piIoThread, NULL, "piIoT");
	if (piDev_g.pIoThread == NULL) {
		pr_err("kthread_run(io) failed\n");
		cleanup();
		return -EFAULT;
	}
	param.sched_priority = RT_PRIO_BRIDGE;
	sched_setscheduler(piDev_g.pIoThread, SCHED_FIFO, &param);
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
	// the IoThread cannot be stopped

	if (piDev_g.init_step >= 17) {
		kthread_stop(piDev_g.pIoThread);
	}
	if (piDev_g.init_step >= 16) {
		kthread_stop(piDev_g.pUartThread);
	}
	if (piDev_g.init_step >= 15) {
		kthread_stop(piDev_g.pGateThread);
	}
	if (piDev_g.init_step >= 14) {
		/* unregister spi */
		BSP_SPI_RWPERI_deinit(0);
	}
	if (piDev_g.init_step >= 13) {
		if (piDev_g.ent != NULL)
			kfree(piDev_g.ent);
		piDev_g.ent = NULL;

		if (piDev_g.devs != NULL)
			kfree(piDev_g.devs);
		piDev_g.devs = NULL;
	}
	if (piDev_g.init_step >= 12) {
		piIoComm_finish();
	}
	/* unregister gpio */
	if (piDev_g.init_step >= 11) {
		piIoComm_writeSniff2B(enGpioValue_Low, enGpioMode_Input);
		gpio_free(GPIO_SNIFF2B);
	}
	if (piDev_g.init_step >= 10) {
		piIoComm_writeSniff2A(enGpioValue_Low, enGpioMode_Input);
		gpio_free(GPIO_SNIFF2A);
	}
	if (piDev_g.init_step >= 9) {
		piIoComm_writeSniff1B(enGpioValue_Low, enGpioMode_Input);
		gpio_free(GPIO_SNIFF1B);
	}
	if (piDev_g.init_step >= 8) {
		piIoComm_writeSniff1A(enGpioValue_Low, enGpioMode_Input);
		gpio_free(GPIO_SNIFF1A);
	}
	if (piDev_g.init_step >= 7) {
		gpio_unexport(GPIO_LED_BRED);
		gpio_free(GPIO_LED_BRED);
	}
	if (piDev_g.init_step >= 6) {
		gpio_unexport(GPIO_LED_BGRN);
		gpio_free(GPIO_LED_BGRN);
	}
	if (piDev_g.init_step >= 5) {
		gpio_unexport(GPIO_LED_ARED);
		gpio_free(GPIO_LED_ARED);
	}
	if (piDev_g.init_step >= 4) {
		gpio_unexport(GPIO_LED_AGRN);
		gpio_free(GPIO_LED_AGRN);
	}
	if (piDev_g.init_step >= 3) {
		gpio_unexport(GPIO_LED_PWRRED);
		gpio_free(GPIO_LED_PWRRED);
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
	}

	if (!IS_ERR_OR_NULL(piControlClass))
		class_destroy(piControlClass);
	unregister_chrdev_region(piControlMajor, 2);

	if (piSpiModule) {
		module_put(piSpiModule);
		piSpiModule = NULL;
	}
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
	if (piDev_g.init_step < 17 || piDev_g.eBridgeState != piBridgeRun) {
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

	timeout /= 100;
	timeout++;

	while (timeout > 0 && piDev_g.eBridgeState != piBridgeRun) {
		msleep(100);
		timeout--;
	}
	if (timeout <= 0)
		return false;

	return true;
}

void printUserMsg(const char *s, ...)
{
	va_list argp;

	va_start(argp, s);

	vsnprintf(pcErrorMessage, sizeof(pcErrorMessage), s, argp);
	pcErrorMessage[sizeof(pcErrorMessage)-1] = 0;	// always add a terminating 0

	pr_info("%s", pcErrorMessage);
}



/*****************************************************************************/
/*              O P E N                                                      */
/*****************************************************************************/
static int piControlOpen(struct inode *inode, struct file *file)
{
	tpiControlInst *priv;

	if (!waitRunning(3000)) {
		pr_err("problem at driver initialization\n");
		return -EINVAL;
	}

	priv = (tpiControlInst *) kzalloc(sizeof(tpiControlInst), GFP_KERNEL);
	file->private_data = priv;

	if (priv == NULL) {
		dev_err(piDev_g.dev, "Private Allocation failed");
		return -EINVAL;
	}

	/* initalize instance variables */
	priv->dev = piDev_g.dev;
	INIT_LIST_HEAD(&priv->piEventList);
	mutex_init(&priv->lockEventList);

	init_waitqueue_head(&priv->wq);

	//dev_info(priv->dev, "piControlOpen");

	piDev_g.PnAppCon++;
	priv->instNum = piDev_g.PnAppCon;

	mutex_lock(&piDev_g.lockListCon);
	list_add(&priv->list, &piDev_g.listCon);
	mutex_unlock(&piDev_g.lockListCon);

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

	//dev_info(priv->dev, "piControlRelease");

	pr_info_drv("close instance %d/%d\n", priv->instNum, piDev_g.PnAppCon);
	piDev_g.PnAppCon--;

	mutex_lock(&piDev_g.lockListCon);
	list_del(&priv->list);
	mutex_unlock(&piDev_g.lockListCon);

	list_for_each_safe(pos, n, &priv->piEventList)
	{
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
	pr_info("piControlRead Count=%u, Pos=%llu: %02x %02x\n", count, *ppos, pPd[0], pPd[1]);
#endif

	rt_mutex_lock(&piDev_g.lockPI);
	if (copy_to_user(pBuf, pPd, nread) != 0) {
		rt_mutex_unlock(&piDev_g.lockPI);
		dev_err(priv->dev, "piControlRead: copy_to_user failed");
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

	rt_mutex_lock(&piDev_g.lockPI);
	if (copy_from_user(pPd, pBuf, nwrite) != 0) {
		rt_mutex_unlock(&piDev_g.lockPI);
		dev_err(priv->dev, "piControlWrite: copy_from_user failed");
		return -EFAULT;
	}
	rt_mutex_unlock(&piDev_g.lockPI);
#ifdef VERBOSE
	pr_info("piControlWrite Count=%u, Pos=%llu: %02x %02x\n", count, *ppos, pPd[0], pPd[1]);
#endif
	*ppos += nwrite;

	return nwrite;		// length written
}


static int piControlReset(tpiControlInst *priv) {
	int status = -EFAULT;
	void *vptr;
	int timeout = 10000;	// ms

	if (piDev_g.ent != NULL)
	{
		vptr = piDev_g.ent;
		piDev_g.ent = NULL;
		kfree(vptr);
	}
	if (piDev_g.devs != NULL)
	{
		vptr = piDev_g.devs;
		piDev_g.devs = NULL;
		kfree(vptr);
	}
	if (piDev_g.cl != NULL)
	{
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

	PiBridgeMaster_Reset();

	if (!waitRunning(timeout)) {
		status = -ETIMEDOUT;
	} else {
		struct list_head *pCon;

		mutex_lock(&piDev_g.lockListCon);
		list_for_each(pCon, &piDev_g.listCon)
		{
			tpiControlInst *pos_inst;
			pos_inst = list_entry(pCon, tpiControlInst, list);
			if (pos_inst != priv)
			{
				struct list_head *pEv;
				tpiEventEntry *pEntry;
				bool found = false;

				// add the event to the list only, if it not already there
				mutex_lock(&pos_inst->lockEventList);
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
					mutex_unlock(&pos_inst->lockEventList);
					pr_info_drv("reset(%d): inform instance %d\n", priv->instNum, pos_inst->instNum);
					wake_up(&pos_inst->wq);
				}
				else
				{
					mutex_unlock(&pos_inst->lockEventList);
				}
			}
		}
		mutex_unlock(&piDev_g.lockListCon);

		status = 0;
	}
	return status;
}

/*****************************************************************************/
/*    I O C T L                                                           */
/*****************************************************************************/
static long piControlIoctl(struct file *file, unsigned int prg_nr, unsigned long usr_addr)
{
	int status = -EFAULT;
	tpiControlInst *priv;
	int timeout = 10000;	// ms

	if (!isRunning())
		return -EAGAIN;

	priv = (tpiControlInst *) file->private_data;

	if (prg_nr != KB_GET_LAST_MESSAGE) {
		// clear old message
		pcErrorMessage[0] = 0;
	}

	switch (prg_nr) {
	case KB_CMD1:
		// do something, copy_from_user ... copy_to_user
		//dev_info(priv->dev, "Got IOCTL 1");
		status = 0;
		break;

	case KB_CMD2:
		// do something, copy_from_user ... copy_to_user
		//dev_info(priv->dev, "Got IOCTL 2");
		status = 0;
		break;

	case KB_RESET:
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
			for (i = 0, found = 0; i < RevPiScan.i8uDeviceCount; i++) {
				if (pDev->i8uAddress != 0 && pDev->i8uAddress == RevPiScan.dev[i].i8uAddress) {
					found = 1;
				}
				if (pDev->i16uModuleType != 0
				    && pDev->i16uModuleType == RevPiScan.dev[i].sId.i16uModulType) {
					found = 1;
				}
				if (found) {
					pDev->i8uAddress = RevPiScan.dev[i].i8uAddress;
					pDev->i8uActive = RevPiScan.dev[i].i8uActive;
					pDev->i32uSerialnumber = RevPiScan.dev[i].sId.i32uSerialnumber;
					pDev->i16uModuleType = RevPiScan.dev[i].sId.i16uModulType;
					pDev->i16uHW_Revision = RevPiScan.dev[i].sId.i16uHW_Revision;
					pDev->i16uSW_Major = RevPiScan.dev[i].sId.i16uSW_Major;
					pDev->i16uSW_Minor = RevPiScan.dev[i].sId.i16uSW_Minor;
					pDev->i32uSVN_Revision = RevPiScan.dev[i].sId.i32uSVN_Revision;
					pDev->i16uInputLength = RevPiScan.dev[i].sId.i16uFBS_InputLength;
					pDev->i16uInputOffset = RevPiScan.dev[i].i16uInputOffset;
					pDev->i16uOutputLength = RevPiScan.dev[i].sId.i16uFBS_OutputLength;
					pDev->i16uOutputOffset = RevPiScan.dev[i].i16uOutputOffset;
					pDev->i16uConfigLength = RevPiScan.dev[i].i16uConfigLength;
					pDev->i16uConfigOffset = RevPiScan.dev[i].i16uConfigOffset;
					if (piDev_g.i8uRightMGateIdx == i)
						pDev->i8uModuleState = MODGATECOM_GetOtherFieldbusState(0);
					if (piDev_g.i8uLeftMGateIdx == i)
						pDev->i8uModuleState = MODGATECOM_GetOtherFieldbusState(1);
					return i;
				}
			}
		}
		break;

	case KB_GET_DEVICE_INFO_LIST:
		{
			SDeviceInfo *pDev = (SDeviceInfo *) usr_addr;
			int i;
			for (i = 0; i < RevPiScan.i8uDeviceCount; i++) {
				pDev[i].i8uAddress = RevPiScan.dev[i].i8uAddress;
				pDev[i].i8uActive = RevPiScan.dev[i].i8uActive;
				pDev[i].i32uSerialnumber = RevPiScan.dev[i].sId.i32uSerialnumber;
				pDev[i].i16uModuleType = RevPiScan.dev[i].sId.i16uModulType;
				pDev[i].i16uHW_Revision = RevPiScan.dev[i].sId.i16uHW_Revision;
				pDev[i].i16uSW_Major = RevPiScan.dev[i].sId.i16uSW_Major;
				pDev[i].i16uSW_Minor = RevPiScan.dev[i].sId.i16uSW_Minor;
				pDev[i].i32uSVN_Revision = RevPiScan.dev[i].sId.i32uSVN_Revision;
				pDev[i].i16uInputLength = RevPiScan.dev[i].sId.i16uFBS_InputLength;
				pDev[i].i16uInputOffset = RevPiScan.dev[i].i16uInputOffset;
				pDev[i].i16uOutputLength = RevPiScan.dev[i].sId.i16uFBS_OutputLength;
				pDev[i].i16uOutputOffset = RevPiScan.dev[i].i16uOutputOffset;
				pDev[i].i16uConfigLength = RevPiScan.dev[i].i16uConfigLength;
				pDev[i].i16uConfigOffset = RevPiScan.dev[i].i16uConfigOffset;
				if (piDev_g.i8uRightMGateIdx == i)
					pDev[i].i8uModuleState = MODGATECOM_GetOtherFieldbusState(0);
				if (piDev_g.i8uLeftMGateIdx == i)
					pDev[i].i8uModuleState = MODGATECOM_GetOtherFieldbusState(1);
			}
			status = RevPiScan.i8uDeviceCount;
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
				rt_mutex_lock(&piDev_g.lockPI);
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

#ifdef VERBOSE
				pr_info("piControlIoctl Addr=%u, bit=%u: %02x %02x\n",
				     pValue->i16uAddress, pValue->i8uBit, pValue->i8uValue, i8uValue_l);
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

			rt_mutex_lock(&piDev_g.lockPI);
			piDev_g.tLastOutput2 = piDev_g.tLastOutput1;
			piDev_g.tLastOutput1 = now;

			for (i = 0; i < piDev_g.cl->i16uNumEntries; i++) {
				uint16_t len = piDev_g.cl->ent[i].i16uLength;
				uint16_t addr = piDev_g.cl->ent[i].i16uAddr;

				if (len >= 8) {
					len /= 8;
#if 0
					if (memcmp(piDev_g.ai8uPI + addr, (void *)(usr_addr + addr), len))
						pr_info("piControlIoctl copy %d bytes at offset %d: %x %x %x %x\n", len, addr,
						       ((unsigned char *)(usr_addr + addr))[0],
							((unsigned char *)(usr_addr + addr))[1],
							((unsigned char *)(usr_addr + addr))[2],
							((unsigned char *)(usr_addr + addr))[3]);
#endif
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
			}
			rt_mutex_unlock(&piDev_g.lockPI);
		}
		break;

	case KB_DIO_RESET_COUNTER:
		{
			SDioCounterReset tel;
			SDIOResetCounter *pPar = (SDIOResetCounter *)usr_addr;
			int i;
			bool found = false;

			if (!isRunning())
				return -EFAULT;

			for (i = 0; i < RevPiScan.i8uDeviceCount && !found; i++) {
				if (RevPiScan.dev[i].i8uAddress == pPar->i8uAddress
				    && RevPiScan.dev[i].i8uActive
				    && (RevPiScan.dev[i].sId.i16uModulType == KUNBUS_FW_DESCR_TYP_PI_DIO_14
				     || RevPiScan.dev[i].sId.i16uModulType == KUNBUS_FW_DESCR_TYP_PI_DI_16))
				{
					found = true;
					break;
				}
			}
			if (!found || pPar->i16uBitfield == 0)
			{
				pr_info("piControlIoctl: resetCounter failed bitfield 0x%x", pPar->i16uBitfield);
				return -EINVAL;
			}

			tel.uHeader.sHeaderTyp1.bitAddress = pPar->i8uAddress;
			tel.uHeader.sHeaderTyp1.bitIoHeaderType = 0;
			tel.uHeader.sHeaderTyp1.bitReqResp = 0;
			tel.uHeader.sHeaderTyp1.bitLength = sizeof(SDioCounterReset) - IOPROTOCOL_HEADER_LENGTH - 1;
			tel.uHeader.sHeaderTyp1.bitCommand = IOP_TYP1_CMD_DATA3;
			tel.i16uChannels = pPar->i16uBitfield;

#if 0
			mutex_lock(&piDev_g.lockUserTel);
			if (copy_from_user(&piDev_g.requestUserTel, &tel, sizeof(tel)))
			{
				mutex_unlock(&piDev_g.lockUserTel);
				pr_info("piControlIoctl: resetCounter failed copy");
				status = -EFAULT;
				break;
			}
#else
			mutex_lock(&piDev_g.lockUserTel);
			memcpy(&piDev_g.requestUserTel, &tel, sizeof(tel));
#endif
			piDev_g.pendingUserTel = true;
			down(&piDev_g.semUserTel);
			status = piDev_g.statusUserTel;
			pr_info("piControlIoctl: resetCounter result %d", status);
			mutex_unlock(&piDev_g.lockUserTel);
		}
		break;

	case KB_INTERN_SET_SERIAL_NUM:
		{
			INT32U *pData = (INT32U *) usr_addr;	// pData is an array containing the module address and the serial number

			if (!isRunning())
				return -EFAULT;

			PiBridgeMaster_Stop();
			msleep(500);

			if (PiBridgeMaster_FWUModeEnter(pData[0], 1) == 0) {

				if (PiBridgeMaster_FWUsetSerNum(pData[1]) == 0) {
					pr_info("piControlIoctl: set serial number to %u in module %u",
					     pData[1], pData[0]);
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

			if (!isRunning()) {
				printUserMsg("piControl is not running");
				return -EFAULT;
			}

			// at the moment the update works only, if there is only one module connected on the right
#ifndef ENDTEST_DIO
			if (RevPiScan.i8uDeviceCount > 2)	// RevPi + Module to update
			{
				printUserMsg("Too many modules! Firmware update is only possible, if there is exactly one module on the right of the RevPi Core.");
				return -EFAULT;
			}
#endif
			if (RevPiScan.i8uDeviceCount < 2
			    || RevPiScan.dev[1].i8uActive == 0
			    || RevPiScan.dev[1].sId.i16uModulType >= PICONTROL_SW_OFFSET)
			{
				printUserMsg("No module to update! Firmware update is only possible, if there is exactly one module on the right of the RevPi Core.");
				return -EFAULT;
			}

			PiBridgeMaster_Stop();
			msleep(50);

			cnt = 0;
			for (i = 0; i < RevPiScan.i8uDeviceCount; i++) {
				if (pData != 0 && RevPiScan.dev[i].i8uAddress != *pData)
				{
					// if pData is not 0, we want to update one specific module
					// -> update all others
					pr_info("skip %d addr %d\n", i, RevPiScan.dev[i].i8uAddress);
				}
				else
				{
					ret = FWU_update(&RevPiScan.dev[i]);
					pr_info("update %d addr %d ret %d\n", i, RevPiScan.dev[i].i8uAddress, ret);
					if (ret > 0) {
						cnt++;
						// update only one device per call
						break;
					} else {
						status = cnt;
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
			SIOGeneric *tel = (SIOGeneric *)usr_addr;

			if (!isRunning())
				return -EFAULT;

			mutex_lock(&piDev_g.lockUserTel);
			if (copy_from_user(&piDev_g.requestUserTel, tel, sizeof(SIOGeneric)))
			{
				mutex_unlock(&piDev_g.lockUserTel);
				status = -EFAULT;
				break;
			}
			piDev_g.pendingUserTel = true;
			down(&piDev_g.semUserTel);
			status = piDev_g.statusUserTel;
			if (copy_to_user(tel, &piDev_g.responseUserTel, sizeof(SIOGeneric)))
			{
				mutex_unlock(&piDev_g.lockUserTel);
				status = -EFAULT;
				break;
			}
			mutex_unlock(&piDev_g.lockUserTel);
		}
		break;

	case KB_WAIT_FOR_EVENT:
		{
			tpiEventEntry *pEntry;
			int *pData = (int *) usr_addr;

			pr_info_drv("wait(%d)\n", priv->instNum);
			if (wait_event_interruptible(priv->wq, !list_empty(&priv->piEventList)) == 0) {
				mutex_lock(&priv->lockEventList);
				pEntry = list_first_entry(&priv->piEventList, tpiEventEntry, list);

				pr_info_drv("wait(%d): got event %d\n", priv->instNum, pEntry->event);
				*pData = pEntry->event;

				list_del(&pEntry->list);
				mutex_unlock(&priv->lockEventList);
				kfree(pEntry);

				status = 0;
			}
		}
		break;

	case KB_GET_LAST_MESSAGE:
		{
			if (copy_to_user((void*)usr_addr, pcErrorMessage, sizeof(pcErrorMessage)))
			{
				dev_err(priv->dev, "piControlIoctl: copy_to_user failed");
				return -EFAULT;
			}
			status = 0;
		}
		break;
	default:
		dev_err(priv->dev, "Invalid Ioctl");
		return (-EINVAL);
		break;

	}

	return status;
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

module_init(piControlInit);
module_exit(piControlCleanup);

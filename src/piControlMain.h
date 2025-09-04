/* SPDX-License-Identifier: GPL-2.0-only
 * SPDX-FileCopyrightText: 2016-2024 KUNBUS GmbH
 */

#ifndef PRODUCTS_PIBASE_PIKERNELMOD_PICONTROLINTERN_H_
#define PRODUCTS_PIBASE_PIKERNELMOD_PICONTROLINTERN_H_
/******************************************************************************/
/********************************  Includes  **********************************/
/******************************************************************************/
#include <linux/cdev.h>
#include <linux/leds.h>

#include "common_define.h"
#include "piConfig.h"
#include "project.h"
/******************************************************************************/
/*********************************  Types  ************************************/
/******************************************************************************/
typedef enum piEvent {
	piEvReset = 1,
} enPiEvent;

enum revpi_machine {
	REVPI_CORE = 1,
	REVPI_COMPACT = 2,
	REVPI_CONNECT = 3,
	REVPI_FLAT = 4,
	REVPI_CONNECT_SE = 5,
	REVPI_CORE_SE = 6,
	REVPI_CONNECT_4 = 7,
	REVPI_CONNECT_5 = 8,
	REVPI_GENERIC_PB = 255,
};

enum {
	REVPI_PIBRIDGE_ETHERNET_GPIO_MPX = 0,
	REVPI_PIBRIDGE_ETHERNET_GPIO_DETECT
};

#define PICONTROL_CYCLE_MAX_DURATION    45000 /* usecs */
struct picontrol_cycle {
	unsigned int duration;
	unsigned int last;
	unsigned int max;
	unsigned int min;
	seqlock_t lock;
};

typedef struct spiControlDev {
	// device driver stuff
	enum revpi_machine machine_type;
	void *machine;
	struct cdev cdev;	// Char device structure
	struct device *dev;
	struct thermal_zone_device *thermal_zone;

	// supports extension modules with RS485 based communication (eg. DIO or AIO)
	unsigned int pibridge_supported:1;
	// device is only equipped on the left side with a PiBridge connector (eg. RevPi Connect)
	unsigned int only_left_pibridge:1;
	// device supports RevPi gateways (only point to point communication)
	unsigned int revpi_gate_supported:1;

	// process image stuff
	INT8U ai8uPI[KB_PI_LEN];
	INT8U ai8uPIDefault[KB_PI_LEN];
	struct rt_mutex lockPI;
#define PICONTROL_DEV_FLAG_STOP_IO		(1 << 0)
	unsigned long flags;
	piDevices *devs;
	piEntries *ent;
	piCopylist *cl;
	/* Protect internal resources, like devs, ent, cl, etc. during
	   execution of ioctls. This is especially needed during reset. */
	struct rt_mutex lockIoctl;
	piConnectionList *connl;
	ktime_t tLastOutput1, tLastOutput2;

	// handle open connections and notification
	struct list_head listCon;
	struct rt_mutex lockListCon;

	struct led_trigger power_red;
	struct led_trigger a1_green;
	struct led_trigger a1_red;
	struct led_trigger a2_green;
	struct led_trigger a2_red;
	struct led_trigger a3_green;
	struct led_trigger a3_red;
	/* Revpi Flat only */
	struct led_trigger a4_green;
	struct led_trigger a4_red;
	struct led_trigger a5_green;
	struct led_trigger a5_red;
	/* RevPi Connect 4, 5 */
	struct led_trigger a1_blue;
	struct led_trigger a2_blue;
	struct led_trigger a3_blue;
	struct led_trigger a4_blue;
	struct led_trigger a5_blue;
	/* Gigabit ethernet on PiBridge */
	bool pibridge_mode_ethernet_left;
	bool pibridge_mode_ethernet_right;
	/* PiControl cycle attributes */
	struct picontrol_cycle cycle;
} tpiControlDev;

typedef struct spiEventEntry {
	enum piEvent event;
	struct list_head list;
} tpiEventEntry;

typedef struct spiControlInst {
	struct device *dev;
	wait_queue_head_t wq;
	struct list_head piEventList;	// head of the event list for this instance
	struct rt_mutex lockEventList;
	struct list_head list;	// list of all instances
	ktime_t tTimeoutTS;	// time stamp when the output must be set to 0
	unsigned long tTimeoutDurationMs;	// length of the timeout in ms, 0 if not active
	char pcErrorMessage[REV_PI_ERROR_MSG_LEN];	// error message of last ioctl call
} tpiControlInst;

extern tpiControlDev piDev_g;

/******************************************************************************/
/*******************************  Prototypes  *********************************/
/******************************************************************************/

bool isRunning(void);
void printUserMsg(tpiControlInst *priv, const char *s, ...);
unsigned int piControl_get_cycle_duration(void);

#endif /* PRODUCTS_PIBASE_PIKERNELMOD_PICONTROLINTERN_H_ */

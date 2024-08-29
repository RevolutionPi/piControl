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
	REVPI_GENERIC_PB = 255,
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
	u8 PnAppCon;		// counter of open connections
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
	/* RevPi Connect 4 only */
	struct led_trigger a1_blue;
	struct led_trigger a2_blue;
	struct led_trigger a3_blue;
	struct led_trigger a4_blue;
	struct led_trigger a5_blue;
} tpiControlDev;

typedef struct spiEventEntry {
	enum piEvent event;
	struct list_head list;
} tpiEventEntry;

typedef struct spiControlInst {
	u8 instNum;		// number of instance
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

#endif /* PRODUCTS_PIBASE_PIKERNELMOD_PICONTROLINTERN_H_ */

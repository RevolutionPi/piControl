/* SPDX-License-Identifier: GPL-2.0-only
 * SPDX-FileCopyrightText: 2018-2024 KUNBUS GmbH
 */

#ifndef PICONTROL_INTERN_H
#define PICONTROL_INTERN_H

#include "piControl.h"
#include "IoProtocol.h"
#include <linux/ioctl.h>
#include <linux/types.h>

#define PICONFIG_FILE					"/etc/revpi/config.rsc"
/* address of first module on the right side of the RevPi Core */
#define REV_PI_DEV_FIRST_RIGHT				32
#define PICONTROL_FIRMWARE_FORCE_UPLOAD			0x0001

// the following call are for KUNBUS internal use only.
/* set serial num in piDIO, piDI or piDO (can be made only once) */
#define  KB_INTERN_SET_SERIAL_NUM			_IO(KB_IOC_MAGIC, 100 )
/* 'Hidden' structure that can be used by userspace for internal io telegrams */
struct io_telegram {
	__u8 dest      :6;
	__u8 type      :1;   /* must be 0 for header type 1 */
	__u8 response  :1;   /* 0 for request, 1 for response */
	__u8 datalen   :5;
	__u8 command   :3;   /* 0 for broadcast*/
	__u8 data[IOPROTOCOL_MAXDATA_LENGTH + 1];
} __attribute__((__packed__));

struct io_telegram2 {
	__u8 command   :6;
	__u8 type      :1;   /* must be 1 for header type 2 */
	__u8 response  :1;   /* 0 for request, 1 for response */
	__u8 datalen   :5;
	__u8 dpart1    :3;
	__u8 data[IOPROTOCOL_MAXDATA_LENGTH + 1];
} __attribute__((__packed__));

/* send an I/O-Protocol message and return response */
#define  KB_INTERN_IO_MSG				_IO(KB_IOC_MAGIC, 101 )
/* 'Hidden' structure that can be used by userspace for internal gateway telegrams */
struct modgate_telegram {
	__u8 dest;
	__u8 src;
	__u16 command;
	__u16 sequence;
	__u8 datalen;
	__u8 data[MAX_TELEGRAM_DATA_SIZE];
} __attribute__((__packed__));

/* send a Gateway-Protocol message and return response */
#define  KB_INTERN_GATE_MSG				_IO(KB_IOC_MAGIC, 102)

typedef struct SEntryInfoStr
{
	/* Address of module in current configuration */
	u8 i8uAddress;
#define ENTRY_INFO_TYPE_INPUT				1
#define ENTRY_INFO_TYPE_OUTPUT				2
#define ENTRY_INFO_TYPE_MEMORY				3
#define ENTRY_INFO_TYPE_MASK				0x7F
	/* + 0x80 if exported */
	u8 i8uType;
	/* index of I/O value for this module */
	u16 i16uIndex;
	/* length of value in bits */
	u16 i16uBitLength;
	/* 0-7 bit position, 0 also for whole byte */
	u8 i8uBitPos;
	/* offset in process image */
	u16 i16uOffset;
	/* default value */
	u32 i32uDefault;
	/* Variable name */
	char strVarName[32];
} SEntryInfo;


#define PICONTROL_CONFIG_ERROR_WRONG_MODULE_TYPE	-10
#define PICONTROL_CONFIG_ERROR_WRONG_INPUT_LENGTH	-11
#define PICONTROL_CONFIG_ERROR_WRONG_OUTPUT_LENGTH	-12
#define PICONTROL_CONFIG_ERROR_WRONG_CONFIG_LENGTH	-13
#define PICONTROL_CONFIG_ERROR_WRONG_INPUT_OFFSET	-14
#define PICONTROL_CONFIG_ERROR_WRONG_OUTPUT_OFFSET	-15
#define PICONTROL_CONFIG_ERROR_WRONG_CONFIG_OFFSET	-16

#define PICONTROL_STATUS_RUNNING			0x01
#define PICONTROL_STATUS_EXTRA_MODULE			0x02
#define PICONTROL_STATUS_MISSING_MODULE			0x04
#define PICONTROL_STATUS_SIZE_MISMATCH			0x08
#define PICONTROL_STATUS_LEFT_GATEWAY			0x10
#define PICONTROL_STATUS_RIGHT_GATEWAY			0x20
/* RevPi Connect only */
#define PICONTROL_STATUS_X2_DIN				0x40
#define PICONTROL_LED_A1_GREEN				0x0001
#define PICONTROL_LED_A1_RED				0x0002
#define PICONTROL_LED_A2_GREEN				0x0004
#define PICONTROL_LED_A2_RED				0x0008
/* Revpi Connect and Flat */
#define PICONTROL_LED_A3_GREEN				0x0010
#define PICONTROL_LED_A3_RED				0x0020
/* RevPi Connect only */
#define PICONTROL_X2_DOUT				0x0040
#define PICONTROL_X2_DOUT_CONNECT4			0x0001
#define PICONTROL_WD_TRIGGER				0x0080
/* Revpi Flat only */
#define PICONTROL_LED_A4_GREEN				0x0040
#define PICONTROL_LED_A4_RED				0x0080
#define PICONTROL_LED_A5_GREEN				0x0100
#define PICONTROL_LED_A5_RED				0x0200
/* RevPi Connect 4, 5 */
#define PICONTROL_LED_RGB_A1_RED			0x0001
#define PICONTROL_LED_RGB_A1_GREEN			0x0002
#define PICONTROL_LED_RGB_A1_BLUE			0x0004
#define PICONTROL_LED_RGB_A2_RED			0x0008
#define PICONTROL_LED_RGB_A2_GREEN			0x0010
#define PICONTROL_LED_RGB_A2_BLUE			0x0020
#define PICONTROL_LED_RGB_A3_RED			0x0040
#define PICONTROL_LED_RGB_A3_GREEN			0x0080
#define PICONTROL_LED_RGB_A3_BLUE			0x0100
#define PICONTROL_LED_RGB_A4_RED			0x0200
#define PICONTROL_LED_RGB_A4_GREEN			0x0400
#define PICONTROL_LED_RGB_A4_BLUE			0x0800
#define PICONTROL_LED_RGB_A5_RED			0x1000
#define PICONTROL_LED_RGB_A5_GREEN			0x2000
#define PICONTROL_LED_RGB_A5_BLUE			0x4000

#endif /* PICONTROL_INTERN_H */

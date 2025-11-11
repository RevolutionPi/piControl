/* SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2016-2025 KUNBUS GmbH
 */

#ifndef PICONTROL_H_
#define PICONTROL_H_

#include <linux/types.h>

#define PICONTROL_DEVICE			"/dev/piControl0"
/* max. length of error message */
#define REV_PI_ERROR_MSG_LEN			256
/* max. number of */
#define REV_PI_DEV_CNT_MAX			64

/*
 *  Module Id
 * 0x0001 - 0x3000      KUNBUS Modules (e.g. DIO, Gateways, ...)
 * 0x3001 - 0x7000      not used
 * 0x6001 - 0x7000      KUNBUS Software Adapters
 * 0x7001 - 0x8000      User defined Software Adapters
 * 0x8001 - 0xb000      KUNBUS Modules configured but not connected
 * 0xb001 - 0xffff      not used
 */
#define PICONTROL_SW_OFFSET			0x6001
#define PICONTROL_SW_MODBUS_TCP_SLAVE		0x6001
#define PICONTROL_SW_MODBUS_RTU_SLAVE		0x6002
#define PICONTROL_SW_MODBUS_TCP_MASTER		0x6003
#define PICONTROL_SW_MODBUS_RTU_MASTER		0x6004
#define PICONTROL_SW_PROFINET_CONTROLLER    	0x6005
#define PICONTROL_SW_PROFINET_DEVICE		0x6006
#define PICONTROL_SW_REVPI_SEVEN		0x6007
#define PICONTROL_SW_REVPI_CLOUD		0x6008
#define PICONTROL_SW_OPCUA_REVPI_SERVER		0x6009
#define PICONTROL_SW_MQTT_REVPI_CLIENT		0x600a

#define PICONTROL_NOT_CONNECTED			0x8000
#define PICONTROL_NOT_CONNECTED_MASK		0x7fff

struct picontrol_firmware_upload {
	__u32 addr;
/* upload regardless of FW version */
#define PICONTROL_FIRMWARE_FORCE_UPLOAD		0x0001
/* do firmware upload in module rescue mode */
#define PICONTROL_FIRMWARE_RESCUE_MODE		0x0002 /* no FWU MODE switch */
	__u32 flags;
	__u8 rescue_mode_hw_revision;
	/* Memory is cheap, so reserve a few bytes for future extensions */
	__u8 padding[15];
};

typedef struct SDeviceInfoStr {
	/* Address of module in current configuration */
	__u8 i8uAddress;
	__u8 pad[3];
	/* serial number of module */
	__u32 i32uSerialnumber;
	/* Type identifier of module */
	__u16 i16uModuleType;
	/* hardware revision */
	__u16 i16uHW_Revision;
	/* major software version */
	__u16 i16uSW_Major;
	/* minor software version */
	__u16 i16uSW_Minor;
	/* svn revision of software */
	__u32 i32uSVN_Revision;
	/* length in bytes of all input values together */
	__u16 i16uInputLength;
	/* length in bytes of all output values together */
	__u16 i16uOutputLength;
	/* length in bytes of all config values together */
	__u16 i16uConfigLength;
	/* offset in process image */
	__u16 i16uBaseOffset;
	/* offset in process image of first input byte */
	__u16 i16uInputOffset;
	/* offset in process image of first output byte */
	__u16 i16uOutputOffset;
	/* offset in process image of first config byte */
	__u16 i16uConfigOffset;
	/* index of entry */
	__u16 i16uFirstEntry;
	/* number of entries in process image */
	__u16 i16uEntries;
	/* fieldbus state of piGate Module */
	__u8 i8uModuleState;
	/* 0 means that the module is not present and no data is available */
	__u8 i8uActive;
	/* space for future extensions */
	__u8 i8uReserve[30];		
} SDeviceInfo;

typedef struct SPIValueStr {
	/* Address of the byte in the process image */
	__u16 i16uAddress;
	/* 0-7 bit position, >= 8 whole byte */
	__u8 i8uBit;
	/* Value: 0/1 for bit access, whole byte otherwise */
	__u8 i8uValue;
} SPIValue;

typedef struct SPIVariableStr {
	/* Variable name */
	char strVarName[32];
	/* Address of the byte in the process image */
	__u16 i16uAddress;
	/* 0-7 bit position, >= 8 whole byte */
	__u8 i8uBit;
	__u8 pad;
	/* length in bits, possible values are 1, 8, 16 and 32 */
	__u16 i16uLength;		
} SPIVariable;

#define KB_IOC_MAGIC  'K'
/* reset the piControl driver including the config file */
#define  KB_RESET				_IO(KB_IOC_MAGIC, 12 )
/* get the device info of all detected devices */
#define  KB_GET_DEVICE_INFO_LIST		_IO(KB_IOC_MAGIC, 13 )
/* get the device info of one device */
#define  KB_GET_DEVICE_INFO			_IO(KB_IOC_MAGIC, 14 )
/* get the value of one bit in the process image */
#define  KB_GET_VALUE				_IO(KB_IOC_MAGIC, 15 )
/* set the value of one bit in the process image */
#define  KB_SET_VALUE				_IO(KB_IOC_MAGIC, 16 )
/* find a varible defined in PiCtory */
#define  KB_FIND_VARIABLE			_IO(KB_IOC_MAGIC, 17 )
/* copy exported outputs from application to the process image */
#define  KB_SET_EXPORTED_OUTPUTS		_IO(KB_IOC_MAGIC, 18 )
/* Deprecated. Use PICONTROL_UPLOAD_FIRMWARE instead. */
#define  KB_UPDATE_DEVICE_FIRMWARE		_IO(KB_IOC_MAGIC, 19 )
/* set a counter or endocder to 0 */
#define  KB_DIO_RESET_COUNTER			_IO(KB_IOC_MAGIC, 20 )
/* copy the last error message */
#define  KB_GET_LAST_MESSAGE			_IO(KB_IOC_MAGIC, 21 )
/* stop/start IO communication, can be used for I/O simulation */
#define  KB_STOP_IO				_IO(KB_IOC_MAGIC, 22 )
/* For download of configuration to Master Gateway: stop IO communication
 * completely.
 */
#define  KB_CONFIG_STOP				_IO(KB_IOC_MAGIC, 23 )
/* for download of configuration to Master Gateway: download config data */
#define  KB_CONFIG_SEND				_IO(KB_IOC_MAGIC, 24 )
/* for download of configuration to Master Gateway: restart IO communication */
#define  KB_CONFIG_START			_IO(KB_IOC_MAGIC, 25 )
/* Activate a watchdog. If write is not called for a given period all outputs
 * are set to 0.
 */
#define  KB_SET_OUTPUT_WATCHDOG			_IO(KB_IOC_MAGIC, 26 )
/* set the f_pos, the unsigned int * is used to interpret the pos value */
#define  KB_SET_POS				_IO(KB_IOC_MAGIC, 27 )
#define  KB_AIO_CALIBRATE			_IO(KB_IOC_MAGIC, 28 )
/* get counter values of a RO module */
#define  KB_RO_GET_COUNTER			_IO(KB_IOC_MAGIC, 29 )

/* wait for an event. This call is normally blocking */
#define  KB_WAIT_FOR_EVENT			_IO(KB_IOC_MAGIC, 50 )
/* piControl was reset, reload configuration */
#define  KB_EVENT_RESET				1

/* new ioctl to upload firmware */
#define PICONTROL_UPLOAD_FIRMWARE		_IOW(KB_IOC_MAGIC, 200, struct picontrol_firmware_upload )

typedef struct SDIOResetCounterStr {
	/* Address of module in current configuration */
	__u8 i8uAddress;
	__u8 pad;
	/* bitfield, if bit n is 1, reset counter/encoder on input */
	__u16 i16uBitfield;
} SDIOResetCounter;

enum revpi_ro_num {
	RELAY_1 = 0,
	RELAY_2,
	RELAY_3,
	RELAY_4,
	REVPI_RO_NUM_RELAYS,
};

/* Data for KB_RO_GET_COUNTER ioctl */
struct revpi_ro_ioctl_counters {
	/* Address of module in current configuration, set by userspace. */
	__u8 addr;
	/* Data returned from kernel */
	__u32 counter[REVPI_RO_NUM_RELAYS];
} __attribute__((__packed__));

#define REVPI_RO_RELAY_1_BIT			BIT(0)
#define REVPI_RO_RELAY_2_BIT			BIT(1)
#define REVPI_RO_RELAY_3_BIT			BIT(2)
#define REVPI_RO_RELAY_4_BIT			BIT(3)

struct pictl_calibrate {
	/* Address of module in current configuration */
	__u8 address;
	/* bitfield: mode */
	__u8 mode;
	/* channels to calibrate */
	__u8 channels;
	/* point in lookupTable */
	__u8 x_val;
	__s16 y_val;
};

#define MAX_TELEGRAM_DATA_SIZE			255

typedef struct SConfigDataStr {
	__u8 bLeft;
	__u8 pad;
	__u16 i16uLen;
	__u8 acData[MAX_TELEGRAM_DATA_SIZE];
} SConfigData;

#endif /* PICONTROL_H_ */

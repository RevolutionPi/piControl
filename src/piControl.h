/* SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2016-2024 KUNBUS GmbH
 */

#ifndef PICONTROL_H_
#define PICONTROL_H_

/******************************************************************************/
/********************************  Includes  **********************************/
/******************************************************************************/
#ifndef WIN32
#include <linux/ioctl.h>
#include <linux/types.h>
#endif //WIN32

//#include <stdint.h>

/******************************************************************************/
/*********************************  Types  ************************************/
/******************************************************************************/
#ifndef WIN32

#define PICONTROL_DEVICE        "/dev/piControl0"

// The config file moved to /etc/revpi.
// If it cannot be found there, the old location should be used.
#define PICONFIG_FILE           "/etc/revpi/config.rsc"
#define PICONFIG_FILE_WHEEZY    "/opt/KUNBUS/config.rsc"

#define REV_PI_DEV_FIRST_RIGHT      32      // address of first module on the right side of the RevPi Core
#define REV_PI_DEV_CNT_MAX          64      // max. number of
#define REV_PI_ERROR_MSG_LEN        256     // max. length of error message

// Module Id
// 0x0001 - 0x3000      KUNBUS Modules (e.g. DIO, Gateways, ...)
// 0x3001 - 0x7000      not used
// 0x6001 - 0x7000      KUNBUS Software Adapters
// 0x7001 - 0x8000      User defined Software Adapters
// 0x8001 - 0xb000      KUNBUS Modules configured but not connected
// 0xb001 - 0xffff      not used

#define PICONTROL_SW_OFFSET                 0x6001      // 24577
#define PICONTROL_SW_MODBUS_TCP_SLAVE       0x6001      // 24577
#define PICONTROL_SW_MODBUS_RTU_SLAVE       0x6002      // 24578
#define PICONTROL_SW_MODBUS_TCP_MASTER      0x6003      // 24579
#define PICONTROL_SW_MODBUS_RTU_MASTER      0x6004      // 24580
#define PICONTROL_SW_PROFINET_CONTROLLER    0x6005      // 24581
#define PICONTROL_SW_PROFINET_DEVICE        0x6006      // 24582
#define PICONTROL_SW_REVPI_SEVEN            0x6007      // 24583
#define PICONTROL_SW_REVPI_CLOUD            0x6008      // 24584
#define PICONTROL_SW_OPCUA_REVPI_SERVER     0x6009      // 24585
#define PICONTROL_SW_MQTT_REVPI_CLIENT      0x6010      // 24586

#define PICONTROL_NOT_CONNECTED             0x8000
#define PICONTROL_NOT_CONNECTED_MASK        0x7fff

#define PICONTROL_USER_MODULE_TYPE          0x8000  // old definition, will be removed soon
#define PICONTROL_USER_MODULE_MASK          0x7fff  // old definition, will be removed soon

#define PICONTROL_FIRMWARE_FORCE_UPLOAD     0x0001

struct picontrol_firmware_upload {
	__u32 addr;
	__u32 flags;
	/* Memory is cheap, so reserve a few bytes for future extensions */
	__u32 padding[4];
};

#define KB_IOC_MAGIC  'K'
#define  KB_CMD1                            _IO(KB_IOC_MAGIC, 10 )  // for test only
#define  KB_CMD2                            _IO(KB_IOC_MAGIC, 11 )  // for test only
#define  KB_RESET                           _IO(KB_IOC_MAGIC, 12 )  // reset the piControl driver including the config file
#define  KB_GET_DEVICE_INFO_LIST            _IO(KB_IOC_MAGIC, 13 )  // get the device info of all detected devices
#define  KB_GET_DEVICE_INFO                 _IO(KB_IOC_MAGIC, 14 )  // get the device info of one device
#define  KB_GET_VALUE                       _IO(KB_IOC_MAGIC, 15 )  // get the value of one bit in the process image
#define  KB_SET_VALUE                       _IO(KB_IOC_MAGIC, 16 )  // set the value of one bit in the process image
#define  KB_FIND_VARIABLE                   _IO(KB_IOC_MAGIC, 17 )  // find a variable defined in PiCtory
#define  KB_SET_EXPORTED_OUTPUTS            _IO(KB_IOC_MAGIC, 18 )  // copy the exported outputs from a application process image to the real process image
/* Deprecated. Use PICONTROL_UPLOAD_FIRMWARE instead. */
#define  KB_UPDATE_DEVICE_FIRMWARE          _IO(KB_IOC_MAGIC, 19 )  // try to update the firmware of connected devices
#define  KB_DIO_RESET_COUNTER               _IO(KB_IOC_MAGIC, 20 )  // set a counter or endocder to 0
#define  KB_GET_LAST_MESSAGE                _IO(KB_IOC_MAGIC, 21 )  // copy the last error message
#define  KB_STOP_IO                         _IO(KB_IOC_MAGIC, 22 )  // stop/start IO communication, can be used for I/O simulation
#define  KB_CONFIG_STOP                     _IO(KB_IOC_MAGIC, 23 )  // for download of configuration to Master Gateway: stop IO communication completely
#define  KB_CONFIG_SEND                     _IO(KB_IOC_MAGIC, 24 )  // for download of configuration to Master Gateway: download config data
#define  KB_CONFIG_START                    _IO(KB_IOC_MAGIC, 25 )  // for download of configuration to Master Gateway: restart IO communication
#define  KB_SET_OUTPUT_WATCHDOG             _IO(KB_IOC_MAGIC, 26 )  // activate a watchdog for this handle. If write is not called for a given period all outputs are set to 0
#define  KB_SET_POS                         _IO(KB_IOC_MAGIC, 27 )  // set the f_pos, the unsigned int * is used to interpret the pos value
#define  KB_AIO_CALIBRATE                   _IO(KB_IOC_MAGIC, 28 )
#define  KB_RO_GET_COUNTER                  _IO(KB_IOC_MAGIC, 29 )  // get counter values of a RO module

#define  KB_WAIT_FOR_EVENT                  _IO(KB_IOC_MAGIC, 50 )  // wait for an event. This call is normally blocking
#define  KB_EVENT_RESET                     1       // piControl was reset, reload configuration

// the following call are for KUNBUS internal use only.
#define  KB_INTERN_SET_SERIAL_NUM           _IO(KB_IOC_MAGIC, 100 )  // set serial num in piDIO, piDI or piDO (can be made only once)
#define  KB_INTERN_IO_MSG                   _IO(KB_IOC_MAGIC, 101 )  // send an I/O-Protocol message and return response
/* new ioctl to upload firmware */
#define PICONTROL_UPLOAD_FIRMWARE           _IOW(KB_IOC_MAGIC, 200, struct picontrol_firmware_upload )
#define MAX_TELEGRAM_DATA_SIZE 255

#endif //WIN32

typedef struct SDeviceInfoStr
{
    uint8_t     i8uAddress;             // Address of module in current configuration
    uint32_t    i32uSerialnumber;       // serial number of module
    uint16_t    i16uModuleType;         // Type identifier of module
    uint16_t    i16uHW_Revision;        // hardware revision
    uint16_t    i16uSW_Major;           // major software version
    uint16_t    i16uSW_Minor;           // minor software version
    uint32_t    i32uSVN_Revision;       // svn revision of software
    uint16_t    i16uInputLength;        // length in bytes of all input values together
    uint16_t    i16uOutputLength;       // length in bytes of all output values together
    uint16_t    i16uConfigLength;       // length in bytes of all config values together
    uint16_t    i16uBaseOffset;         // offset in process image
    uint16_t    i16uInputOffset;        // offset in process image of first input byte
    uint16_t    i16uOutputOffset;       // offset in process image of first output byte
    uint16_t    i16uConfigOffset;       // offset in process image of first config byte
    uint16_t    i16uFirstEntry;         // index of entry
    uint16_t    i16uEntries;            // number of entries in process image
    uint8_t     i8uModuleState;         // fieldbus state of piGate Module
    uint8_t     i8uActive;              // == 0 means that the module is not present and no data is available
    uint8_t     i8uReserve[30];         // space for future extensions without changing the size of the struct
} SDeviceInfo;

typedef struct SEntryInfoStr
{
    uint8_t     i8uAddress;             // Address of module in current configuration
#define ENTRY_INFO_TYPE_INPUT		1
#define ENTRY_INFO_TYPE_OUTPUT		2
#define ENTRY_INFO_TYPE_MEMORY		3
#define ENTRY_INFO_TYPE_MASK		0x7F
    uint8_t     i8uType;                // 1=input, 2=output, 3=memory, 4=config, 0=undefined, + 0x80 if exported
    uint16_t    i16uIndex;              // index of I/O value for this module
    uint16_t    i16uBitLength;          // length of value in bits
    uint8_t     i8uBitPos;              // 0-7 bit position, 0 also for whole byte
    uint16_t    i16uOffset;             // offset in process image
    uint32_t    i32uDefault;            // default value
    char        strVarName[32];         // Variable name
} SEntryInfo;

typedef struct SPIValueStr
{
    uint16_t    i16uAddress;            // Address of the byte in the process image
    uint8_t     i8uBit;                 // 0-7 bit position, >= 8 whole byte
    uint8_t     i8uValue;               // Value: 0/1 for bit access, whole byte otherwise
} SPIValue;

typedef struct SPIVariableStr
{
    char        strVarName[32];         // Variable name
    uint16_t    i16uAddress;            // Address of the byte in the process image
    uint8_t     i8uBit;                 // 0-7 bit position, >= 8 whole byte
    uint16_t    i16uLength;             // length of the variable in bits. Possible values are 1, 8, 16 and 32
} SPIVariable;

typedef struct SDIOResetCounterStr
{
    uint8_t     i8uAddress;             // Address of module in current configuration
    uint16_t    i16uBitfield;           // bitfield, if bit n is 1, reset counter/encoder on input n
} SDIOResetCounter;

struct pictl_calibrate {
	/* Address of module in current configuration */
	unsigned char	address;
	/* bitfield: mode */
	unsigned char	mode;
	/* channels to calibrate */
	unsigned char	channels;
	/* point in lookupTable */
	unsigned char	x_val;
	signed short	y_val;
};

typedef struct SConfigDataStr
{
    uint8_t     bLeft;
    uint16_t    i16uLen;
    char        acData[MAX_TELEGRAM_DATA_SIZE];
} SConfigData;

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

#define REVPI_RO_RELAY_1_BIT 	BIT(0)
#define REVPI_RO_RELAY_2_BIT 	BIT(1)
#define REVPI_RO_RELAY_3_BIT 	BIT(2)
#define REVPI_RO_RELAY_4_BIT 	BIT(3)

#define PICONTROL_CONFIG_ERROR_WRONG_MODULE_TYPE         -10
#define PICONTROL_CONFIG_ERROR_WRONG_INPUT_LENGTH        -11
#define PICONTROL_CONFIG_ERROR_WRONG_OUTPUT_LENGTH       -12
#define PICONTROL_CONFIG_ERROR_WRONG_CONFIG_LENGTH       -13
#define PICONTROL_CONFIG_ERROR_WRONG_INPUT_OFFSET        -14
#define PICONTROL_CONFIG_ERROR_WRONG_OUTPUT_OFFSET       -15
#define PICONTROL_CONFIG_ERROR_WRONG_CONFIG_OFFSET       -16

#define PICONTROL_STATUS_RUNNING                        0x01
#define PICONTROL_STATUS_EXTRA_MODULE                   0x02
#define PICONTROL_STATUS_MISSING_MODULE                 0x04
#define PICONTROL_STATUS_SIZE_MISMATCH                  0x08
#define PICONTROL_STATUS_LEFT_GATEWAY                   0x10
#define PICONTROL_STATUS_RIGHT_GATEWAY                  0x20
#define PICONTROL_STATUS_X2_DIN                         0x40    // RevPi Connect only

#define PICONTROL_LED_A1_GREEN                          0x0001
#define PICONTROL_LED_A1_RED                            0x0002
#define PICONTROL_LED_A2_GREEN                          0x0004
#define PICONTROL_LED_A2_RED                            0x0008
/* Revpi Connect and Flat */
#define PICONTROL_LED_A3_GREEN                          0x0010
#define PICONTROL_LED_A3_RED                            0x0020
/* RevPi Connect only */
#define PICONTROL_X2_DOUT                               0x0040
#define PICONTROL_X2_DOUT_CONNECT4                      0x0001
#define PICONTROL_WD_TRIGGER                            0x0080
/* Revpi Flat only */
#define PICONTROL_LED_A4_GREEN                          0x0040
#define PICONTROL_LED_A4_RED                            0x0080
#define PICONTROL_LED_A5_GREEN                          0x0100
#define PICONTROL_LED_A5_RED                            0x0200
/* RevPi Connect 4 only */
#define PICONTROL_LED_RGB_A1_RED                        0x0001
#define PICONTROL_LED_RGB_A1_GREEN                      0x0002
#define PICONTROL_LED_RGB_A1_BLUE                       0x0004
#define PICONTROL_LED_RGB_A2_RED                        0x0008
#define PICONTROL_LED_RGB_A2_GREEN                      0x0010
#define PICONTROL_LED_RGB_A2_BLUE                       0x0020
#define PICONTROL_LED_RGB_A3_RED                        0x0040
#define PICONTROL_LED_RGB_A3_GREEN                      0x0080
#define PICONTROL_LED_RGB_A3_BLUE                       0x0100
#define PICONTROL_LED_RGB_A4_RED                        0x0200
#define PICONTROL_LED_RGB_A4_GREEN                      0x0400
#define PICONTROL_LED_RGB_A4_BLUE                       0x0800
#define PICONTROL_LED_RGB_A5_RED                        0x1000
#define PICONTROL_LED_RGB_A5_GREEN                      0x2000
#define PICONTROL_LED_RGB_A5_BLUE                       0x4000



/******************************************************************************/
/*******************************  Prototypes  *********************************/
/******************************************************************************/

#endif /* PICONTROL_H_ */

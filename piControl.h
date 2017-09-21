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
 * MIT License
 *
 * Copyright (C) 2017 : KUNBUS GmbH, Heerweg 15C, 73370 Denkendorf, Germany
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *=======================================================================================
 */

#ifndef PICONTROL_H_
#define PICONTROL_H_

/******************************************************************************/
/********************************  Includes  **********************************/
/******************************************************************************/
#ifndef WIN32
#include <linux/ioctl.h>
#endif //WIN32

//#include <stdint.h>

/******************************************************************************/
/*********************************  Types  ************************************/
/******************************************************************************/
#ifndef WIN32

#define PICONTROL_DEVICE	"/dev/piControl0"

// The config file moved to /etc/revpi.
// If it cannot be found there, the old location should be used.
#define PICONFIG_FILE		"/etc/revpi/config.rsc"
#define PICONFIG_FILE_WHEEZY    "/opt/KUNBUS/config.rsc"

#define REV_PI_DEV_FIRST_RIGHT      32		// address of first module on the right side of the RevPi Core
#define REV_PI_DEV_CNT_MAX          64		// max. number of

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

#define PICONTROL_NOT_CONNECTED             0x8000
#define PICONTROL_NOT_CONNECTED_MASK        0x7fff

#define PICONTROL_USER_MODULE_TYPE          0x8000  // old definition, will be removed soon
#define PICONTROL_USER_MODULE_MASK          0x7fff  // old definition, will be removed soon

#define KB_IOC_MAGIC  'K'
#define  KB_CMD1			_IO(KB_IOC_MAGIC, 10 )  // for test only
#define  KB_CMD2			_IO(KB_IOC_MAGIC, 11 )  // for test only
#define  KB_RESET			_IO(KB_IOC_MAGIC, 12 )  // reset the piControl driver including the config file
#define  KB_GET_DEVICE_INFO_LIST	_IO(KB_IOC_MAGIC, 13 )  // get the device info of all detected devices
#define  KB_GET_DEVICE_INFO		_IO(KB_IOC_MAGIC, 14 )  // get the device info of one device
#define  KB_GET_VALUE			_IO(KB_IOC_MAGIC, 15 )  // get the value of one bit in the process image
#define  KB_SET_VALUE			_IO(KB_IOC_MAGIC, 16 )  // set the value of one bit in the process image
#define  KB_FIND_VARIABLE		_IO(KB_IOC_MAGIC, 17 )  // find a varible defined in piCtory
#define  KB_SET_EXPORTED_OUTPUTS	_IO(KB_IOC_MAGIC, 18 )  // copy the exported outputs from a application process image to the real process image
#define  KB_UPDATE_DEVICE_FIRMWARE	_IO(KB_IOC_MAGIC, 19 )  // try to update the firmware of connected devices
#define  KB_DIO_RESET_COUNTER		_IO(KB_IOC_MAGIC, 20 )  // set a counter or endocder to 0
#define  KB_GET_LAST_MESSAGE		_IO(KB_IOC_MAGIC, 21 )  // copy the last error message

#define  KB_WAIT_FOR_EVENT		_IO(KB_IOC_MAGIC, 50 )  // wait for an event. This call is normally blocking
#define  KB_EVENT_RESET			1		// piControl was reset, reload configuration

// the following call are for KUNBUS internal use only. uncomment the following define to activate them.
// #define KUNBUS_TEST
#define  KB_INTERN_SET_SERIAL_NUM	_IO(KB_IOC_MAGIC, 100 )  // set serial num in piDIO, piDI or piDO (can be made only once)
#define  KB_INTERN_IO_MSG		_IO(KB_IOC_MAGIC, 101 )  // send an I/O-Protocol message and return response

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
    uint16_t    i16uLength;              // length of the variable in bits. Possible values are 1, 8, 16 and 32
} SPIVariable;

typedef struct SDIOResetCounterStr
{
	uint8_t     i8uAddress;             // Address of module in current configuration
	uint16_t    i16uBitfield;           // bitfield, if bit n is 1, reset counter/encoder on input n
} SDIOResetCounter;

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

#define PICONTROL_LED_A1_GREEN                          0x01
#define PICONTROL_LED_A1_RED                            0x02
#define PICONTROL_LED_A2_GREEN                          0x04
#define PICONTROL_LED_A2_RED                            0x08


/******************************************************************************/
/*******************************  Prototypes  *********************************/
/******************************************************************************/

#endif /* PICONTROL_H_ */

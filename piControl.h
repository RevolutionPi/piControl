/*!
 *
 * Project: piControl
 * (C)    : KUNBUS GmbH, Heerweg 15C, 73370 Denkendorf, Germany
 *
 */

#ifndef PICONTROL_H_
#define PICONTROL_H_

/******************************************************************************/
/********************************  Includes  **********************************/
/******************************************************************************/
#ifndef WIN32
#include <linux/ioctl.h>
#endif //WIN32

/******************************************************************************/
/*********************************  Types  ************************************/
/******************************************************************************/
#ifndef WIN32

#define PICONTROL_DEVICE    "/dev/piControl0"
#define PICONFIG_FILE       "/opt/KUNBUS/config.rsc"
//#define PICONFIG_FILE       "/home/pi/config.rsc"

#define PICONTROL_USER_MODULE_TYPE      0x8000
#define PICONTROL_USER_MODULE_MASK      0x7fff

#define KB_IOC_MAGIC  'K'
#define  KB_CMD1                    _IO(KB_IOC_MAGIC, 10 )  // for test only
#define  KB_CMD2                    _IO(KB_IOC_MAGIC, 11 )  // for test only
#define  KB_RESET                   _IO(KB_IOC_MAGIC, 12 )  // reset the piControl driver including the config file
#define  KB_GET_DEVICE_INFO_LIST    _IO(KB_IOC_MAGIC, 13 )  // get the device info of all detected devices
#define  KB_GET_DEVICE_INFO         _IO(KB_IOC_MAGIC, 14 )  // get the device info of one device
#define  KB_GET_VALUE               _IO(KB_IOC_MAGIC, 15 )  // get the value of one bit in the process image
#define  KB_SET_VALUE               _IO(KB_IOC_MAGIC, 16 )  // set the value of one bit in the process image
#define  KB_FIND_VARIABLE           _IO(KB_IOC_MAGIC, 17 )  // find a varible defined in piCtory
#define  KB_SET_EXPORTED_OUTPUTS    _IO(KB_IOC_MAGIC, 18 )  // copy the exported outputs from a application process image to the real process image

// the following call are for KUNBUS internal use only
#define  KB_INTERN_SET_SERIAL_NUM   _IO(KB_IOC_MAGIC, 100 )  // set serial num in piDIO, piDI or piDO (can be made only once)

//tut so nicht #define  KB_GET_DEVICE_INFO_U   _IO(KB_IOC_MAGIC, 55 )
#endif //WIN32

typedef struct 
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

typedef struct
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

typedef struct
{
    uint16_t    i16uAddress;            // Address of the byte in the process image
    uint8_t     i8uBit;                 // 0-7 bit position, >= 8 whole byte
    uint8_t     i8uValue;               // Value: 0/1 for bit access, whole byte otherwise
} SPIValue;

typedef struct
{
    char        strVarName[32];         // Variable name
    uint16_t    i16uAddress;            // Address of the byte in the process image
    uint8_t     i8uBit;                 // 0-7 bit position, >= 8 whole byte
    uint16_t    i16uLength;              // length of the variable in bits. Possible values are 1, 8, 16 and 32
} SPIVariable;


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

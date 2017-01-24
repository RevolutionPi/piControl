//+=============================================================================================
//|
//!    \file common_define.h
//!    This file defines global data types and constants as well as other global definitions.
//|
//+---------------------------------------------------------------------------------------------
//|
//|    File-ID:    $Id: common_define.h 11312 2016-12-21 12:39:58Z bvanlaak $
//|    Location:   $URL: http://srv-kunbus03.de.pilz.local/feldbus/software/trunk/platform/common/sw/common_define.h $
//|    Copyright:  KUNBUS GmbH
//|
//+=============================================================================================
#ifndef _COMMON_DEFINE_H_
#define _COMMON_DEFINE_H_

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

//----- TI AM335XX -------
#if defined(__TIAM335XGENERIC__)
#define _32BIT_U_CONTROLLER_
#undef  _MSC_VER

//----- PIC32 MX 360 F 512 L -----
#elif defined(__32MX360F512L__)
#define _32BIT_U_CONTROLLER_
#undef  _MSC_VER

//----- PIC32 MX GENERIC -----
#elif defined(__32MXGENERIC__)
#define _32BIT_U_CONTROLLER_
#undef  _MSC_VER

#elif defined(__STM32GENERIC__)
#define _32BIT_U_CONTROLLER_
#undef  _MSC_VER

#define KB_STORE_PARA           __attribute__ ((section(".storePara"),aligned(4)))
#define KB_STORE_PARA_CRC       __attribute__ ((section(".storeParaCrc")))
#define KB_SCRIPT_FLASH         __attribute__ ((section(".scriptFlash"),aligned(4)))
#define KB_SCRIPT_FLASH_LEN     __attribute__ ((section(".scriptFlashLen"),aligned(4)))
#define KB_CCM_RAM              __attribute__ ((section(".ccmRam"),aligned(4)))    //  closely coupled RAM on STM32F40x Processors



/*
#define COPY_8_P(dest, src) ((INT32U *)(void *)(dest))[0] = ((INT32U *)(void *)(src))[0], \
((INT32U *)(void *)(dest))[1] = ((INT32U *)(void *)(src))[1]
*/

#define COPY_8_P(dest, src) ((INT8U *)(dest))[0] = ((INT8U *)(src))[0], \
  ((INT8U *)(dest))[1] = ((INT8U *)(src))[1], \
  ((INT8U *)(dest))[2] = ((INT8U *)(src))[2], \
  ((INT8U *)(dest))[3] = ((INT8U *)(src))[3], \
  ((INT8U *)(dest))[4] = ((INT8U *)(src))[4], \
  ((INT8U *)(dest))[5] = ((INT8U *)(src))[5], \
  ((INT8U *)(dest))[6] = ((INT8U *)(src))[6], \
  ((INT8U *)(dest))[7] = ((INT8U *)(src))[7]


#elif defined(__NIOS_GENERIC__)
#define _32BIT_U_CONTROLLER_
#undef  _MSC_VER

#elif defined(__SF2_GENERIC__)
#define _32BIT_U_CONTROLLER_
#undef  _MSC_VER

#define KB_STORE_PARA       __attribute__ ((section(".storePara"),aligned(4)))
#define KB_STORE_PARA_CRC   __attribute__ ((section(".storeParaCrc")))
#define KB_CCM_RAM          __attribute__ ((section(".ccmRam"),aligned(4)))
//#define KB_DDR_RAM          __attribute__ ((section(".ddrRam"),aligned(4)))

#define COPY_8_P(dest, src) ((INT8U *)(dest))[0] = ((INT8U *)(src))[0], \
  ((INT8U *)(dest))[1] = ((INT8U *)(src))[1], \
  ((INT8U *)(dest))[2] = ((INT8U *)(src))[2], \
  ((INT8U *)(dest))[3] = ((INT8U *)(src))[3], \
  ((INT8U *)(dest))[4] = ((INT8U *)(src))[4], \
  ((INT8U *)(dest))[5] = ((INT8U *)(src))[5], \
  ((INT8U *)(dest))[6] = ((INT8U *)(src))[6], \
  ((INT8U *)(dest))[7] = ((INT8U *)(src))[7]

#elif defined(__KUNBUSPI__) || defined (__KUNBUSPI_KERNEL__)
#define _32BIT_U_CONTROLLER_
#undef  _MSC_VER

#else // Common Define for Visual Studio / WIN32

#define KB_STORE_PARA
#define KB_STORE_PARA_CRC
#define KB_CCM_RAM

#define COPY_8_P(dest, src) ((INT32U *)(dest))[0] = ((INT32U *)(src))[0], \
  ((INT32U *)(dest))[1] = ((INT32U *)(src))[1]
#endif

#if defined (__GNUC__)
    #define kbSTRUCT_PACKED			__attribute__((__packed__))
#else
    #define kbSTRUCT_PACKED
#endif

//+=============================================================================================
//|    Typen / types
//+=============================================================================================

//__GNUC__
#if defined (_MSC_VER)
typedef unsigned char      INT8U;    ///< 8 Bit unsigned integer
typedef signed char        INT8S;    ///< 8 Bit signed integer
typedef unsigned __int16   INT16U;   ///< 16 Bit unsigned integer
typedef signed __int16     INT16S;   ///< 16 Bit signed integer
typedef unsigned __int32   INT32U;   ///< 32 Bit unsigned integer
typedef signed __int32     INT32S;   ///< 32 Bit signed integer
typedef unsigned __int64   INT64U;   ///< 64 Bit unsigned integer
typedef signed __int64     INT64S;   ///< 64 Bit signed integer
typedef float              FLOAT32;  //< 32 Bit signed floating point
typedef double             FLOAT64;  //< 64 Bit signed floating point
#ifdef __cplusplus
typedef bool               TBOOL;
#else
typedef unsigned __int8    TBOOL;    ///< Boolean value (bTRUE/bFALSE)
#endif
typedef unsigned char      CHAR8U;   ///< 8 Bit unsigned character
typedef signed char        CHAR8S;   ///< 8 Bit signed character
typedef char               CHAR8;    ///< 8 Bit character

#define  __attribute__(x)

#elif defined (_8BIT_U_CONTROLLER_)
#define  _8BIT_U_CONTROLLER_TYPES_
#error "no types defined yet!"

#elif defined (_16BIT_U_CONTROLLER_)
#define  _16BIT_U_CONTROLLER_TYPES_
#error "no types defined yet!"

#elif defined (_32BIT_U_CONTROLLER_)
#define _32BIT_U_CONTROLLER_TYPES_
// 32 bit u-controller is used

// Static Types
typedef unsigned char       INT8U;       ///< 8 Bit unsigned integer
typedef signed char         INT8S;       ///< 8 Bit signed integer
typedef unsigned short      INT16U;      ///< 16 Bit unsigned integer
typedef signed short        INT16S;      ///< 16 Bit signed integer
typedef unsigned int        INT32U;      ///< 32 Bit unsigned integer
typedef signed int          INT32S;      ///< 32 Bit signed integer
typedef unsigned long long  INT64U;      ///< 64 Bit unsigned integer
typedef signed long long    INT64S;      ///< 64 Bit signed integer
typedef float               FLOAT32;     ///< 32 Bit signed floating point
typedef double              FLOAT64;     ///< 64 Bit signed floating point
typedef unsigned char       TBOOL;       ///< Boolean value (bTRUE/bFALSE)
typedef unsigned char       CHAR8U;      ///< 8 Bit unsigned character
typedef signed char         CHAR8S;      ///< 8 Bit signed character
typedef char                CHAR8;       ///< 8 Bit character

typedef unsigned short      uint16_t;      ///< 16 Bit unsigned integer

#else
#error "Current architecture is not supported by BSP/CommonDefine"
#endif//_32BIT_U_CONTROLLER_TYPES_

#ifdef __cplusplus
#define bTRUE      true
#define bFALSE     false
#else
#define bTRUE      ((TBOOL)1)
#define bFALSE    ((TBOOL)0)
#endif
#define ROMCONST__         const
#define NOP__          _nop_

#ifndef NULL
#ifdef __cplusplus
#define NULL      0
#define NULL_PTR  0
#else
#define NULL    ((void *)0)
#define NULL_PTR  ((void *)0)
#endif
#endif

//+=============================================================================================
//|    Konstanten / constant data
//+=============================================================================================

typedef struct S_KUNBUS_REV_NUMBER
{
  INT8U  IDENT1[9];    ///< identification String 1 "KB_SW_REV"
  INT8U  SW_MAJOR;     ///< major revision number; valid numbers 0-50, other numbers reserved
  INT16U  SW_MINOR;    ///< minor revision number; valid numbers 0-1000, other numbers reserved
  INT32U  REVISION;    ///< SVN revision number (mainly for internal use);
  ///< valid numbers 0-999999, other numbers reserved; typicalli SVN rev.
} T_KUNBUS_REV_NUMBER;  ///< Kunbus internal revision and release number



#define KUNBUS_FW_DESCR_TYP_PNOZ_PROFIBUS                           1
#define KUNBUS_FW_DESCR_TYP_PNOZ_CAN_OPEN                           2
#define KUNBUS_FW_DESCR_TYP_PNOZ_DEV_NET                            3
#define KUNBUS_FW_DESCR_TYP_IC_PROFIBUS                             4
#define KUNBUS_FW_DESCR_TYP_IC_CAN_OPEN                             5
#define KUNBUS_FW_DESCR_TYP_IC_DEV_NET                              6
#define KUNBUS_FW_DESCR_TYP_PNOZ_ETHERNET_IP                        7
#define KUNBUS_FW_DESCR_TYP_IC_ETHERNET_IP                          8
#define KUNBUS_FW_DESCR_TYP_PNOZ_ETHERCAT                           9
#define KUNBUS_FW_DESCR_TYP_IC_ETHERCAT                            10
#define KUNBUS_FW_DESCR_TYP_PNOZ_SERCOS3                           11
#define KUNBUS_FW_DESCR_TYP_PNOZ_CCLINK                            12
#define KUNBUS_FW_DESCR_TYP_PNOZ_PROFINET_RT                       13
#define KUNBUS_FW_DESCR_TYP_IC_CCLINK                              14
#define KUNBUS_FW_DESCR_TYP_MINIMULTI_PROFIBUS                     15
#define KUNBUS_FW_DESCR_TYP_MINIMULTI_CAN_OPEN                     16
#define KUNBUS_FW_DESCR_TYP_MINIMULTI_DEV_NET                      17
#define KUNBUS_FW_DESCR_TYP_MINIMULTI_ETHERNET_IP                  18
#define KUNBUS_FW_DESCR_TYP_MINIMULTI_ETHERCAT                     19
#define KUNBUS_FW_DESCR_TYP_MINIMULTI_PROFINET_RT                  20
#define KUNBUS_FW_DESCR_TYP_MINIMULTI_SERCOS3                      21
#define KUNBUS_FW_DESCR_TYP_PNOZ_POWERLINK                         22
#define KUNBUS_FW_DESCR_TYP_IC_MODBUS_TCP                          23
#define KUNBUS_FW_DESCR_TYP_COMS_BASEBOARD                         24
#define KUNBUS_FW_DESCR_TYP_COMS_PROFIBUS                          25
#define KUNBUS_FW_DESCR_TYP_COMS_CAN_OPEN                          26
#define KUNBUS_FW_DESCR_TYP_COMS_DEV_NET                           27
#define KUNBUS_FW_DESCR_TYP_COMS_CCLINK                            28
#define KUNBUS_FW_DESCR_TYP_COMS_ETHERCAT                          29
#define KUNBUS_FW_DESCR_TYP_COMS_ETHERNET_IP                       30
#define KUNBUS_FW_DESCR_TYP_COMS_POWERLINK                         31
#define KUNBUS_FW_DESCR_TYP_COMS_PROFINET_RT                       32
#define KUNBUS_FW_DESCR_TYP_COMS_SERCOS3                           33
#define KUNBUS_FW_DESCR_TYP_COMM_CAN_OPEN                          34
#define KUNBUS_FW_DESCR_TYP_REGT_MINI_MULTI_SERNUM_PROG            35
#define KUNBUS_FW_DESCR_TYP_REGT_POWER_SWITCH                      36
#define KUNBUS_FW_DESCR_TYP_REGT_ETHERNET_SW                       37
#define KUNBUS_FW_DESCR_TYP_REGT_FIELD_BUS_SWITCH                  38
#define KUNBUS_FW_DESCR_TYP_REGT_SHIFT_REGISTER                    39
#define KUNBUS_FW_DESCR_TYP_IC_PROFINET_RT                         40
#define KUNBUS_FW_DESCR_TYP_MINIMULTI_CCLINK                       41
#define KUNBUS_FW_DESCR_TYP_REGT_CAN_CARD                          42
#define KUNBUS_FW_DESCR_TYP_REGT_PNOZ_B0_SIM                       43
#define KUNBUS_FW_DESCR_TYP_GW_MODBUS_ETHERCAT_S_KUNBUS            44
#define KUNBUS_FW_DESCR_TYP_GW_MODBUS_CANOPEN_S_PILZ               45
#define KUNBUS_FW_DESCR_TYP_GW_MODBUS_CANOPEN_M                    46
#define KUNBUS_FW_DESCR_TYP_PNOZ_B0_PROFIBUS                       47
#define KUNBUS_FW_DESCR_TYP_PNOZ_B0_CAN_OPEN                       48
#define KUNBUS_FW_DESCR_TYP_PNOZ_B0_DEV_NET                        49
#define KUNBUS_FW_DESCR_TYP_PNOZ_B0_ETHERNET_IP                    50
#define KUNBUS_FW_DESCR_TYP_PNOZ_B0_ETHERCAT                       51
#define KUNBUS_FW_DESCR_TYP_PNOZ_B0_PROFINET_RT                    52
#define KUNBUS_FW_DESCR_TYP_PNOZ_B0_SERCOS3                        53
#define KUNBUS_FW_DESCR_TYP_REGT_PNOZ_MM_SIM                       54
#define KUNBUS_FW_DESCR_TYP_GW_MODBUS_CANOPEN_S_KUNBUS             55
#define KUNBUS_FW_DESCR_TYP_IC_ETHERNET_IP_2PORT                   56
#define KUNBUS_FW_DESCR_TYP_GW_MODBUS_ETHERCAT_S_PILZ              57
#define KUNBUS_FW_DESCR_TYP_COMS_PROFINET_TPS1                     58
#define KUNBUS_FW_DESCR_TYP_REGT_LED_SWITCH_SIM                    59
#define KUNBUS_FW_DESCR_TYP_COMS_PROFIBUS_ANALOG                   60
#define KUNBUS_FW_DESCR_TYP_IC_PROFINET_TPS1                       61
#define KUNBUS_FW_DESCR_TYP_IC_CAN_OPEN_SCHLEGEL                   62
#define KUNBUS_FW_DESCR_TYP_PNOZ_B0_POWERLINK                      63
#define KUNBUS_FW_DESCR_TYP_MINIMULTI_POWERLINK                    64
#define KUNBUS_FW_DESCR_TYP_PNOZ_B0_PROFINET_TPS1                  65
#define KUNBUS_FW_DESCR_TYP_IC_SERIAL                              66
#define KUNBUS_FW_DESCR_TYP_COMS_SERIAL                            67
#define KUNBUS_FW_DESCR_TYP_GW_MODBUS_SERIAL                       68
#define KUNBUS_FW_DESCR_TYP_COMS_PROFINET_RT_CLA                   69       // COMS Profinet RT Conformance Class A
#define KUNBUS_FW_DESCR_TYP_IC_DEV_NET_SCHMIDT                     70
#define KUNBUS_FW_DESCR_TYP_MG_CAN_OPEN                            71
#define KUNBUS_FW_DESCR_TYP_MG_CCLINK                              72
#define KUNBUS_FW_DESCR_TYP_MG_DEV_NET                             73
#define KUNBUS_FW_DESCR_TYP_MG_ETHERCAT                            74
#define KUNBUS_FW_DESCR_TYP_MG_ETHERNET_IP                         75
#define KUNBUS_FW_DESCR_TYP_MG_POWERLINK                           76
#define KUNBUS_FW_DESCR_TYP_MG_PROFIBUS                            77
#define KUNBUS_FW_DESCR_TYP_MG_PROFINET_RT                         78
#define KUNBUS_FW_DESCR_TYP_MG_PROFINET_IRT                        79
#define KUNBUS_FW_DESCR_TYP_MG_CAN_OPEN_MASTER                     80
#define KUNBUS_FW_DESCR_TYP_MG_SERCOS3                             81
#define KUNBUS_FW_DESCR_TYP_MG_SERIAL                              82
#define KUNBUS_FW_DESCR_TYP_MG_PROFINET_RT_MASTER                  83
#define KUNBUS_FW_DESCR_TYP_MG_PROFINET_IRT_MASTER                 84
#define KUNBUS_FW_DESCR_TYP_MG_ETHERCAT_MASTER                     85
#define KUNBUS_FW_DESCR_TYP_REGT_UART_CARD                         86
#define KUNBUS_FW_DESCR_TYP_IC_MODBUS_TCP_2PORT                    87
#define KUNBUS_FW_DESCR_TYP_SB_ETHERNET_IP                         88
#define KUNBUS_FW_DESCR_TYP_SB_PROFIBUS                            89
#define KUNBUS_FW_DESCR_TYP_SB_PROFINET                            90
#define KUNBUS_FW_DESCR_TYP_IC_PROFINET_SGATE                      91
#define KUNBUS_FW_DESCR_TYP_MG_MODBUS_RTU                          92
#define KUNBUS_FW_DESCR_TYP_MG_MODBUS_TCP                          93
#define KUNBUS_FW_DESCR_TYP_SB_MODBUS_TCP                          94
#define KUNBUS_FW_DESCR_TYP_PI_CORE                                95
#define KUNBUS_FW_DESCR_TYP_PI_DIO_14                              96
#define KUNBUS_FW_DESCR_TYP_PI_DI_16                               97
#define KUNBUS_FW_DESCR_TYP_PI_DO_16                               98
#define KUNBUS_FW_DESCR_TYP_TAP                                    99
#define KUNBUS_FW_DESCR_TYP_MG_DMX                                100
#define KUNBUS_FW_DESCR_TYP_REGT_IOLM                             101
#define KUNBUS_FW_DESCR_TYP_REGT_IOLD                             102
#define KUNBUS_FW_DESCR_TYP_PI_AIO                                103


#define KUNBUS_FW_DESCR_TYP_INTERN                                  0xffff
#define KUNBUS_FW_DESCR_TYP_UNDEFINED                               0xffff

#define KUNBUS_FW_DESCR_MAC_ADDR_LEN                  6     //!< number of bytes in a MAC Address


// defines for field i32uBootFlags in T_KUNBUS_APPL_DESCR
#define KUNBUS_APPL_DESCR_BOOT_FACTORY_RESET                    0x00000001  // if this bit is set in the default appl descr, a factory reset is performed on the first boot after a firmware update



//-- defines for product variants -----------------------------------------------------------------
#define VARIANT_PILZ        1       // Pilz Variant of a Product
#define VARIANT_KUNBUS      2       // KUNBUS Variant of a Product

#ifdef VARIANT_PRODUCT
    #if !(  (VARIANT_PRODUCT == VARIANT_PILZ)           \
          ||(VARIANT_PRODUCT == VARIANT_KUNBUS)         \
         )
        #error "VARIANT_PRODUCT with unregistered VARIANT used"
    #endif
#endif


typedef
#include <COMP_packBegin.h>
struct S_KUNBUS_FW_DESCR
{
  INT32U  i32uLength;                   ///< number of bytes in struct, used to determine which elements are present
  ///  must be always the first member of the struct
  INT32U  i32uSerialNumber;             ///< 32 Bit serial number
  INT16U  i16uDeviceType;               ///< Kunbus internal, unambiguous device type
  INT16U  i16uHwRevision;               ///< Revision of hardware, used for firmware update
  INT8U   ai8uMacAddr[KUNBUS_FW_DESCR_MAC_ADDR_LEN];  ///< Field for manufacturer set MAC Address
  INT32U  i32uFwuEntryAddr;             //!< Entry of Firmwareupdate from application
  INT32U  i32uApplStartAddr;            //!< Startaddress of application specific flash area
  INT32U  i32uApplEndAddr;              //!< Last address of application specific flash area
}                                       ///< Kunbus internal option bytes
#include <COMP_packEnd.h>
T_KUNBUS_FW_DESCR;


#define KUNBUS_APPL_BOOT_FLAG_FACTORY_RESET     0x00000001

typedef
#include <COMP_packBegin.h>
struct S_KUNBUS_APPL_DESCR_V1
{
  INT32U i32uLength;                    //!< number of bytes in struct, used to determine which elements are present
  INT32U i32uVectorAddr;                //!< address of vector table.
  INT32U i32uCrcCheckStartAddr;         //!< first address of application area for crc check
  INT32U i32uCrcCheckEndAddr;           //!< last address of application area for crc check
  INT32U i32uCrcAddr;                   //!< address of CRC Checksum over application area
  INT8U  ai8uIdent1[9];                 ///< identification String 1 "KB_SW_REV"
  INT8U  i8uSwMajor;                    ///< major revision number; valid numbers 0-50, other numbers reserved
  INT16U i16uSwMinor;                   ///< minor revision number; valid numbers 0-1000, other numbers reserved
  INT32U i32uSvnRevision;               ///< SVN revision number (mainly for internal use);
}
#include <COMP_packEnd.h>
T_KUNBUS_APPL_DESCR_V1;

typedef
#include <COMP_packBegin.h>
struct S_KUNBUS_APPL_DESCR
{
    INT32U i32uLength;                    //!< number of bytes in struct, used to determine which elements are present
    INT32U i32uVectorAddr;                //!< address of vector table.
    INT32U i32uCrcCheckStartAddr;         //!< first address of application area for crc check
    INT32U i32uCrcCheckEndAddr;           //!< last address of application area for crc check
    INT32U i32uCrcAddr;                   //!< address of CRC Checksum over application area
    INT8U  ai8uIdent1[9];                 ///< identification String 1 "KB_SW_REV"
    INT8U  i8uSwMajor;                    ///< major revision number; valid numbers 0-50, other numbers reserved
    INT16U i16uSwMinor;                   ///< minor revision number; valid numbers 0-1000, other numbers reserved
    INT32U i32uSvnRevision;               ///< SVN revision number (mainly for internal use);
    INT32U i32uBootFlags;                 ///< Boot action flags
}
#include <COMP_packEnd.h>
T_KUNBUS_APPL_DESCR;

typedef
#include <COMP_packBegin.h>
struct S_KUNBUS_CNFG_DATA_HDR
{
  INT8U  ai8uIdent[4];                  ///< identification String 1 "KBCD"
  INT32U i32uCrc;                       ///< address of CRC Checksum over application area
  INT32U i32uLength;                    ///< number of bytes stored directly after this header
  INT16U i16uDeviceType;                ///< Kunbus internal, unambiguous device type
  INT16U i16uHwRevision;                ///< Revision of hardware
  INT8U  i8uSwMajor;                    ///< major revision number; valid numbers 0-50, other numbers reserved
  INT8U  ai8uDummy[3];                  ///< padding
}
#include <COMP_packEnd.h>
T_KUNBUS_CNFG_DATA_HDR;


#define SER_WR_SUCCESS              (0xF0)  ///< Serial number successful written
#define SER_WR_FAIL_ILLEGAL_NUM     (0xF1)  ///< Illegal / Invalid Serial number
#define SER_WR_FAIL_ALREADY_SET     (0xF2)  ///< Serial number already set; it MUST not be changed !

//+=============================================================================================
//|    Prototypen / prototypes
//+=============================================================================================

#ifdef __cplusplus
extern "C" {
#endif

extern const T_KUNBUS_APPL_DESCR ctKunbusApplDescription_g;
extern const T_KUNBUS_FW_DESCR ctKunbusFirmwareDescription_g;
extern char *sKunbusFWDescription_g;

#ifdef __cplusplus
}
#endif


#else   // _COMMON_DEFINE_H_
//  #pragma message "_COMMON_DEFINE_H_ already defined !"
#endif   // _COMMON_DEFINE_H_

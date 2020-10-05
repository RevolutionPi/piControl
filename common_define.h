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

#ifndef _COMMON_DEFINE_H_
#define _COMMON_DEFINE_H_

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#define _32BIT_U_CONTROLLER_
#undef  _MSC_VER

//+=============================================================================================
//|    Typen / types
//+=============================================================================================

//__GNUC__

// Static Types
typedef unsigned char INT8U;	///< 8 Bit unsigned integer
typedef signed char INT8S;	///< 8 Bit signed integer
typedef unsigned short INT16U;	///< 16 Bit unsigned integer
typedef signed short INT16S;	///< 16 Bit signed integer
typedef unsigned int INT32U;	///< 32 Bit unsigned integer
typedef signed int INT32S;	///< 32 Bit signed integer
typedef unsigned long long INT64U;	///< 64 Bit unsigned integer
typedef signed long long INT64S;	///< 64 Bit signed integer
typedef float FLOAT32;		///< 32 Bit signed floating point
typedef double FLOAT64;		///< 64 Bit signed floating point
typedef unsigned char TBOOL;	///< Boolean value (bTRUE/bFALSE)
typedef unsigned char CHAR8U;	///< 8 Bit unsigned character
typedef signed char CHAR8S;	///< 8 Bit signed character
typedef char CHAR8;		///< 8 Bit character

typedef unsigned short uint16_t;	///< 16 Bit unsigned integer

#ifdef __cplusplus
#define bTRUE      true
#define bFALSE     false
#else /*  */
#define bTRUE      ((TBOOL)1)
#define bFALSE    ((TBOOL)0)
#endif /*  */

#ifndef NULL
#ifdef __cplusplus
#define NULL      0
#define NULL_PTR  0
#else /*       */
#define NULL    ((void *)0)
#define NULL_PTR  ((void *)0)
#endif /*        */
#endif /*        */

//+=============================================================================================
//|    Konstanten / constant data
//+=============================================================================================

typedef struct S_KUNBUS_REV_NUMBER {

	INT8U IDENT1[9];	///< identification String 1 "KB_SW_REV"
	INT8U SW_MAJOR;		///< major revision number; valid numbers 0-50, other numbers reserved
	INT16U SW_MINOR;	///< minor revision number; valid numbers 0-1000, other numbers reserved
	INT32U REVISION;	///< SVN revision number (mainly for internal use);
	///< valid numbers 0-999999, other numbers reserved; typicalli SVN rev.
} T_KUNBUS_REV_NUMBER;		///< Kunbus internal revision and release number

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
#define KUNBUS_FW_DESCR_TYP_MG_PROFINET_SITARA                     83
#define KUNBUS_FW_DESCR_TYP_MG_PROFINET_IRT_MASTER                 84
#define KUNBUS_FW_DESCR_TYP_MG_ETHERCAT_MASTER                     85
#define KUNBUS_FW_DESCR_TYP_MG_MODBUS_RTU                          92
#define KUNBUS_FW_DESCR_TYP_MG_MODBUS_TCP                          93
#define KUNBUS_FW_DESCR_TYP_PI_CORE                                95
#define KUNBUS_FW_DESCR_TYP_PI_DIO_14                              96
#define KUNBUS_FW_DESCR_TYP_PI_DI_16                               97
#define KUNBUS_FW_DESCR_TYP_PI_DO_16                               98
#define KUNBUS_FW_DESCR_TYP_MG_DMX                                100
#define KUNBUS_FW_DESCR_TYP_PI_AIO                                103
#define KUNBUS_FW_DESCR_TYP_PI_COMPACT                            104
#define KUNBUS_FW_DESCR_TYP_PI_CONNECT                            105
#define KUNBUS_FW_DESCR_TYP_PI_CON_CAN                            109
#define KUNBUS_FW_DESCR_TYP_PI_CON_MBUS                           110
#define KUNBUS_FW_DESCR_TYP_PI_CON_BT                             111
#define KUNBUS_FW_DESCR_TYP_PI_MIO                                118
#define KUNBUS_FW_DESCR_TYP_PI_FLAT                               135

#define KUNBUS_FW_DESCR_TYP_INTERN                                  0xffff
#define KUNBUS_FW_DESCR_TYP_UNDEFINED                               0xffff

#define KUNBUS_FW_DESCR_MAC_ADDR_LEN                  6	//!< number of bytes in a MAC Address

typedef
#include <COMP_packBegin.h>
    struct S_KUNBUS_FW_DESCR {

	INT32U i32uLength;	///< number of bytes in struct, used to determine which elements are present
	///  must be always the first member of the struct
	INT32U i32uSerialNumber;	///< 32 Bit serial number
	INT16U i16uDeviceType;	///< Kunbus internal, unambiguous device type
	INT16U i16uHwRevision;	///< Revision of hardware, used for firmware update
	INT8U ai8uMacAddr[KUNBUS_FW_DESCR_MAC_ADDR_LEN];	///< Field for manufacturer set MAC Address
	INT32U i32uFwuEntryAddr;	//!< Entry of Firmwareupdate from application
	INT32U i32uApplStartAddr;	//!< Startaddress of application specific flash area
	INT32U i32uApplEndAddr;	//!< Last address of application specific flash area
}				///< Kunbus internal option bytes
#include <COMP_packEnd.h>
T_KUNBUS_FW_DESCR;


typedef
#include <COMP_packBegin.h>
    struct S_KUNBUS_APPL_DESCR {

	INT32U i32uLength;	//!< number of bytes in struct, used to determine which elements are present
	INT32U i32uVectorAddr;	//!< address of vector table.
	INT32U i32uCrcCheckStartAddr;	//!< first address of application area for crc check
	INT32U i32uCrcCheckEndAddr;	//!< last address of application area for crc check
	INT32U i32uCrcAddr;	//!< address of CRC Checksum over application area
	INT8U ai8uIdent1[9];	///< identification String 1 "KB_SW_REV"
	INT8U i8uSwMajor;	///< major revision number; valid numbers 0-50, other numbers reserved
	INT16U i16uSwMinor;	///< minor revision number; valid numbers 0-1000, other numbers reserved
	INT32U i32uSvnRevision;	///< SVN revision number (mainly for internal use);
	INT32U i32uBootFlags;	///< Boot action flags
}
#include <COMP_packEnd.h>
T_KUNBUS_APPL_DESCR;

typedef
#include <COMP_packBegin.h>
    struct S_KUNBUS_CNFG_DATA_HDR {

	INT8U ai8uIdent[4];	///< identification String 1 "KBCD"
	INT32U i32uCrc;		///< address of CRC Checksum over application area
	INT32U i32uLength;	///< number of bytes stored directly after this header
	INT16U i16uDeviceType;	///< Kunbus internal, unambiguous device type
	INT16U i16uHwRevision;	///< Revision of hardware
	INT8U i8uSwMajor;	///< major revision number; valid numbers 0-50, other numbers reserved
	INT8U ai8uDummy[3];	///< padding
}
#include <COMP_packEnd.h>
T_KUNBUS_CNFG_DATA_HDR;


//+=============================================================================================
//|    Prototypen / prototypes
//+=============================================================================================

#ifdef __cplusplus
extern "C" {
#endif				/*
				 */

	extern const T_KUNBUS_APPL_DESCR ctKunbusApplDescription_g;

	extern const T_KUNBUS_FW_DESCR ctKunbusFirmwareDescription_g;

	extern char *sKunbusFWDescription_g;

#ifdef __cplusplus
}
#endif				/*
				 */
#else // _COMMON_DEFINE_H_
//  #pragma message "_COMMON_DEFINE_H_ already defined !"
#endif // _COMMON_DEFINE_H_

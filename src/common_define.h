/* SPDX-License-Identifier: GPL-2.0-only
 * SPDX-FileCopyrightText: 2016-2024 KUNBUS GmbH
 */

#ifndef _COMMON_DEFINE_H_
#define _COMMON_DEFINE_H_

#include <linux/types.h>

//+=============================================================================================
//|    Konstanten / constant data
//+=============================================================================================

typedef struct S_KUNBUS_REV_NUMBER {

	u8 IDENT1[9];	///< identification String 1 "KB_SW_REV"
	u8 SW_MAJOR;		///< major revision number; valid numbers 0-50, other numbers reserved
	u16 SW_MINOR;	///< minor revision number; valid numbers 0-1000, other numbers reserved
	u32 REVISION;	///< SVN revision number (mainly for internal use);
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
#define KUNBUS_FW_DESCR_TYP_PI_CONNECT_4                          136
#define KUNBUS_FW_DESCR_TYP_PI_RO                                 137
#define KUNBUS_FW_DESCR_TYP_PI_CONNECT_5                          138

#define KUNBUS_FW_DESCR_TYP_PI_REVPI_GENERIC_PB                     0xfffe
#define KUNBUS_FW_DESCR_TYP_INTERN                                  0xffff
#define KUNBUS_FW_DESCR_TYP_UNDEFINED                               0xffff

#define KUNBUS_FW_DESCR_MAC_ADDR_LEN                  6	//!< number of bytes in a MAC Address

typedef struct S_KUNBUS_FW_DESCR {
	u32 i32uLength;	///< number of bytes in struct, used to determine which elements are present
	///  must be always the first member of the struct
	u32 i32uSerialNumber;	///< 32 Bit serial number
	u16 i16uDeviceType;	///< Kunbus internal, unambiguous device type
	u16 i16uHwRevision;	///< Revision of hardware, used for firmware update
	u8 ai8uMacAddr[KUNBUS_FW_DESCR_MAC_ADDR_LEN];	///< Field for manufacturer set MAC Address
	u32 i32uFwuEntryAddr;	//!< Entry of Firmwareupdate from application
	u32 i32uApplStartAddr;	//!< Startaddress of application specific flash area
	u32 i32uApplEndAddr;	//!< Last address of application specific flash area
} __attribute__((__packed__)) T_KUNBUS_FW_DESCR; ///< Kunbus internal option bytes


typedef struct S_KUNBUS_APPL_DESCR {
	u32 i32uLength;	//!< number of bytes in struct, used to determine which elements are present
	u32 i32uVectorAddr;	//!< address of vector table.
	u32 i32uCrcCheckStartAddr;	//!< first address of application area for crc check
	u32 i32uCrcCheckEndAddr;	//!< last address of application area for crc check
	u32 i32uCrcAddr;	//!< address of CRC Checksum over application area
	u8 ai8uIdent1[9];	///< identification String 1 "KB_SW_REV"
	u8 i8uSwMajor;	///< major revision number; valid numbers 0-50, other numbers reserved
	u16 i16uSwMinor;	///< minor revision number; valid numbers 0-1000, other numbers reserved
	u32 i32uSvnRevision;	///< SVN revision number (mainly for internal use);
	u32 i32uBootFlags;	///< Boot action flags
} __attribute__((__packed__)) T_KUNBUS_APPL_DESCR;

typedef struct S_KUNBUS_CNFG_DATA_HDR {
	u8 ai8uIdent[4];	///< identification String 1 "KBCD"
	u32 i32uCrc;		///< address of CRC Checksum over application area
	u32 i32uLength;	///< number of bytes stored directly after this header
	u16 i16uDeviceType;	///< Kunbus internal, unambiguous device type
	u16 i16uHwRevision;	///< Revision of hardware
	u8 i8uSwMajor;	///< major revision number; valid numbers 0-50, other numbers reserved
	u8 ai8uDummy[3];	///< padding
} __attribute__((__packed__)) T_KUNBUS_CNFG_DATA_HDR;


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

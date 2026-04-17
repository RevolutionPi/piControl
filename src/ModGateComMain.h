/* SPDX-License-Identifier: GPL-2.0-only
 * SPDX-FileCopyrightText: 2016-2023 KUNBUS GmbH
 */

#ifndef MODGATECOMMAIN_H_INC
#define MODGATECOMMAIN_H_INC

#if defined (_MSC_VER)
#pragma warning (disable: 4200)
#endif

#include "common_define.h"

typedef enum
{
    FBSTATE_OFFLINE   = 0x00, // Physikalisch nicht verbunden
    FBSTATE_LINK      = 0x01, // Physikalisch verbunden, jedoch keine Kommunikation
    FBSTATE_STANDBY   = 0x02, // Kommunikationsbereit (z.B. preoperational)
    FBSTATE_CYCLIC_IO = 0x03, // Zyklischer Datenaustausch ist aktiv (z.B. operational)
} MODGATECOM_FieldbusStatus;

//**********************************************************************************************
// Link Layer
//**********************************************************************************************
typedef struct {
    u8   i8uDestination[6];
    u8   i8uSource[6];
    u16  i16uType;
#ifndef __KUNBUSPI_KERNEL__
    u8   i8uACK;             //Acknowledge
    u8   i8uCounter;
#endif
} __attribute__((__packed__)) MODGATECOM_LinkLayer;

//**********************************************************************************************
// Transport Layer
//**********************************************************************************************
typedef struct {
#ifdef __KUNBUSPI_KERNEL__
    u8   i8uACK;             //Acknowledge
    u8   i8uCounter;
#endif
    u16  i16uCmd;
    u16  i16uDataLength;
    u32  i32uError;
    u8   i8uVersion;
    u8   i8uReserved;
} __attribute__((__packed__)) MODGATECOM_TransportLayer;

//**********************************************************************************************
// Application Layer
//**********************************************************************************************

typedef enum
{
    MODGATE_ST_HW_CHECK                  = 0x00,
    MODGATE_ST_LINK_CHECK                = 0x01,
    MODGATE_ST_ID_REQ                    = 0x02,
    MODGATE_ST_ID_RESP                   = 0x03,
    MODGATE_ST_RUN_NO_DATA               = 0x04,
    MODGATE_ST_RUN                       = 0x05,
} MODGATE_AL_Status;

typedef enum
{
    MODGATE_AL_CMD_ID_Req                   = 0x0001,
    MODGATE_AL_CMD_ID_Resp                  = 0x8001,
    MODGATE_AL_CMD_cyclicPD                 = 0x0002,
    MODGATE_AL_CMD_updatePD                 = 0x0003,   // not used yet
    MODGATE_AL_CMD_ST_Req                   = 0x0004,   // not used
    MODGATE_AL_CMD_ST_Resp                  = 0x8004,   // not used
} MODGATE_AL_Command;

// Feature descriptor bits
#define MODGATE_feature_IODataExchange          0x0001 // supports data-exchange using ethernet (e.g. mGate)

#define MODGATE_MAX_PD_DATALEN              512

//**********************************************************************************************
typedef struct {
    u32  i32uSerialnumber;
    u16  i16uModulType;
    u16  i16uHW_Revision;
    u16  i16uSW_Major;
    u16  i16uSW_Minor;
    u32  i32uSVN_Revision;
    u16  i16uFBS_InputLength;
    u16  i16uFBS_OutputLength;
    u16  i16uFeatureDescriptor;
} __attribute__((__packed__)) MODGATECOM_IDResp;

//**********************************************************************************************
typedef struct {
    u8   i8uFieldbusStatus;  // type MODGATECOM_FieldbusStatus
    u16  i16uOffset;
    u16  i16uDataLen;
    u8   i8uData[0];     // dummy declaration for up to MODGATE_MAX_PD_DATALEN bytes
} __attribute__((__packed__)) MODGATECOM_CyclicPD;

#define MODGATE_LL_MAX_LEN                  ((sizeof(MODGATECOM_LinkLayer) + sizeof(MODGATECOM_TransportLayer) + sizeof(MODGATECOM_CyclicPD) + MODGATE_MAX_PD_DATALEN + 3) & 0xfffffffc)

#endif //MODGATECOMMAIN_H_INC

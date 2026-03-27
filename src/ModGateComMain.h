/* SPDX-License-Identifier: GPL-2.0-only
 * SPDX-FileCopyrightText: 2016-2023 KUNBUS GmbH
 */

#ifndef MODGATECOMMAIN_H_INC
#define MODGATECOMMAIN_H_INC

#define MODGATECOM_MAX_MODULES      2

#if defined (_MSC_VER)
#pragma warning (disable: 4200)
#endif

#include "common_define.h"
#include "kbUtilities.h"

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
    MODGATECOM_enPLS_INIT                   = 0x01,
    MODGATECOM_enPLS_LINK_MISSING           = 0x02,
    MODGATECOM_enPLS_DATA_MISSING           = 0x03,
    MODGATECOM_enPLS_RUN                    = 0x04,
    MODGATECOM_enPLS_ERROR                  = 0x05,
}  MODGATECOM_EPowerLedState;


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
#define MODGATE_feature_RS485DataExchange       0x0002 // supports data exchange using RS485 (e.g. piDio)


#define MODGATE_LL_HEADER_LEN               (sizeof(MODGATECOM_LinkLayer))  // 16
#define MODGATE_TL_HEADER_LEN               (sizeof(MODGATECOM_LinkLayer) + sizeof(MODGATECOM_TransportLayer))  // 26
#define MODGATE_MAX_PD_DATALEN              512
#define MODGATE_AL_MAX_LEN                  (sizeof(MODGATECOM_CyclicPD) + MODGATE_MAX_PD_DATALEN)   // 543 size of the biggest AL packet
#define MODGATE_LL_MAX_LEN                  ((MODGATE_TL_HEADER_LEN + MODGATE_AL_MAX_LEN + 3) & 0xfffffffc) // 544 bigger packet on the line, rounded up to the next multiple of 4

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

typedef struct {
    MODGATECOM_LinkLayer      strLinkLayer;
    MODGATECOM_TransportLayer strTransportLayer;
    u8                     i8uData[MODGATE_AL_MAX_LEN];
} __attribute__((__packed__)) MODGATECOM_Packet;

//**********************************************************************************************

#ifdef __cplusplus
extern "C" {
#endif

//**********************************************************************************************
// Link Layer
//**********************************************************************************************

typedef struct _sLLData
{
    MODGATECOM_LinkLayer    Header;
    u8                   state;
    u32                  send_tick;       // tick counter of last sent packet
    u8                   send_retry;      // retry counter
    u32                  recv_tick;       // tick counter of last recv packet
    bool                   timed_out;
    MODGATECOM_Packet      *pLastData;
} sLLData;

typedef sLLData *LLHandle;

u32 MG_LL_init (LLHandle llHdl);
u32 MG_LL_send (LLHandle llHdl, MODGATECOM_Packet *pData_p);
MODGATECOM_Packet *MG_LL_recv(LLHandle llHdl, u32 *pi32uStatus_p, u16 *pi16uLen_p);
bool  MG_LL_pending (LLHandle llHdl);
void   MG_LL_abort (LLHandle llHdl);
bool  MG_LL_timed_out (LLHandle llHdl);

//**********************************************************************************************
// Application Layer
//**********************************************************************************************
typedef struct _sALData
{
    sLLData     llParas;

    MODGATECOM_IDResp OtherID;              //!< ID-Data of other mGate
    kbUT_Timer  AL_Timeout;

    u8 *pi8uInData;
    u16 i16uInDataLen;
    u16 i16uInDataLenActive;

    u8 *pi8uOutData;
    u16 i16uOutDataLen;
    u16 i16uOutDataLenActive;

    u8    i8uState;                      //!< modular Gateway state machine state
    u8    i8uOtherFieldbusState;         //!< Fieldbus State of other mGate
    MODGATECOM_EPowerLedState enLedStateAct;
    MODGATECOM_EPowerLedState enLedStateOld;
} sALData;

typedef sALData *ALHandle;

extern sALData AL_Data_s[MODGATECOM_MAX_MODULES];

//**********************************************************************************************
#ifndef __KUNBUSPI_KERNEL__
u32 MODGATECOM_init (u8 *pi8uInData_p,  u16 i16uInDataLen_p, u8 *pi8uOutData_p, u16 i16uOutDataLen_p, ETHERNET_INTERFACE *EthDrv);
void   MODGATECOM_run (void);

void   MODGATECOM_SetOwnFieldbusState(u8 i8uOwnFieldbusState_p);
u8  MODGATECOM_GetOwnFieldbusState(void);
u8  MODGATECOM_GetOtherFieldbusState(u8 i8uInst_p);               // in modular Gateways always 0 must be passed
MODGATECOM_EPowerLedState MODGATECOM_GetLedState(void);

u16 MODGATECOM_GetInputDataLengthActive(u8 i8uInstance_p);    // in modular Gateways, always 0 must be passed
u16 MODGATECOM_GetOutputDataLengthActive(u8 i8uInstance_p);   // ditto

//**********************************************************************************************
// internal
//**********************************************************************************************

u32 MODGATECOM_send_ID_Req   (ALHandle);
u32 MODGATECOM_send_ID_Resp  (ALHandle);
u32 MODGATECOM_send_cyclicPD (ALHandle);

bool MODGATECOM_recv_Id_Resp  (ALHandle, MODGATECOM_Packet *pPacket_p);
bool MODGATECOM_recv_cyclicPD (ALHandle, MODGATECOM_Packet *pPacket_p);

void   MODGATECOM_managePowerLedRun (void);
MODGATE_AL_Status MODGATECOM_GetState(u8 i8uInst_p);

//**********************************************************************************************

extern MODGATECOM_IDResp MODGATE_OwnID_g;     //!< ID-Data of this mGate



#define MODGATECOM_GetOtherFieldbusStatePtr(i)       (&AL_Data_s[i].i8uOtherFieldbusState)
#define MODGATECOM_GetOwnIdDataPtr()                 (&MODGATE_OwnID_g)
#define MODGATECOM_GetOtherIdDataPtr(i)              (&AL_Data_s[i].OtherID)


// internal functions
// for all functions: return value true is send/receive was successful
bool  MODGATECOM_receive (void);
bool  MODGATECOM_send (u16 cmd);
bool  MODGATECOM_send_ACK (void);

void   MODGATECOM_T1_Handler (void);
void   MODGATECOM_T2_Handler (void);

#endif // !__KUNBUSPI_KERNEL__

#ifdef  __cplusplus
}
#endif


#endif //MODGATECOMMAIN_H_INC

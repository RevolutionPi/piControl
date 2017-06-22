//+=============================================================================================
//|
//!		\file ModGateRS485.h
//!		Serial communication of mGate
//|
//+---------------------------------------------------------------------------------------------
//|
//|		File-ID:		$Id: ModGateRS485.h 10720 2016-08-08 14:51:10Z mduckeck $
//|		Location:		$URL: http://srv-kunbus03.de.pilz.local/feldbus/software/trunk/platform/ModGateCom/sw/ModGateRS485.h $
//|		Company:		$Cpn: KUNBUS GmbH $
//|
//+=============================================================================================

#ifndef MODGATERS485_H_INC
#define MODGATERS485_H_INC

#include <stddef.h>

#include <IoProtocol.h>

#define MAX_TELEGRAM_DATA_SIZE 256
#define MAX_FWU_DATA_SIZE      250

#define RS485_HDRLEN (offsetof(SRs485Telegram, ai8uData))

#define MODGATE_RS485_BROADCAST_ADDR            0xff
#define MODGATE_RS485_COMMAND_ANSWER_OK         0x4000
#define MODGATE_RS485_COMMAND_ANSWER_ERROR      0x8000
#define MODGATE_RS485_COMMAND_ANSWER_FILTER     (~(MODGATE_RS485_COMMAND_ANSWER_OK | MODGATE_RS485_COMMAND_ANSWER_ERROR))

typedef enum
{
    eCmdPing                  = 0x0001,
    eCmdSetFwUpdateMode       = 0x0002,
    eCmdGetFwInfo             = 0x0003,
    eCmdReadFwFlash           = 0x0004,
    eCmdWriteFwFlash          = 0x0005,
    eCmdEraseFwFlash          = 0x0006,
    eCmdWriteSerialNumber     = 0x0007,
    eCmdResetModule           = 0x0008,
    eCmdWriteMacAddr          = 0x0009,
    eCmdGetDeviceInfo         = 0x000a,
    eCmdFactoryReset          = 0x000b,
    eCmdSetLEDGreen           = 0x000c,
    eCmdSetLEDRed             = 0x000d,
    eCmdRAPIMessage           = 0x000e,
    eCmdGetErrorLog           = 0x000f,
    eCmdScriptMessage         = 0x0010,
    eCmdAgentCode             = 0x0011,     //!< Agent testing only, not in productive environment
    eCmdserFlashErase         = 0x0012,     // löschen des externen Flash
    eCmdserFlashWrite         = 0x0013,     // schreiben auf externen Flash
    // Commands from PiBridge master to slave
    eCmdPiIoSetAddress        = 0x0014,     // PiBridge master sends the module address
    eCmdPiIoSetTermination    = 0x0015,     // The slave should set the RS485 termination resistor
    eCmdPiIoConfigure         = 0x0016,     // The configuration data for the slave
    eCmdPiIoStartDataExchange = 0x0017,     // Slave have to start dataexchange
} ERs485Command;

typedef enum
{
    eIoConfig,
    eGateProtocol,
    eIoProtocol,
} ERs485Protocol;

typedef
#include <COMP_packBegin.h>
struct
{
    INT8U i8uDstAddr;
    INT8U i8uSrcAddr;
    INT16U i16uCmd;
    INT16U i16uSequNr;
    INT8U i8uDataLen;
    INT8U ai8uData[MAX_TELEGRAM_DATA_SIZE];
}
#include <COMP_packEnd.h>
SRs485Telegram;

//-----------------------------------------------------------------------------

extern void *REG_MOD_GATE_pvRs485TelegHeapHandle_g;
extern void *pvRs485TelegHeapHandle_g;
extern TBOOL bConfigurationComplete_g;  //!< Configuration from PiBridge master is completed

extern INT8U i8uOwnAddress_g;

//-----------------------------------------------------------------------------

// Initialize RS485-Communication
INT32U ModGateRs485Init(void);

TBOOL ModGateRs485GetResponseState(void);
void  ModGateRs485SetResponseState(TBOOL bResponseState_p);

// Runtime function
void  ModGateRs485Run(void);

// Send telegram
void  ModGateRs485InsertSendTelegram(SRs485Telegram *psuSendTelegram_p);

// Change protocol
void  ModGateRs485SetProtocol(ERs485Protocol eRs485Protocol_p);


TBOOL ModGateRs485IsRunning(void);


#endif // MODGATERS485_H_INC

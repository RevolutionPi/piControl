//+=============================================================================================
//|
//!		\file IoProtocol.h
//!		Serial communication for Pi IO modules
//|
//+---------------------------------------------------------------------------------------------
//|
//|		File-ID:		$Id:$
//|		Location:		$URL:$
//|		Company:		$Cpn:$
//|
//+=============================================================================================

#ifndef IOPROTOCOL_H_INC
#define IOPROTOCOL_H_INC

#include <common_define.h>

#define IOPROTOCOL_MAXDATA_LENGTH 32
#define IOPROTOCOL_HEADER_LENGTH 2

#define IOP_TYP1_CMD_DATA       0
#define IOP_TYP1_CMD_CFG        1
#define IOP_TYP1_CMD_DATA2      2
#define IOP_TYP1_CMD_DATA3      3
#define IOP_TYP1_CMD_DATA4      4
#define IOP_TYP1_CMD_DATA5      5
#define IOP_TYP1_CMD_DATA6      6
#define IOP_TYP1_CMD_DATA7      7

#define IOP_TYP2_CMD_UNDEF0                    0
#define IOP_TYP2_CMD_UNDEF1                    1
#define IOP_TYP2_CMD_GOTO_GATE_PROTOCOL     0x3f

typedef enum
{
    eIoProtocolIdle     = 1, // Wait for first received character of frame
    eIoProtocolReceive  = 2, // Wait for the rest of the frame
    eIoProtocolTimeout  = 3, // IOPROTOCOL_TIMEOUT occured
} EIoProtocolState;

// Header
typedef 
#include <COMP_packBegin.h>
union
{
    INT8U ai8uHeader[IOPROTOCOL_HEADER_LENGTH];
    struct                              // Unicast
    {
        INT8U bitAddress : 6;
        INT8U bitIoHeaderType : 1;      // 0 for header type 1
        INT8U bitReqResp : 1;
        INT8U bitLength : 5;
        INT8U bitCommand : 3;
    }sHeaderTyp1;
    struct                              // Broadcast
    {
        INT8U bitCommand : 6;
        INT8U bitIoHeaderType : 1;      // 1 for header type 2
        INT8U bitReqResp : 1;
        INT8U bitLength : 5;
        INT8U bitDataPart1 : 3;
    }sHeaderTyp2;
} 
#include <COMP_packEnd.h>
UIoProtocolHeader;

//-----------------------------------------------------------------------------
// generic data structure for request and response
typedef
#include <COMP_packBegin.h>
struct
{
    UIoProtocolHeader uHeader;
    INT8U ai8uData[IOPROTOCOL_MAXDATA_LENGTH+1];    // one more byte for CRC
}
#include <COMP_packEnd.h>
SIOGeneric;

// ----------------- BROADCAST messages -------------------------------------

//-----------------------------------------------------------------------------
// ----------------- DIGITAL IO modules -------------------------------------
//-----------------------------------------------------------------------------
// Request for Digital IO modules: Config
typedef
#include <COMP_packBegin.h>
struct      // IOP_TYP1_CMD_CFG
{
    UIoProtocolHeader uHeader;
    INT16U i16uOutputPushPull;          // bitfield: 1=push-pull, 0=high side mode
    INT16U i16uOutputOpenLoadDetect;    // bitfield: 1=detect open load in high side mode
    INT16U i16uOutputPWM;               // bitfield: 1=generate pwm signal
    INT8U  i8uOutputPWMIncrement;       // [1-10] increment for pwm algorithm

    INT8U  i8uInputDebounce;            // 0=Off, 1=25us, 2=750us, 3=3ms, 4-255 not allowed
    INT32U i32uInputMode;               // bitfield, 2 bits per channel: 00=direct, 01=counter, rising edge, 10=counter, falling edge, 11=encoder
    INT8U i8uCrc;
}
#include <COMP_packEnd.h>
SDioConfig;


//-----------------------------------------------------------------------------
// Request for Digital IO modules: digital output only, no pwm
typedef
#include <COMP_packBegin.h>
struct      // IOP_TYP1_CMD_DATA
{
    UIoProtocolHeader uHeader;
    INT16U i16uOutput;
    INT8U  i8uCrc;
}
#include <COMP_packEnd.h>
SDioRequest;

// Request for Digital IO modules: output with variable number of pwm values
typedef
#include <COMP_packBegin.h>
struct      // IOP_TYP1_CMD_DATA2
{
    UIoProtocolHeader uHeader;
    INT16U i16uOutput;
    INT16U i16uChannels;    // bitfield counter channel
    INT8U  ai8uValue[16];   // [0-100] pwm value in %
    INT8U  i8uCrc;
}
#include <COMP_packEnd.h>
SDioPWMOutput;

// Request for Digital IO modules: reset counter values
typedef
#include <COMP_packBegin.h>
struct      // IOP_TYP1_CMD_DATA3
{
    UIoProtocolHeader uHeader;
    INT16U i16uChannels;    // bitfield counter channel
    INT8U  i8uCrc;
}
#include <COMP_packEnd.h>
SDioCounterReset;


//-----------------------------------------------------------------------------
// Response of Digital IO modules
typedef struct  
{
    INT8U bitInputCommErr : 1;          // no communication with chip
    INT8U bitInputUVL1 : 1;             // under voltage 1 on channel 0-7
    INT8U bitInputUVL2 : 1;             // under voltage 2 on channel 0-7
    INT8U bitInputOTL : 1;              // over temperature on channel 0-7
    INT8U bitInputUVH1 : 1;             // under voltage 1 on channel 8-15
    INT8U bitInputUVH2 : 1;             // under voltage 2 on channel 8-15
    INT8U bitInputOTh : 1;              // over temperature on channel 8-15
    INT8U bitInputFault : 1;            // fault signal on gpio

    INT8U bitOutputCommErr : 1;         // no communication with chip
    INT8U bitOutputCRCErr : 1;          // output chip reports crc error
    INT8U bitOutputFault : 1;           // fault signal on gpio
    INT8U bitOutputReserve : 5;
}SDioModuleStatus;

// Answer if Digital IO modules: digital input and status, no counter or encoder values
typedef
#include <COMP_packBegin.h>
struct      // IOP_TYP1_CMD_DATA
{
    UIoProtocolHeader uHeader;
    INT16U i16uInput;                   // 0=low signal, 1=high signal
    INT16U i16uOutputStatus;            // 0=error on output pin (thermal shutdown, over load, open load in high side mode)
    SDioModuleStatus sDioModuleStatus;
    INT8U i8uCrc;
}
#include <COMP_packEnd.h>
SDioResponse;

// Answer if Digital IO modules: digital input and status, with counter or encoder values
typedef
#include <COMP_packBegin.h>
struct      // IOP_TYP1_CMD_DATA2
{
    UIoProtocolHeader uHeader;
    INT16U i16uInput;                   // 0=low signal, 1=high signal
    INT16U i16uOutputStatus;            // 0=error on output pin (thermal shutdown, over load, open load in high side mode)
    SDioModuleStatus sDioModuleStatus;
    INT32U ai32uCounters[16];           // dummy array, contains only values for the activated counters/encoders
    INT8U i8uCrc;
}
#include <COMP_packEnd.h>
SDioCounterResponse;

//-----------------------------------------------------------------------------
// ----------------- ANALOG IO modules -------------------------------------
//-----------------------------------------------------------------------------
// Config request for Analog IO modules
typedef
#include <COMP_packBegin.h>
struct      // IOP_TYP1_CMD_CFG
{
    UIoProtocolHeader uHeader;
}
#include <COMP_packEnd.h>
SAioConfig;

//-----------------------------------------------------------------------------
// Data request for Analog IO modules
typedef
#include <COMP_packBegin.h>
struct      // IOP_TYP1_CMD_DATA
{
    UIoProtocolHeader uHeader;
    INT16U i16uOutput[4];
    INT8U  i8uCrc;
}
#include <COMP_packEnd.h>
SAioRequest;

//-----------------------------------------------------------------------------
// Response of Analog IO modules
typedef struct
{
    INT8U bitInputCommErr : 1;          // no communication with input chip
    INT8U bitOutputCommErr : 1;         // no communication with output chip
} SAioModuleStatus;

typedef
#include <COMP_packBegin.h>
struct      // IOP_TYP1_CMD_DATA
{
    UIoProtocolHeader uHeader;
    SAioModuleStatus sAioModuleStatus;
    INT16U i16uInput[4];
    INT16U i16uRtd[2];
    INT16U i16uCurrentSens;
    INT16U i16uTempSens;
    INT16U i16uVsIn;
    INT8U i8uCrc;
}
#include <COMP_packEnd.h>
SAioResponse;

//-----------------------------------------------------------------------------
#endif // IOPROTOCOL_H_INC

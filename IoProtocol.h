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
    eIoProtocolIdle = 1, // Wait for first received character of frame
    eIoProtocolReceive = 2, // Wait for the rest of the frame
    eIoProtocolTimeout = 3, // IOPROTOCOL_TIMEOUT occured
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
    INT8U ai8uData[IOPROTOCOL_MAXDATA_LENGTH + 1];    // one more byte for CRC
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
#define AIO_MAX_OUTPUTS 2
#define AIO_MAX_INPUTS 4
#define AIO_HALF_INPUTS  2
#define AIO_MAX_RTD 2

// Config request for Analog IO modules
// Output ranges
typedef enum
{
    AIO_OUTPUT_RANGE_UNUSED = 0,   // Output not in use
    AIO_OUTPUT_RANGE_0TO5V = 1,   // Range 0 V to 5 V
    AIO_OUTPUT_RANGE_0TO10V = 2,   // Range 0  V to 10 V
    AIO_OUTPUT_RANGE_PM5V = 3,   // Range -5 V to +5 V
    AIO_OUTPUT_RANGE_PM10V = 4,   // Range -10 V to +10 V
    AIO_OUTPUT_RANGE_0TO5_5V = 5,   // Range 0 V to 5.5 V
    AIO_OUTPUT_RANGE_0TO11V = 6,   // Range 0  V to 11 V
    AIO_OUTPUT_RANGE_PM5_5V = 7,   // Range -5.5 V to +5.5 V
    AIO_OUTPUT_RANGE_PM11V = 8,   // Range -11 V to +11 V
    AIO_OUTPUT_RANGE_4TO20MA = 9,   // Range 4 mA to 20 mA
    AIO_OUTPUT_RANGE_0TO20MA = 10,  // Range 0 mA to 20 mA
    AIO_OUTPUT_RANGE_0TO24MA = 11,  // Range 0 mA to 24 mA
} EAioOutputRange;

// Output slew rate stepsize
typedef enum
{
    AIO_OUTPUT_SLEWRATE_STEPSIZE_1 = 0,
    AIO_OUTPUT_SLEWRATE_STEPSIZE_2 = 1,
    AIO_OUTPUT_SLEWRATE_STEPSIZE_4 = 2,
    AIO_OUTPUT_SLEWRATE_STEPSIZE_8 = 3,
    AIO_OUTPUT_SLEWRATE_STEPSIZE_16 = 4,
    AIO_OUTPUT_SLEWRATE_STEPSIZE_32 = 5,
    AIO_OUTPUT_SLEWRATE_STEPSIZE_64 = 6,
    AIO_OUTPUT_SLEWRATE_STEPSIZE_128 = 7,
} EAioSlewRateStepSize;

// Output slew rate update frequency
typedef enum
{
    AIO_OUTPUT_SLEWRATE_FREQUENCY_258_065 = 0,
    AIO_OUTPUT_SLEWRATE_FREQUENCY_200_000 = 1,
    AIO_OUTPUT_SLEWRATE_FREQUENCY_153_845 = 2,
    AIO_OUTPUT_SLEWRATE_FREQUENCY_131_145 = 3,
    AIO_OUTPUT_SLEWRATE_FREQUENCY_115_940 = 4,
    AIO_OUTPUT_SLEWRATE_FREQUENCY_69_565 = 5,
    AIO_OUTPUT_SLEWRATE_FREQUENCY_37_560 = 6,
    AIO_OUTPUT_SLEWRATE_FREQUENCY_25_805 = 7,
    AIO_OUTPUT_SLEWRATE_FREQUENCY_20_150 = 8,
    AIO_OUTPUT_SLEWRATE_FREQUENCY_16_030 = 9,
    AIO_OUTPUT_SLEWRATE_FREQUENCY_10_295 = 10,
    AIO_OUTPUT_SLEWRATE_FREQUENCY_8_280 = 11,
    AIO_OUTPUT_SLEWRATE_FREQUENCY_6_900 = 12,
    AIO_OUTPUT_SLEWRATE_FREQUENCY_5_530 = 13,
    AIO_OUTPUT_SLEWRATE_FREQUENCY_4_240 = 14,
    AIO_OUTPUT_SLEWRATE_FREQUENCY_3_300 = 15,
} EAioSlewRateFrequency;

// Input ranges
typedef enum
{
    AIO_INPUT_RANGE_UNUSED = 0,  // Input not in use
    AIO_INPUT_RANGE_PM10V = 1,  // Range -10 V to +10 V
    AIO_INPUT_RANGE_0TO10V = 2,  // Range 0 V to 10 V
    AIO_INPUT_RANGE_0TO5V = 3,  // Range 0 V to 5 V
    AIO_INPUT_RANGE_PM5V = 4,  // Range -5 V to +5 V
    AIO_INPUT_RANGE_0TO20MA = 5,  // Range 0 mA to 20 mA
    AIO_INPUT_RANGE_0TO24MA = 6,  // Range 0 mA to 24 mA
    AIO_INPUT_RANGE_4TO20MA = 7,  // Range 4 mA to 20 mA
    AIO_INPUT_RANGE_PM25MA = 8,  // Range -25 mA to +25 mA
} EAioInputRange;

// Input samples per second
typedef enum
{
    AIO_INPUT_SAMPLE_RATE_1 = 0,
    AIO_INPUT_SAMPLE_RATE_2 = 1,
    AIO_INPUT_SAMPLE_RATE_4 = 2,
    AIO_INPUT_SAMPLE_RATE_8 = 3,
    AIO_INPUT_SAMPLE_RATE_10 = 4,
    AIO_INPUT_SAMPLE_RATE_25 = 5,
    AIO_INPUT_SAMPLE_RATE_50 = 6,
    AIO_INPUT_SAMPLE_RATE_125 = 7,
} EAioInputSampleRate;

// Output configuration
typedef
#include <COMP_packBegin.h>
struct
{
    EAioOutputRange       eOutputRange : 4;
    TBOOL                 bSlewRateEnabled : 1;
    EAioSlewRateStepSize  eSlewRateStepSize : 3;
    EAioSlewRateFrequency eSlewRateFrequency : 4;
    INT8U                 i8uReserve : 4;
    // Scaling output values val = A1/A2*x + B
    INT16S                i16sA1; // Scaling A1
    INT16U                i16uA2; // Scaling A2
    INT16S                i16sB;  // Scaling B
}
#include <COMP_packEnd.h>
SAioOutputConfig;

// Input configuration
typedef
#include <COMP_packBegin.h>
struct
{
    EAioInputRange      eInputRange : 4;
    EAioInputSampleRate i8uReserve : 4;
    // Scaling input values val = A1/A2*x + B
    INT16S              i16sA1; // Scaling A1
    INT16U              i16uA2; // Scaling A2
    INT16S              i16sB;  // Scaling B
}
#include <COMP_packEnd.h>
SAioInputConfig;

// RTD configuration, 7 Bytes
typedef
#include <COMP_packBegin.h>
struct
{
    INT8U i8uSensorType : 1; // 0:PT100 1:PT1000
    INT8U i8uMeasureMethod : 1; // 0:3-wire 1:4-wire
    INT8U i8uReserve : 6;
    // Scaling temperatures val = A1/A2*x + B
    INT16S i16sA1;              // Scaling A1
    INT16U i16uA2;              // Scaling A2
    INT16S i16sB;               // Scaling B
}
#include <COMP_packEnd.h>
SAioRtdConfig;

typedef
#include <COMP_packBegin.h>
struct      // IOP_TYP1_CMD_CFG
{
    UIoProtocolHeader uHeader;
    INT8U i8uInputSampleRate;
    SAioRtdConfig sAioRtdConfig[AIO_MAX_RTD];
    SAioOutputConfig sAioOutputConfig[AIO_MAX_OUTPUTS];
    INT8U i8uCrc;
}
#include <COMP_packEnd.h>
SAioConfig;

typedef
#include <COMP_packBegin.h>
struct      // IOP_TYP1_CMD_CFG
{
    UIoProtocolHeader uHeader;
    SAioInputConfig sAioInputConfig[AIO_HALF_INPUTS];		// 16 Bytes
    INT8U i8uCrc;
}
#include <COMP_packEnd.h>
SAioInConfig;

//-----------------------------------------------------------------------------
// Data request for Analog IO modules, 4 Bytes
typedef
#include <COMP_packBegin.h>
struct      // IOP_TYP1_CMD_DATA
{
    UIoProtocolHeader uHeader;
    INT16S i16sOutputValue[AIO_MAX_OUTPUTS]; // Output value in mV or uA
    INT8U  i8uCrc;
}
#include <COMP_packEnd.h>
SAioRequest;

//-----------------------------------------------------------------------------
// Data response of Analog IO modules, 20 Bytes
typedef
#include <COMP_packBegin.h>
struct      // IOP_TYP1_CMD_DATA
{
    UIoProtocolHeader uHeader;
    INT16S i16sInputValue[AIO_MAX_INPUTS];  // Input value in mV or uA
    INT8U i8uInputStatus[AIO_MAX_INPUTS];   // Input status
    INT16S i16sRtdValue[AIO_MAX_RTD];       // RTD value in 0,1°C
    INT8U i8uRtdStatus[AIO_MAX_RTD];        // RTD status
    INT8U i8uOutputStatus[AIO_MAX_OUTPUTS]; // Output status
    INT8U i8uCrc;
}
#include <COMP_packEnd.h>
SAioResponse;

//-----------------------------------------------------------------------------
// Data request for raw values of Analog IO modules, 0 Bytes
typedef
#include <COMP_packBegin.h>
struct      // IOP_TYP1_CMD_DATA4
{
    UIoProtocolHeader uHeader;
    INT8U  i8uCrc;
}
#include <COMP_packEnd.h>
SAioRawRequest;

//-----------------------------------------------------------------------------
// Data response for raw values of Analog IO modules, 10 Bytes
typedef
#include <COMP_packBegin.h>
struct      // IOP_TYP1_CMD_DATA4
{
    UIoProtocolHeader uHeader;
    INT16U i16uInternalCurrentSensor;
    INT16U i16uInternalVoltageSensor;
    INT16U i16uInternalTemperatureSensor;
    INT16U i16uRtdValue[AIO_MAX_RTD];
    INT8U  i8uCrc;
}
#include <COMP_packEnd.h>
SAioRawResponse;

//-----------------------------------------------------------------------------
// Data request for rtd scaling values of Analog IO modules, 0 Bytes
typedef
#include <COMP_packBegin.h>
struct
{
    INT16U i16uRtd3wireFactor[AIO_MAX_RTD];
    INT16S i16sRtd3wireOffset[AIO_MAX_RTD];
    INT16U i16uRtd4wireFactor[AIO_MAX_RTD];
    INT16S i16sRtd4wireOffset[AIO_MAX_RTD];
}
#include <COMP_packEnd.h>
SAioRtdScaling;

typedef
#include <COMP_packBegin.h>
struct      // IOP_TYP1_CMD_DATA5
{
    UIoProtocolHeader uHeader;
    SAioRtdScaling sRtdScaling;
    INT8U  i8uCrc;
}
#include <COMP_packEnd.h>
SAioScalingRequest;

//-----------------------------------------------------------------------------
// Data response for rtd scaling values of Analog IO modules, 1 Byte
typedef
#include <COMP_packBegin.h>
struct      // IOP_TYP1_CMD_DATA5
{
    UIoProtocolHeader uHeader;
    INT8U  i8uSuccess; // 1: Success
    INT8U  i8uCrc;
}
#include <COMP_packEnd.h>
SAioScalingResponse;

//-----------------------------------------------------------------------------
#endif // IOPROTOCOL_H_INC

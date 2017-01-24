//+=============================================================================================
//|
//!		\file ModGateComError.h
//!		Error handling in communication layers of mGate
//|
//+---------------------------------------------------------------------------------------------
//|
//|		File-ID:		$Id: ModGateComError.h 10628 2016-07-13 13:57:28Z mduckeck $
//|		Location:		$URL: http://srv-kunbus03.de.pilz.local/feldbus/software/trunk/platform/ModGateCom/sw/ModGateComError.h $
//|		Company:		$Cpn: KUNBUS GmbH $
//|
//+=============================================================================================

#ifndef MODGATEERROR_H_INC
#define MODGATEERROR_H_INC

#include <stdarg.h>
#include <bsp/setjmp/BspSetJmp.h>

// mGate errors 0x37xxxxxx
typedef enum
{
    MODGATECOM_NO_ERROR                       = 0x00000000,  //!< Default Value

    // mGate communication
    MODGATECOM_ERROR_TIMER_INIT               = 0x37000001, // Error during timer initialization
    MODGATECOM_ERROR_RS485_INIT               = 0x37000002, // Error during RS485 initialization
    MODGATECOM_ERROR_RESP_SEQUENCE            = 0x37000003, // Falsche Sequenznummer im Antwortframe
    MODGATECOM_ERROR_RESP_CMDNO               = 0x37000004, // Falsche Kommandonummer im Antwortframe
    MODGATECOM_ERROR_WRONG_ID                 = 0x37000005, // Falsche Daten in der ID-Response
    MODGATECOM_ERROR_INVALID_DATA_SIZE        = 0x37000006, // 
    MODGATECOM_ERROR_INVALID_DATA_OFFSET      = 0x37000007, // 
    MODGATECOM_ERROR_WRONG_STATE              = 0x37000008, // 
    MODGATECOM_ERROR_CMD_UNKNOWN              = 0x37000009, // 
    MODGATECOM_ERROR_SEND_FAILED              = 0x3700000a, // send failed
    MODGATECOM_ERROR_SEND_UNACKED             = 0x3700000b, // send failed
    MODGATECOM_ERROR_NO_LINK                  = 0x3700000c, // 
    MODGATECOM_ERROR_OUT_OF_MEMORY_1          = 0x3700000d, // 
    MODGATECOM_ERROR_OUT_OF_MEMORY_2          = 0x3700000e, // 
    MODGATECOM_ERROR_OUT_OF_MEMORY_3          = 0x3700000f, // 
    MODGATECOM_ERROR_OUT_OF_MEMORY_4          = 0x37000010, // 
    MODGATECOM_ERROR_OUT_OF_MEMORY_5          = 0x37000011, // 
    MODGATECOM_ERROR_NO_PACKET                = 0x37000012, // 
    MODGATECOM_ERROR_ACK_MISSING              = 0x37000013, //
    MODGATECOM_ERROR_UNKNOWN_CMD              = 0x37000014, // 


    // RS485 Command error codes
    MODGATECOM_ERROR_CMD_NOT_SUPPORTED        = 0x37000100, // Unknown RS485 command
    MODGATECOM_ERROR_READ_FW_FLASH            = 0x37000101, // Fehler beim Lesen des FW-Flashs
    MODGATECOM_ERROR_SERIAL_NUMBER_PROTECTED  = 0x37000102, // Die schon vorhandene Seriennummer darf nicht überschrieben werden
    MODGATECOM_ERROR_SERIAL_NUMBER_WRITE      = 0x37000103, // Beim Schreiben der Seriennummer ist ein Fehler aufgetreten
    MODGATECOM_ERROR_ERASE_FW_FLASH           = 0x37000104, // Beim Löschen des Firmware Flashspeichers ist ein Fehler aufgetreten
    MODGATECOM_ERROR_RESET                    = 0x37000105, // Der Reset konnte nicht ausgeführt werden
    MODGATECOM_ERROR_UPDATE_MODE              = 0x37000106, // Es kann nicht in den Updatemodus umgeschaltet werden
    MODGATECOM_ERROR_NO_UPDATE_MODE           = 0x37000107, // Das Kommando darf nur im Updatemodus ausgeführt werden
    MODGATECOM_ERROR_TOO_FEW_FLASH_DATA       = 0x37000108, // Flash Write : Too few data
    MODGATECOM_ERROR_FLASH_WRITE_OUT_OF_RANGE = 0x37000109, // Flash Write : Start address out of range
    MODGATECOM_ERROR_TOO_MANY_FLASH_DATA      = 0x3700010a, // Flash Write : Overflow of allowed area
    MODGATECOM_ERROR_FLASH_WRITE              = 0x3700010b, // Flash Write : Flash Programming Error
    MODGATECOM_ERROR_FLASH_WRITE_ALIGNMENT    = 0x3700010c, // Flash Driver Write: odd alignment of start address
    MODGATECOM_ERROR_FLASH_WRITE_GENERAL      = 0x3700010d, // Flash Driver Write: general write error
    MODGATECOM_ERROR_FLASH_ERASE_GENERAL      = 0x3700010e, // Flash Driver Erase: general error
    MODGATECOM_ERROR_FLASH_READ_FORMAT        = 0x3700010f, // Read Data: Invalid Data Format in Request
    MODGATECOM_ERROR_FLASH_READ_OUT_OF_RANGE  = 0x37000110, // Read Data: Startaddress is out of range
    MODGATECOM_ERROR_NO_APPL                  = 0x37000111, // No Application loaded
    MODGATECOM_ERROR_NO_APPL_END              = 0x37000112, // No valid application start or end address
    MODGATECOM_ERROR_DEVTYPE_EXIST            = 0x37000113, // Device Type: Type allready programmed
    MODGATECOM_ERROR_DEVTYPE_FORMAT           = 0x37000114, // Device Type: Telegram Format error
    MODGATECOM_ERROR_APPE_FORMAT              = 0x37000115, // Application End Addr: Invalid Data Format in Request
    MODGATECOM_ERROR_APPE_EXIST               = 0x37000116, // Application End Addr: Address was already written
    MODGATECOM_ERROR_HWR_FORMAT               = 0x37000117, // Hardware Revision: Invalid Data Format in Request
    MODGATECOM_ERROR_HWR_EXIST                = 0x37000118, // Hardware Revision: Revision was already written
    MODGATECOM_ERROR_MAC_FORMAT               = 0x37000119, // MAC Address: Invalid Data Format in Request
    MODGATECOM_ERROR_MAC_EXIST                = 0x3700011a, // MAC Address: Address was already written

    //
    MODGATECOM_ERROR_UNKNOWN                  = 0x37ffffff, // Unknown error, should never happens

} EModGateComError;

#define MODGATECOM_ASSERT(expr, errCode)   if (!(expr)) MODGATECOM_error (errCode, bTRUE, 0)

#ifdef __cplusplus
extern "C" { 
#endif 

extern void MODGATECOM_errorInit (BSP_TJumpBuf *ptExceptionPoint_p, void (*cbErrHandler_p)(INT32U i32uErrorCode_p, TBOOL bFatal_p, INT8U i8uParaCnt_p, va_list argptr_p));
extern void MODGATECOM_error (INT32U i32uErrCode_p, TBOOL bFatalErr_p, INT8U i8uParaCnt_p, ...);

#if defined(MGATE_ERROR_STACK) && defined(STM_WITH_EEPROM)
#define MGATE_ERROR_STACK_ELEMENTS              8               //!< number of error stack elements

void MODGATECOM_errorLogEEPROM(INT32U i32uErrCode_p);
void MODGATECOM_getErrorLogEEPROM(INT32U *pai32uErrStackBuf_p);
#endif //#if defined(MGATE_ERROR_STACK) && defined(STM_WITH_EEPROM)

#ifdef  __cplusplus 
} 
#endif 

#endif //MODGATEERROR_H_INC

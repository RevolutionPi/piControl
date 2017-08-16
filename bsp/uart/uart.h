//+=============================================================================================
//|
//!		\file uart.h
//!
//!		STM32 BSP uart header.
//!
//!		definitions for UART usage within STM BSP.
//|
//+---------------------------------------------------------------------------------------------
//|
//|		Files required:	(none)
//|
//+=============================================================================================

#ifndef UART_H_INC
#define UART_H_INC

typedef enum UART_EBaudRate_
{
    UART_enBAUD_INVALID     =  0,
    UART_enBAUD_110         =  1,
    UART_enBAUD_300         =  2,
    UART_enBAUD_600         =  3,
    UART_enBAUD_1200        =  4,
    UART_enBAUD_2400        =  5,
    UART_enBAUD_4800        =  6,
    UART_enBAUD_9600        =  7,
    UART_enBAUD_19200       =  8,
    UART_enBAUD_38400       =  9,
    UART_enBAUD_57600       = 10,
    UART_enBAUD_115200      = 11,
    UART_enBAUD_500         = 12,
    UART_enBAUD_1000        = 13,
    UART_enBAUD_100000      = 14,   //DMX baud rate for break, 250 kBbd used for data frames
    UART_enBAUD_250000      = 15,   //DMX baud rate for break, 250 kBbd used for data frames
    UART_enBAUD_230400      = 16,   //IO Link COM3
} UART_EBaudRate;

typedef enum UART_EParity_
{
    UART_enPARITY_INVALID   = 0,
    UART_enPARITY_NONE      = 1,
    UART_enPARITY_EVEN      = 2,
    UART_enPARITY_ODD       = 3,
} UART_EParity;

typedef enum UART_EStopBit_
{
    UART_enSTOPBIT_INVALID  = 0,
    UART_enSTOPBIT_1        = 1,
    UART_enSTOPBIT_2        = 2,
} UART_EStopBit;

typedef enum UART_EDataLen_
{
    UART_enDATA_LEN_INVALID = 0,
    UART_enDATA_LEN_7       = 1,
    UART_enDATA_LEN_8       = 2,
} UART_EDataLen;

typedef enum UART_EFlowControl_
{
    UART_enFLOWCTRL_INVALID = 0,
    UART_enFLOWCTRL_NONE    = 1,
} UART_EFlowControl;

typedef enum UART_ERecError_
{
    UART_enERR_INVALID	= 0x0000,
    UART_enERR_PARITY	= 0x0001,
    UART_enERR_FRAME	= 0x0002,
    UART_enERR_NOISE	= 0x0004,
    UART_enERR_OVERRUN	= 0x0008
} UART_ERecError;

typedef enum UART_ERS485_
{
    UART_enRS485_INVALID,
    UART_enRS485_DISABLE,
    UART_enRS485_ENABLE
} UART_ERS485;

typedef struct UART_TConfig_
{
    UART_EBaudRate		enBaudRate;
    UART_EParity		enParity;
    UART_EStopBit		enStopBit;
    UART_EDataLen		enDataLen;
    UART_EFlowControl	enFlowControl;
    UART_ERS485			enRS485;
    void (*cbReceive)	(INT8U i8uChar_p);				///< Receive callback
    TBOOL (*cbTransmit)	(void);							///< Transmit callback
    void (*cbError)		(UART_ERecError enRecError_p);	///< Error callback
} UART_TConfig;

typedef enum UART_EBufferSendState_
{
    UART_enBSS_INVALID,                 //!< Initialization value
    UART_enBSS_IN_PROGRESS,             //!< Buffer transmition is still in progress
    UART_enBSS_SEND_OK,                 //!< Buffer succesfull transmited
    UART_enBSS_SEND_ERROR               //!< Error occured during buffer transmision
} UART_EBufferSendState;                //!< Buffer Send State


#define UART_PORT_1         0
#define UART_PORT_2         1
#define UART_PORT_3         2
#define UART_PORT_4         3
#define UART_PORT_5         4
#define UART_PORT_6         5
#define UART_PORT_7         6
#define UART_PORT_8         7


//+=============================================================================
//|	Prototype:	<name of function prototype>
//+=============================================================================
#ifdef __cplusplus
extern "C" { 
#endif 


extern INT32U UART_init (INT8U i8uPort_p, const UART_TConfig *tConf_p);
extern INT32U UART_putChar (INT8U i8uPort_p, INT8U i8uChar_p);
extern INT32U UART_sendBuffer (INT8U i8uPort_p, const INT8U *pi8uData_p, INT16U i16uDataLen_p, 
  UART_EBufferSendState *penState_p);
extern void UART_close (INT8U i8uPort_p);
                                            
void UART_uart1IntHandler (void);
void UART_uart2IntHandler (void);
void UART_uart3IntHandler (void);
void UART_uart4IntHandler (void);
void UART_uart5IntHandler (void);
void UART_uart6IntHandler (void);
void UART_uart7IntHandler (void);
void UART_uart8IntHandler (void);

#ifdef __cplusplus
}    
#endif 


#endif //UART_H_INC


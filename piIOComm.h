#pragma once

#include <common_define.h>
#include <IoProtocol.h>

#define REV_PI_IO_TIMEOUT           100         // msec
#define REV_PI_RECV_BUFFER_SIZE     100

#define REV_PI_RECV_IO_HEADER_LEN	65530

typedef enum _EGpioValue
{
    enGpioValue_Low  = 0,
    enGpioValue_High = 1
} EGpioValue;
typedef enum _EGpioMode
{
    enGpioMode_Input,
    enGpioMode_Output,
} EGpioMode;

extern struct file *piIoComm_fd_m;
extern int piIoComm_timeoutCnt_m;

int piIoComm_open_serial(void);
int piIoComm_send(INT8U *buf_p, INT16U i16uLen_p);
int piIoComm_recv(INT8U *buf_p, INT16U i16uLen_p);
int UartThreadProc ( void *pArg);

INT8U piIoComm_Crc8(INT8U *pi8uFrame_p, INT16U i16uLen_p);

int  piIoComm_init(void);
void piIoComm_finish(void);

void piIoComm_writeSniff1A(EGpioValue eVal_p, EGpioMode eMode_p);
void piIoComm_writeSniff1B(EGpioValue eVal_p, EGpioMode eMode_p);
void piIoComm_writeSniff2A(EGpioValue eVal_p, EGpioMode eMode_p);
void piIoComm_writeSniff2B(EGpioValue eVal_p, EGpioMode eMode_p);
void piIoComm_writeSniff(int pin, EGpioValue eVal_p, EGpioMode eMode_p);
EGpioValue piIoComm_readSniff1A(void);
EGpioValue piIoComm_readSniff1B(void);
EGpioValue piIoComm_readSniff2A(void);
EGpioValue piIoComm_readSniff2B(void);
EGpioValue piIoComm_readSniff(int pin);

INT32S piIoComm_sendRS485Tel(INT16U i16uCmd_p, INT8U i8uAdress_p,
    INT8U *pi8uSendData_p, INT8U i8uSendDataLen_p,
    INT8U *pi8uRecvData_p, INT8U i8uRecvDataLen_p);

INT32S piIoComm_sendTelegram(SIOGeneric * pRequest_p, SIOGeneric * pResponse_p);
INT32S piIoComm_gotoGateProtocol(void);
INT32S piIoComm_gotoFWUMode(int address);
INT32S piIoComm_setSerNum(int address, INT32U serNum);
INT32S piIoComm_fwuReset(int address);


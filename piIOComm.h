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

#pragma once

#include <common_define.h>
#include <IoProtocol.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio/machine.h>

#define REV_PI_IO_TIMEOUT           10         // msec
#define REV_PI_RECV_BUFFER_SIZE     100

#define REV_PI_RECV_IO_HEADER_LEN	65530

#define REV_PI_TTY_DEVICE	"/dev/ttyAMA0"

enum IOSTATE {
    /* physically not connected */
    IOSTATE_OFFLINE   = 0x00,
    /* cyclical data exchange is active */
    IOSTATE_CYCLIC_IO = 0x01,
};


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
int piIoComm_recv(INT8U *buf_p, INT16U i16uLen_p);	// using default timeout REV_PI_IO_TIMEOUT
int piIoComm_recv_timeout(INT8U * buf_p, INT16U i16uLen_p, INT16U timeout_p);
bool piIoComm_response_valid(SIOGeneric *resp, u8 expected_addr,
			     u8 expected_len);
int UartThreadProc ( void *pArg);

INT8U piIoComm_Crc8(INT8U *pi8uFrame_p, INT16U i16uLen_p);

int  piIoComm_init(void);
void piIoComm_finish(void);

void piIoComm_writeSniff1A(EGpioValue eVal_p, EGpioMode eMode_p);
void piIoComm_writeSniff1B(EGpioValue eVal_p, EGpioMode eMode_p);
void piIoComm_writeSniff2A(EGpioValue eVal_p, EGpioMode eMode_p);
void piIoComm_writeSniff2B(EGpioValue eVal_p, EGpioMode eMode_p);
void piIoComm_writeSniff(struct gpio_desc *, EGpioValue eVal_p, EGpioMode eMode_p);
EGpioValue piIoComm_readSniff1A(void);
EGpioValue piIoComm_readSniff1B(void);
EGpioValue piIoComm_readSniff2A(void);
EGpioValue piIoComm_readSniff2B(void);
EGpioValue piIoComm_readSniff(struct gpio_desc *);

INT32S piIoComm_sendRS485Tel(INT16U i16uCmd_p, INT8U i8uAdress_p,
    INT8U *pi8uSendData_p, INT8U i8uSendDataLen_p,
    INT8U *pi8uRecvData_p, INT16U *pi16uRecvDataLen_p);

INT32S piIoComm_sendTelegram(SIOGeneric * pRequest_p, SIOGeneric * pResponse_p);
INT32S piIoComm_gotoGateProtocol(void);
INT32S piIoComm_gotoFWUMode(int address);
INT32S piIoComm_fwuSetSerNum(int address, INT32U serNum);
INT32S piIoComm_fwuFlashErase(int address);
INT32S piIoComm_fwuFlashWrite(int address, INT32U flashAddr, char *data, INT32U length);
INT32S piIoComm_fwuReset(int address);

void revpi_io_build_header(UIoProtocolHeader *hdr,
		unsigned char addr, unsigned char len, unsigned char cmd);

int revpi_io_talk(void *sndbuf, int sndlen, void *rcvbuf, int rcvlen);

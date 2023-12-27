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

#include <linux/gpio/consumer.h>

#include "common_define.h"
#include "IoProtocol.h"

#define REV_PI_IO_TIMEOUT           10         // msec

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

INT8U piIoComm_Crc8(INT8U *pi8uFrame_p, INT16U i16uLen_p);

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

INT32S piIoComm_gotoGateProtocol(void);

void revpi_io_build_header(UIoProtocolHeader *hdr,
		unsigned char addr, unsigned char len, unsigned char cmd);

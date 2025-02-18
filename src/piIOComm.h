/* SPDX-License-Identifier: GPL-2.0-only
 * SPDX-FileCopyrightText: 2016-2023 KUNBUS GmbH
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
int piIoComm_send(INT8U * buf_p, INT16U i16uLen_p);

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

u8 piIoComm_Crc8(u8 *pi8uFrame_p, u16 i16uLen_p);

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

s32 piIoComm_sendRS485Tel(u16 i16uCmd_p, u8 i8uAdress_p,
    u8 *pi8uSendData_p, u8 i8uSendDataLen_p,
    u8 *pi8uRecvData_p, u16 *pi16uRecvDataLen_p);

s32 piIoComm_gotoGateProtocol(void);

void revpi_io_build_header(UIoProtocolHeader *hdr,
		unsigned char addr, unsigned char len, unsigned char cmd);
int piIoComm_send(u8 * buf_p, u16 i16uLen_p);

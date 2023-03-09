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

#include <linux/module.h>	// included for all kernel modules
#include <linux/kernel.h>
#include <linux/pibridge_comm.h>

#include <project.h>
#include <common_define.h>
#include <RS485FwuCommand.h>
#include "compat.h"
#include "revpi_common.h"
#include "revpi_core.h"
#include "piIOComm.h"

int piIoComm_send(INT8U * buf_p, INT16U i16uLen_p)
{
	int written;

#ifdef DEBUG_SERIALCOMM
	if (i16uLen_p == 1) {
		pr_info("send %d: %02x\n", i16uLen_p, buf_p[0]);
	} else if (i16uLen_p == 2) {
		pr_info("send %d: %02x %02x\n", i16uLen_p, buf_p[0], buf_p[1]);
	} else if (i16uLen_p == 3) {
		pr_info("send %d: %02x %02x %02x\n", i16uLen_p, buf_p[0], buf_p[1], buf_p[2]);
	} else if (i16uLen_p == 4) {
		pr_info("send %d: %02x %02x %02x %02x\n", i16uLen_p, buf_p[0], buf_p[1], buf_p[2], buf_p[3]);
	} else if (i16uLen_p == 5) {
		pr_info("send %d: %02x %02x %02x %02x %02x\n", i16uLen_p, buf_p[0], buf_p[1], buf_p[2], buf_p[3], buf_p[4]);
	} else if (i16uLen_p == 6) {
		pr_info("send %d: %02x %02x %02x %02x %02x %02x\n", i16uLen_p, buf_p[0], buf_p[1], buf_p[2], buf_p[3], buf_p[4], buf_p[5]);
	} else if (i16uLen_p == 7) {
		pr_info("send %d: %02x %02x %02x %02x %02x %02x %02x\n", i16uLen_p, buf_p[0], buf_p[1], buf_p[2], buf_p[3], buf_p[4], buf_p[5], buf_p[6]);
	} else if (i16uLen_p == 8) {
		pr_info("send %d: %02x %02x %02x %02x %02x %02x %02x %02x\n", i16uLen_p, buf_p[0], buf_p[1], buf_p[2],
			buf_p[3], buf_p[4], buf_p[5], buf_p[6], buf_p[7]);
	} else {
		pr_info("send %d: %02x %02x %02x %02x %02x %02x %02x %02x %02x ...\n", i16uLen_p, buf_p[0], buf_p[1],
			buf_p[2], buf_p[3], buf_p[4], buf_p[5], buf_p[6], buf_p[7], buf_p[8]);
	}
	//pr_info("vfs_write(%d, %d, %d)\n", (int)piIoComm_fd_m, i16uLen_p, (int)piIoComm_fd_m->f_pos);
#endif
	/* First clear receive FIFO to remove stale data */
	pibridge_clear_fifo();

	written = pibridge_send(buf_p, i16uLen_p);
	if (written < 0) {
		pr_info_serial("pibridge_send error: %i\n", written);
		return written;
	} else if (written != i16uLen_p) {
		pr_info_serial("pibridge_send error: not all data written (%i/%i)\n",
			written, i16uLen_p);
		return -ETIMEDOUT;
	}

	return 0;
}

int piIoComm_recv(INT8U * buf_p, INT16U i16uLen_p)
{
#ifdef DEBUG_SERIALCOMM
	if (i16uLen_p == 1) {
		pr_info("recv %d: %02x\n", i16uLen_p, buf_p[0]);
	} else if (i16uLen_p == 2) {
		pr_info("recv %d: %02x %02x\n", i16uLen_p, buf_p[0], buf_p[1]);
	} else if (i16uLen_p == 3) {
		pr_info("recv %d: %02x %02x %02x\n", i16uLen_p, buf_p[0], buf_p[1], buf_p[2]);
	} else if (i16uLen_p == 4) {
		pr_info("recv %d: %02x %02x %02x %02x\n", i16uLen_p, buf_p[0], buf_p[1], buf_p[2], buf_p[3]);
	} else if (i16uLen_p == 5) {
		pr_info("recv %d: %02x %02x %02x %02x %02x\n", i16uLen_p, buf_p[0], buf_p[1], buf_p[2], buf_p[3], buf_p[4]);
	} else if (i16uLen_p == 6) {
		pr_info("recv %d: %02x %02x %02x %02x %02x %02x\n", i16uLen_p, buf_p[0], buf_p[1], buf_p[2], buf_p[3], buf_p[4], buf_p[5]);
	} else if (i16uLen_p == 7) {
		pr_info("recv %d: %02x %02x %02x %02x %02x %02x %02x\n", i16uLen_p, buf_p[0], buf_p[1], buf_p[2], buf_p[3], buf_p[4], buf_p[5], buf_p[6]);
	} else if (i16uLen_p == 8) {
		pr_info("recv %d: %02x %02x %02x %02x %02x %02x %02x %02x\n", i16uLen_p, buf_p[0], buf_p[1],
			buf_p[2], buf_p[3], buf_p[4], buf_p[5], buf_p[6], buf_p[7]);
	} else {
		pr_info("recv %d: %02x %02x %02x %02x %02x %02x %02x %02x %02x ...\n", i16uLen_p, buf_p[0],
			buf_p[1], buf_p[2], buf_p[3], buf_p[4], buf_p[5], buf_p[6], buf_p[7], buf_p[8]);
	}
#endif
	return piIoComm_recv_timeout(buf_p, i16uLen_p, REV_PI_IO_TIMEOUT);
}

int piIoComm_recv_timeout(INT8U * buf_p, INT16U i16uLen_p, INT16U timeout_p)
{
	return pibridge_recv_timeout(buf_p, i16uLen_p, timeout_p);
}

INT8U piIoComm_Crc8(INT8U * pi8uFrame_p, INT16U i16uLen_p)
{
	INT8U i8uRv_l = 0;

	while (i16uLen_p--) {
		i8uRv_l = i8uRv_l ^ pi8uFrame_p[i16uLen_p];
	}
	return i8uRv_l;
}

bool piIoComm_response_valid(SIOGeneric *resp, u8 expected_addr,
			     u8 expected_len)
{
#ifdef DEBUG_DEVICE_IO
	static unsigned long good, bad;
#endif

	if (resp->uHeader.sHeaderTyp1.bitAddress != expected_addr) {
		pr_info_io("dev %2hhu: expected packet from dev %hhu%s%s\n",
			   resp->uHeader.sHeaderTyp1.bitAddress, expected_addr,
			   resp->uHeader.sHeaderTyp1.bitLength != expected_len
			   ? " / len_mismatch" : "",
			   resp->ai8uData[expected_len] != piIoComm_Crc8(
							   (INT8U *)resp,
				 IOPROTOCOL_HEADER_LENGTH + expected_len)
			   ? " / crc_mismatch" : "");
		goto err;
	}

	if (resp->uHeader.sHeaderTyp1.bitReqResp != 1) {
		pr_info_io("dev %2hhu: received request, expected response%s%s\n",
			   resp->uHeader.sHeaderTyp1.bitAddress,
			   resp->uHeader.sHeaderTyp1.bitLength != expected_len
			   ? " / len_mismatch" : "",
			   resp->ai8uData[expected_len] != piIoComm_Crc8(
							   (INT8U *)resp,
				 IOPROTOCOL_HEADER_LENGTH + expected_len)
			   ? " / crc_mismatch" : "");
		goto err;
	}

	if (resp->uHeader.sHeaderTyp1.bitLength != expected_len) {
		pr_info_io("dev %2hhu: received %hhu, expected %hhu bytes\n",
			   resp->uHeader.sHeaderTyp1.bitAddress,
			   resp->uHeader.sHeaderTyp1.bitLength, expected_len);
		goto err;
	}

	if (resp->ai8uData[expected_len] != piIoComm_Crc8((INT8U *)resp,
	    IOPROTOCOL_HEADER_LENGTH + expected_len)) {
		pr_info_io("dev %2hhu: invalid crc\n",
			   resp->uHeader.sHeaderTyp1.bitAddress);
		goto err;
	}

#ifdef DEBUG_DEVICE_IO
	good++;
#endif
	return true;

err:
	pr_info_io("packet: %*ph\n", resp->uHeader.sHeaderTyp1.bitLength +
		   IOPROTOCOL_HEADER_LENGTH + 1, resp);
#ifdef DEBUG_DEVICE_IO
	bad++;
	if ((bad % 1000) == 0)
		pr_info("dev all: ioprotocol statistics: %lu errors, %lu good\n",
			bad, good);
#endif
	return false;
}

void piIoComm_writeSniff1A(EGpioValue eVal_p, EGpioMode eMode_p)
{
#ifdef DEBUG_GPIO
	pr_info("sniff1A: mode %d value %d\n", (int)eMode_p, (int)eVal_p);
#endif
	piIoComm_writeSniff(piCore_g.gpio_sniff1a, eVal_p, eMode_p);
}

void piIoComm_writeSniff1B(EGpioValue eVal_p, EGpioMode eMode_p)
{
	if (!piDev_g.only_left_pibridge) {
		piIoComm_writeSniff(piCore_g.gpio_sniff1b, eVal_p, eMode_p);
#ifdef DEBUG_GPIO
		pr_info("sniff1B: mode %d value %d\n", (int)eMode_p, (int)eVal_p);
#endif
	}
}

void piIoComm_writeSniff2A(EGpioValue eVal_p, EGpioMode eMode_p)
{
#ifdef DEBUG_GPIO
	pr_info("sniff2A: mode %d value %d\n", (int)eMode_p, (int)eVal_p);
#endif
	piIoComm_writeSniff(piCore_g.gpio_sniff2a, eVal_p, eMode_p);
}

void piIoComm_writeSniff2B(EGpioValue eVal_p, EGpioMode eMode_p)
{
	if (!piDev_g.only_left_pibridge) {
		piIoComm_writeSniff(piCore_g.gpio_sniff2b, eVal_p, eMode_p);
#ifdef DEBUG_GPIO
		pr_info("sniff2B: mode %d value %d\n", (int)eMode_p, (int)eVal_p);
#endif
	}
}

void piIoComm_writeSniff(struct gpio_desc *pGpio, EGpioValue eVal_p, EGpioMode eMode_p)
{
	if (eMode_p == enGpioMode_Input) {
		gpiod_direction_input(pGpio);
	} else {
		gpiod_direction_output(pGpio, eVal_p);
		gpiod_set_value_cansleep(pGpio, eVal_p);
	}
}

EGpioValue piIoComm_readSniff1A()
{
	EGpioValue v = piIoComm_readSniff(piCore_g.gpio_sniff1a);
#ifdef DEBUG_GPIO
	pr_info("sniff1A: input value %d\n", (int)v);
#endif
	return v;
}

EGpioValue piIoComm_readSniff1B()
{
	if (!piDev_g.only_left_pibridge) {
		EGpioValue v = piIoComm_readSniff(piCore_g.gpio_sniff1b);
#ifdef DEBUG_GPIO
		pr_info("sniff1B: input value %d\n", (int)v);
#endif
		return v;
	}
	return enGpioValue_Low;
}

EGpioValue piIoComm_readSniff2A()
{
	EGpioValue v = piIoComm_readSniff(piCore_g.gpio_sniff2a);
#ifdef DEBUG_GPIO
	pr_info("sniff2A: input value %d\n", (int)v);
#endif
	return v;
}

EGpioValue piIoComm_readSniff2B()
{
	if (!piDev_g.only_left_pibridge) {
		EGpioValue v = piIoComm_readSniff(piCore_g.gpio_sniff2b);
#ifdef DEBUG_GPIO
		pr_info("sniff2B: input value %d\n", (int)v);
#endif
		return v;
	}
	return enGpioValue_Low;
}

EGpioValue piIoComm_readSniff(struct gpio_desc * pGpio)
{
	EGpioValue ret = enGpioValue_Low;

	if (gpiod_get_value_cansleep(pGpio))
		ret = enGpioValue_High;

	return ret;
}

INT32S piIoComm_sendRS485Tel(INT16U i16uCmd_p, INT8U i8uAddress_p,
			     INT8U * pi8uSendData_p, INT8U i8uSendDataLen_p, INT8U * pi8uRecvData_p, INT16U * pi16uRecvDataLen_p)
{
	u16 timeout = REV_PI_IO_TIMEOUT;
	unsigned int rcvlen;
	int ret;

	rcvlen = (pi16uRecvDataLen_p != NULL) ? *pi16uRecvDataLen_p : 0;

	// this starts a flash erase on the master module
	// this usually take longer than the normal timeout
	// -> increase the timeout value to 1s
	if (i8uSendDataLen_p > 0 && pi8uSendData_p[0] == 'F')
		timeout = 1000; // ms

	ret = pibridge_req_gate_tmt(i8uAddress_p, i16uCmd_p, pi8uSendData_p,
				    i8uSendDataLen_p, pi8uRecvData_p,
				    rcvlen, timeout);
	if (ret != rcvlen) {
		if (ret >= 0)
			ret = -EIO;
		pr_info_serial("Error sending gate request: %i\n", ret);
		return ret;
	}

	return 0;
}

INT32S piIoComm_gotoGateProtocol(void)
{
	SIOGeneric sRequest_l;
	INT8U len_l;
	int ret;

	len_l = 0;

	sRequest_l.uHeader.sHeaderTyp2.bitCommand = IOP_TYP2_CMD_GOTO_GATE_PROTOCOL;
	sRequest_l.uHeader.sHeaderTyp2.bitIoHeaderType = 1;
	sRequest_l.uHeader.sHeaderTyp2.bitReqResp = 0;
	sRequest_l.uHeader.sHeaderTyp2.bitLength = len_l;
	sRequest_l.uHeader.sHeaderTyp2.bitDataPart1 = 0;

	sRequest_l.ai8uData[len_l] = piIoComm_Crc8((INT8U *) & sRequest_l, IOPROTOCOL_HEADER_LENGTH + len_l);

	ret = piIoComm_send((INT8U *) & sRequest_l, IOPROTOCOL_HEADER_LENGTH + len_l + 1);
	if (ret == 0) {
		// there is no reply
	} else {
#ifdef DEBUG_DEVICE_IO
		pr_info("dev all: send ioprotocol send error %d\n", ret);
#endif
	}
	return 0;
}

INT32S piIoComm_gotoFWUMode(int address)
{
	return fwuEnterFwuMode(address);
}

INT32S piIoComm_fwuSetSerNum(int address, INT32U serNum)
{
	return fwuWriteSerialNum(address, serNum);
}

INT32S piIoComm_fwuFlashErase(int address)
{
	return fwuEraseFlash(address);
}

INT32S piIoComm_fwuFlashWrite(int address, INT32U flashAddr, char *data, INT32U length)
{
	return fwuWrite(address, flashAddr, data, length);
}

INT32S piIoComm_fwuReset(int address)
{
	return fwuResetModule(address);
}

void revpi_io_build_header(UIoProtocolHeader *hdr,
		unsigned char addr, unsigned char len, unsigned char cmd)
{
	hdr->sHeaderTyp1.bitAddress = addr;
	hdr->sHeaderTyp1.bitIoHeaderType = 0;
	hdr->sHeaderTyp1.bitReqResp = 0;
	hdr->sHeaderTyp1.bitLength = len;
	hdr->sHeaderTyp1.bitCommand = cmd;
}

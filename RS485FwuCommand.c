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


#include <linux/delay.h>
#include <linux/pibridge_comm.h>

#include "ModGateRS485.h"
#include "piIOComm.h"
#include "RS485FwuCommand.h"


////*************************************************************************************************
INT32S fwuEnterFwuMode (INT8U address)
{
	int ret;

	/* Ignore the response for this command */
	ret = pibridge_req_send_gate(address, eCmdSetFwUpdateMode, NULL, 0);
	if (ret)
		return ret;

	msleep(100);
	return 0;
}


////*************************************************************************************************
INT32S fwuWriteSerialNum (
    INT8U address,
    INT32U i32uSerNum_p)

{
	int ret;
	u16 err;

	/*
	 * Writing the serial number to flash takes time.
	 * Allow an extra 100 msec before expecting a response.
	 * The response consists of an error code which is 0 on success.
	 */
	ret = pibridge_req_gate_tmt(address, eCmdWriteSerialNumber,
				    &i32uSerNum_p, sizeof(i32uSerNum_p),
				    &err, sizeof(err),
				    REV_PI_IO_TIMEOUT + 100);
	if (ret < 0)
		return ret;

	if (ret < sizeof(err)) {
		pr_warn("Truncated WriteSerialNumber response (addr %hhu)\n",
			address);
		return -EIO;
	}

	if (err) {
		pr_warn("Error in WriteSerialNumber response (addr %hhu: 0x%04x)\n",
			address, err);
		return -EIO;
	}

	return 0;
}


INT32S fwuEraseFlash (INT8U address)
{
	int ret;
	u16 err;

	/*
	 * Allow 6 sec before expecting a response.
	 * The response consists of an error code which is 0 on success.
	 */
	ret = pibridge_req_gate_tmt(address, eCmdEraseFwFlash,
				    NULL, 0,
				    &err, sizeof(err),
				    6000);
	if (ret < 0)
		return ret;

	if (ret < sizeof(err)) {
		pr_warn("Truncated EraseFwFlash response (addr %hhu)\n",
			address);
		return -EIO;
	}

	if (err) {
		pr_warn("Error in EraseFwFlash response (addr %hhu: 0x%04x)\n",
			address, err);
		return -EIO;
	}

	return 0;
}

INT32S fwuWrite(INT8U address, INT32U flashAddr, char *data, INT32U length)
{
	INT8U ai8uSendBuf_l[MAX_TELEGRAM_DATA_SIZE];
	int ret;
	u16 err;

	memcpy (ai8uSendBuf_l, &flashAddr, sizeof (flashAddr));
	if (length == 0 || length > MAX_TELEGRAM_DATA_SIZE - sizeof(flashAddr))
		return -14;

	memcpy (ai8uSendBuf_l + sizeof (flashAddr), data, length);

	/*
	 * Allow 1 sec before expecting a response.
	 * The response consists of an error code which is 0 on success.
	 */
	ret = pibridge_req_gate_tmt(address, eCmdWriteFwFlash,
				    ai8uSendBuf_l, sizeof(flashAddr) + length,
				    &err, sizeof(err),
				    1000);
	if (ret < 0)
		return ret;

	if (ret < sizeof(err)) {
		pr_warn("Truncated WriteFwFlash response (addr %hhu)\n",
			address);
		return -EIO;
	}

	if (err) {
		pr_warn("Error in WriteFwFlash response (addr %hhu: 0x%04x)\n",
			address, err);
		return -EIO;
	}

	return 0;
}

INT32S fwuResetModule (INT8U address)
{
	int ret;

	/* There is no response for this command. */
	ret = pibridge_req_send_gate(address, eCmdResetModule, NULL, 0);
	if (ret)
		return ret;

	msleep(10000);
	return 0;
}

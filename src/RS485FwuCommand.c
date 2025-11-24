// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2017-2024 KUNBUS GmbH

// Firmware update of RevPi modules using gateway protocol over RS-485

#include <linux/delay.h>
#include <linux/pibridge_comm.h>

#include "ModGateRS485.h"
#include "revpi_core.h"
#include "piIOComm.h"
#include "RS485FwuCommand.h"

int fwuEnterFwuMode(u8 address)
{
	int ret;

	/* Ignore the response for this command */
	ret = pibridge_req_send_gate(piCore_g.pibridge, address,
				     eCmdSetFwUpdateMode, NULL, 0);
	if (ret)
		return ret;

	msleep(100);
	return 0;
}

int fwuWriteSerialNum (u8 address, u32 sernum)
{
	int ret;
	u16 err;

	/*
	 * Writing the serial number to flash takes time.
	 * Allow an extra 100 msec before expecting a response.
	 * The response consists of an error code which is 0 on success.
	 */
	ret = pibridge_req_gate_tmt(piCore_g.pibridge, address,
				    eCmdWriteSerialNumber, &sernum,
				    sizeof(sernum), &err, sizeof(err),
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


int fwuEraseFlash (u8 address)
{
	int ret;
	u16 err;

	/*
	 * Allow 6 sec before expecting a response.
	 * The response consists of an error code which is 0 on success.
	 */
	ret = pibridge_req_gate_tmt(piCore_g.pibridge, address,
				    eCmdEraseFwFlash, NULL, 0, &err,
				    sizeof(err), 6000);
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

int fwuWrite(u8 address, u32 flashAddr, char *data, u32 length)
{
	u8 sendbuf[MAX_TELEGRAM_DATA_SIZE];
	int ret;
	u16 err;

	memcpy (sendbuf, &flashAddr, sizeof (flashAddr));
	if (length == 0 || length > MAX_TELEGRAM_DATA_SIZE - sizeof(flashAddr))
		return -EINVAL;

	memcpy (sendbuf + sizeof (flashAddr), data, length);

	/*
	 * Allow 1 sec before expecting a response.
	 * The response consists of an error code which is 0 on success.
	 */
	ret = pibridge_req_gate_tmt(piCore_g.pibridge, address,
				    eCmdWriteFwFlash, sendbuf,
				    sizeof(flashAddr) + length, &err,
				    sizeof(err), 1000);
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

int fwuResetModule (u8 address)
{
	int ret;

	/* There is no response for this command. */
	ret = pibridge_req_send_gate(piCore_g.pibridge, address,
				     eCmdResetModule, NULL, 0);
	if (ret)
		return ret;

	msleep(10000);
	return 0;
}

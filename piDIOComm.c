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

#include <project.h>
#include <common_define.h>
#include <linux/pibridge_comm.h>

#include "revpi_common.h"
#include "revpi_core.h"

#define DIO_OUTPUT_DATA_LEN		18

static INT8U i8uConfigured_s = 0;
static SDioConfig dioConfig_s[10];
static INT8U i8uNumCounter[64];
static INT16U i16uCounterAct[64];

void piDIOComm_InitStart(void)
{
	i8uConfigured_s = 0;
}

INT32U piDIOComm_Config(uint8_t i8uAddress, uint16_t i16uNumEntries, SEntryInfo * pEnt)
{
	uint16_t i;

	if (i8uConfigured_s >= sizeof(dioConfig_s) / sizeof(SDioConfig)) {
		pr_err("max. number of DIOs reached\n");
		return -1;
	}

	pr_info_dio("piDIOComm_Config addr %d entries %d  num %d\n", i8uAddress, i16uNumEntries, i8uConfigured_s);
	memset(&dioConfig_s[i8uConfigured_s], 0, sizeof(SDioConfig));

	dioConfig_s[i8uConfigured_s].uHeader.sHeaderTyp1.bitAddress = i8uAddress;
	dioConfig_s[i8uConfigured_s].uHeader.sHeaderTyp1.bitIoHeaderType = 0;
	dioConfig_s[i8uConfigured_s].uHeader.sHeaderTyp1.bitReqResp = 0;
	dioConfig_s[i8uConfigured_s].uHeader.sHeaderTyp1.bitLength = sizeof(SDioConfig) - IOPROTOCOL_HEADER_LENGTH - 1;
	dioConfig_s[i8uConfigured_s].uHeader.sHeaderTyp1.bitCommand = IOP_TYP1_CMD_CFG;

	i8uNumCounter[i8uAddress] = 0;
	i16uCounterAct[i8uAddress] = 0;

	for (i = 0; i < i16uNumEntries; i++) {
		pr_info_dio("addr %2d  type %d  len %3d  offset %3d  value %d 0x%x\n",
			    pEnt[i].i8uAddress, pEnt[i].i8uType, pEnt[i].i16uBitLength, pEnt[i].i16uOffset,
			    pEnt[i].i32uDefault, pEnt[i].i32uDefault);

		if (pEnt[i].i16uOffset >= 88 && pEnt[i].i16uOffset <= 103) {
			dioConfig_s[i8uConfigured_s].i32uInputMode |=
			    (pEnt[i].i32uDefault & 0x03) << ((pEnt[i].i16uOffset - 88) * 2);
			if ((pEnt[i].i32uDefault == 1 || pEnt[i].i32uDefault == 2)
			    || (pEnt[i].i32uDefault == 3 && ((pEnt[i].i16uOffset - 88) % 2) == 0)) {
				i8uNumCounter[i8uAddress]++;
				i16uCounterAct[i8uAddress] |= (1 << (pEnt[i].i16uOffset - 88));
			}
		} else {
			switch (pEnt[i].i16uOffset) {
			case 104:
				dioConfig_s[i8uConfigured_s].i8uInputDebounce = pEnt[i].i32uDefault;
				break;
			case 106:
				dioConfig_s[i8uConfigured_s].i16uOutputPushPull = pEnt[i].i32uDefault;
				break;
			case 108:
				dioConfig_s[i8uConfigured_s].i16uOutputOpenLoadDetect = pEnt[i].i32uDefault;
				break;
			case 110:
				dioConfig_s[i8uConfigured_s].i16uOutputPWM = pEnt[i].i32uDefault;
				break;
			case 112:
				dioConfig_s[i8uConfigured_s].i8uOutputPWMIncrement = pEnt[i].i32uDefault;
				break;
			}
		}
	}
	dioConfig_s[i8uConfigured_s].i8uCrc =
	    piIoComm_Crc8((INT8U *) & dioConfig_s[i8uConfigured_s], sizeof(SDioConfig) - 1);

	pr_info_dio("piDIOComm_Config done addr %d input mode %08x  numCnt %d\n", i8uAddress,
		    dioConfig_s[i8uConfigured_s].i32uInputMode, i8uNumCounter[i8uAddress]);
	i8uConfigured_s++;

	return 0;
}

INT32U piDIOComm_Init(INT8U i8uDevice_p)
{
	u8 snd_len = sizeof(SDioConfig) - IOPROTOCOL_HEADER_LENGTH - 1;
	u8 addr = RevPiDevice_getDev(i8uDevice_p)->i8uAddress;
	UIoProtocolHeader *hdr;
	u8 *snd_buf;
	int ret;
	int i;

	pr_info_dio("piDIOComm_Init %d of %d  addr %d numCnt %d\n", i8uDevice_p,
		    i8uConfigured_s, addr, i8uNumCounter[addr]);

	ret = 4;  // unknown device

	for (i = 0; i < i8uConfigured_s; i++) {
		hdr = &dioConfig_s[i].uHeader;

		if (hdr->sHeaderTyp1.bitAddress == addr) {
			snd_buf = ((u8 *) &dioConfig_s[i]) +
				  IOPROTOCOL_HEADER_LENGTH;

			ret = pibridge_req_io(addr, IOP_TYP1_CMD_CFG, snd_buf,
					      snd_len, NULL, 0);
			break;
		}
	}

	return ret;
}

INT32U piDIOComm_sendCyclicTelegram(u8 devnum)
{
	static u8 last_out[40][DIO_OUTPUT_DATA_LEN];
	u8 in_buf[IOPROTOCOL_MAXDATA_LENGTH];
	u8 out_buf[DIO_OUTPUT_DATA_LEN];
	u8 snd_buf[DIO_OUTPUT_DATA_LEN];
	SDevice *revpi_dev;
	u8 data_in[70];
	u8 snd_len;
	u8 rcv_len;
	u8 addr;
	int ret;
	u8 cmd;
	u8 i;
	u8 j;

	revpi_dev = RevPiDevice_getDev(devnum);

	if (revpi_dev->sId.i16uFBS_OutputLength != DIO_OUTPUT_DATA_LEN)
		return 4;

	addr = revpi_dev->i8uAddress;

	if (!test_bit(PICONTROL_DEV_FLAG_STOP_IO, &piDev_g.flags)) {
		rt_mutex_lock(&piDev_g.lockPI);
		memcpy(out_buf, piDev_g.ai8uPI + revpi_dev->i16uOutputOffset,
		       DIO_OUTPUT_DATA_LEN);
		rt_mutex_unlock(&piDev_g.lockPI);
	} else {
		memset(out_buf, 0, sizeof(out_buf));
	}

	/* check if any PWM values have changed since last cycle */
	if (!memcmp(out_buf + 2, last_out[addr] + 2, DIO_OUTPUT_DATA_LEN - 2)) {
		// only the direct output pins have changed
		snd_len = sizeof(u16);
		cmd = IOP_TYP1_CMD_DATA;
		memcpy(snd_buf, out_buf, snd_len);
	} else {
		struct pwm_data *pwm = (struct pwm_data *) snd_buf;

		memcpy(&pwm->output, out_buf, sizeof(u16));
		// copy all PWM values that have changed
		pwm->channels = 0;
		j = 0;
		for (i = 0; i < 16; i++) {
			if (last_out[addr][i + 2] != out_buf[i + 2]) {
				pwm->channels |= 1 << i;
				pwm->value[j] = out_buf[i + 2];
				j++;
			}
		}
		snd_len = j + 4;
		cmd = IOP_TYP1_CMD_DATA2;
	}

	memcpy(last_out[addr], out_buf, sizeof(out_buf));

	rcv_len = 3 * sizeof(u16) + i8uNumCounter[addr] * sizeof(u32);

	ret = pibridge_req_io(addr, cmd, snd_buf, snd_len, in_buf, rcv_len);
	if (ret != rcv_len) {
		if (ret > 0)
			ret = -EIO;
		pr_warn_ratelimited("addr %2d: io communication failed %d\n",
			addr, ret);
		return 1;
	}

	memcpy(&data_in[0], in_buf, 3 * sizeof(u16));
	memset(&data_in[6], 0, 64);

	j = 0;
	for (i = 0; i < 16; i++) {
		if (i16uCounterAct[addr] & (1 << i)) {
			memcpy(&data_in[3 * sizeof(u16) + i * sizeof(u32)],
			       &in_buf[3 * sizeof(u16) + j * sizeof(u32)],
			       sizeof(u32));
			j++;
		}
	}

	rt_mutex_lock(&piDev_g.lockPI);
	memcpy(piDev_g.ai8uPI + revpi_dev->i16uInputOffset, data_in,
	       sizeof(data_in));
	rt_mutex_unlock(&piDev_g.lockPI);

	return 0;
}

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
#include <linux/uaccess.h>
#include <linux/fcntl.h>
#include <linux/termios.h>
#include <linux/syscalls.h>
#include <asm/uaccess.h>
#include <asm/segment.h>
#include <linux/fs.h>
#include <linux/kthread.h>
#include <linux/gpio.h>

#include <project.h>
#include <common_define.h>
#include <ModGateRS485.h>

#include "revpi_common.h"
#include <PiBridgeMaster.h>
#include <RevPiDevice.h>
#include <piControlMain.h>

#include <IoProtocol.h>
#include <piIOComm.h>
#include <piAIOComm.h>

static INT8U i8uConfigured_s = 0;
static SAioConfig aioConfig_s[10];
static SAioInConfig aioIn1Config_s[10];
static SAioInConfig aioIn2Config_s[10];

void piAIOComm_InitStart(void)
{
	pr_info_aio("piAIOComm_InitStart\n");
	i8uConfigured_s = 0;
}

INT32U piAIOComm_Config(uint8_t i8uAddress, uint16_t i16uNumEntries, SEntryInfo * pEnt)
{
	uint16_t i;

	if (i8uConfigured_s >= sizeof(aioConfig_s) / sizeof(SAioConfig)) {
		pr_err("max. number of AIOs reached\n");
		return -1;
	}

	pr_info_aio("piAIOComm_Config addr %d entries %d  num %d\n", i8uAddress, i16uNumEntries, i8uConfigured_s);
	memset(&aioConfig_s[i8uConfigured_s], 0, sizeof(SAioConfig));

	aioConfig_s[i8uConfigured_s].uHeader.sHeaderTyp1.bitAddress = i8uAddress;
	aioConfig_s[i8uConfigured_s].uHeader.sHeaderTyp1.bitIoHeaderType = 0;
	aioConfig_s[i8uConfigured_s].uHeader.sHeaderTyp1.bitReqResp = 0;
	aioConfig_s[i8uConfigured_s].uHeader.sHeaderTyp1.bitLength = sizeof(SAioConfig) - IOPROTOCOL_HEADER_LENGTH - 1;
	aioConfig_s[i8uConfigured_s].uHeader.sHeaderTyp1.bitCommand = IOP_TYP1_CMD_CFG;

	aioIn1Config_s[i8uConfigured_s].uHeader.sHeaderTyp1.bitAddress = i8uAddress;
	aioIn1Config_s[i8uConfigured_s].uHeader.sHeaderTyp1.bitIoHeaderType = 0;
	aioIn1Config_s[i8uConfigured_s].uHeader.sHeaderTyp1.bitReqResp = 0;
	aioIn1Config_s[i8uConfigured_s].uHeader.sHeaderTyp1.bitLength = sizeof(SAioInConfig) - IOPROTOCOL_HEADER_LENGTH - 1;
	aioIn1Config_s[i8uConfigured_s].uHeader.sHeaderTyp1.bitCommand = IOP_TYP1_CMD_DATA2;

	aioIn2Config_s[i8uConfigured_s].uHeader.sHeaderTyp1.bitAddress = i8uAddress;
	aioIn2Config_s[i8uConfigured_s].uHeader.sHeaderTyp1.bitIoHeaderType = 0;
	aioIn2Config_s[i8uConfigured_s].uHeader.sHeaderTyp1.bitReqResp = 0;
	aioIn2Config_s[i8uConfigured_s].uHeader.sHeaderTyp1.bitLength = sizeof(SAioInConfig) - IOPROTOCOL_HEADER_LENGTH - 1;
	aioIn2Config_s[i8uConfigured_s].uHeader.sHeaderTyp1.bitCommand = IOP_TYP1_CMD_DATA3;


	for (i = 0; i < i16uNumEntries; i++) {
		pr_info_aio("addr %2d  type %d  len %3d  offset %3d  value %d 0x%x\n",
			    pEnt[i].i8uAddress, pEnt[i].i8uType, pEnt[i].i16uBitLength, pEnt[i].i16uOffset,
			    pEnt[i].i32uDefault, pEnt[i].i32uDefault);

		switch (pEnt[i].i16uOffset) {
		case AIO_OFFSET_InputValue_1:
		case AIO_OFFSET_InputValue_2:
		case AIO_OFFSET_InputValue_3:
		case AIO_OFFSET_InputValue_4:
		case AIO_OFFSET_InputStatus_1:
		case AIO_OFFSET_InputStatus_2:
		case AIO_OFFSET_InputStatus_3:
		case AIO_OFFSET_InputStatus_4:
		case AIO_OFFSET_RTDValue_1:
		case AIO_OFFSET_RTDValue_2:
		case AIO_OFFSET_RTDStatus_1:
		case AIO_OFFSET_RTDStatus_2:
		case AIO_OFFSET_Output_Status_1:
		case AIO_OFFSET_Output_Status_2:
		case AIO_OFFSET_OutputValue_1:
		case AIO_OFFSET_OutputValue_2:
			// nothing to do
			break;
		case AIO_OFFSET_Input1Range:
			aioIn1Config_s[i8uConfigured_s].sAioInputConfig[0].eInputRange = pEnt[i].i32uDefault;
			break;
		case AIO_OFFSET_Input1Factor:
			aioIn1Config_s[i8uConfigured_s].sAioInputConfig[0].i16sA1 = pEnt[i].i32uDefault;
			break;
		case AIO_OFFSET_Input1Divisor:
			aioIn1Config_s[i8uConfigured_s].sAioInputConfig[0].i16uA2 = pEnt[i].i32uDefault;
			break;
		case AIO_OFFSET_Input1Offset:
			aioIn1Config_s[i8uConfigured_s].sAioInputConfig[0].i16sB = pEnt[i].i32uDefault;
			break;
		case AIO_OFFSET_Input2Range:
			aioIn1Config_s[i8uConfigured_s].sAioInputConfig[1].eInputRange = pEnt[i].i32uDefault;
			break;
		case AIO_OFFSET_Input2Factor:
			aioIn1Config_s[i8uConfigured_s].sAioInputConfig[1].i16sA1 = pEnt[i].i32uDefault;
			break;
		case AIO_OFFSET_Input2Divisor:
			aioIn1Config_s[i8uConfigured_s].sAioInputConfig[1].i16uA2 = pEnt[i].i32uDefault;
			break;
		case AIO_OFFSET_Input2Offset:
			aioIn1Config_s[i8uConfigured_s].sAioInputConfig[1].i16sB = pEnt[i].i32uDefault;
			break;
		case AIO_OFFSET_Input3Range:
			aioIn2Config_s[i8uConfigured_s].sAioInputConfig[0].eInputRange = pEnt[i].i32uDefault;
			break;
		case AIO_OFFSET_Input3Factor:
			aioIn2Config_s[i8uConfigured_s].sAioInputConfig[0].i16sA1 = pEnt[i].i32uDefault;
			break;
		case AIO_OFFSET_Input3Divisor:
			aioIn2Config_s[i8uConfigured_s].sAioInputConfig[0].i16uA2 = pEnt[i].i32uDefault;
			break;
		case AIO_OFFSET_Input3Offset:
			aioIn2Config_s[i8uConfigured_s].sAioInputConfig[0].i16sB = pEnt[i].i32uDefault;
			break;
		case AIO_OFFSET_Input4Range:
			aioIn2Config_s[i8uConfigured_s].sAioInputConfig[1].eInputRange = pEnt[i].i32uDefault;
			break;
		case AIO_OFFSET_Input4Factor:
			aioIn2Config_s[i8uConfigured_s].sAioInputConfig[1].i16sA1 = pEnt[i].i32uDefault;
			break;
		case AIO_OFFSET_Input4Divisor:
			aioIn2Config_s[i8uConfigured_s].sAioInputConfig[1].i16uA2 = pEnt[i].i32uDefault;
			break;
		case AIO_OFFSET_Input4Offset:
			aioIn2Config_s[i8uConfigured_s].sAioInputConfig[1].i16sB = pEnt[i].i32uDefault;
			break;
		case AIO_OFFSET_InputSampleRate:
			aioConfig_s[i8uConfigured_s].i8uInputSampleRate = pEnt[i].i32uDefault;
			break;
		case AIO_OFFSET_RTD1Type:
			aioConfig_s[i8uConfigured_s].sAioRtdConfig[0].i8uSensorType = pEnt[i].i32uDefault;
			break;
		case AIO_OFFSET_RTD1Method:
			if (pEnt[i].i32uDefault == 1)
				aioConfig_s[i8uConfigured_s].sAioRtdConfig[0].i8uMeasureMethod = 1;	// 4 wire
			else
				aioConfig_s[i8uConfigured_s].sAioRtdConfig[0].i8uMeasureMethod = 0;	// 2 or 3 wire
			break;
		case AIO_OFFSET_RTD1Factor:
			aioConfig_s[i8uConfigured_s].sAioRtdConfig[0].i16sA1 = pEnt[i].i32uDefault;
			break;
		case AIO_OFFSET_RTD1Divisor:
			aioConfig_s[i8uConfigured_s].sAioRtdConfig[0].i16uA2 = pEnt[i].i32uDefault;
			break;
		case AIO_OFFSET_RTD1Offset:
			aioConfig_s[i8uConfigured_s].sAioRtdConfig[0].i16sB = pEnt[i].i32uDefault;
			break;
		case AIO_OFFSET_RTD2Type:
			aioConfig_s[i8uConfigured_s].sAioRtdConfig[1].i8uSensorType = pEnt[i].i32uDefault;
			break;
		case AIO_OFFSET_RTD2Method:
			if (pEnt[i].i32uDefault == 1)
				aioConfig_s[i8uConfigured_s].sAioRtdConfig[1].i8uMeasureMethod = 1;	// 4 wire
			else
				aioConfig_s[i8uConfigured_s].sAioRtdConfig[1].i8uMeasureMethod = 0;	// 2 or 3 wire
			break;
		case AIO_OFFSET_RTD2Factor:
			aioConfig_s[i8uConfigured_s].sAioRtdConfig[1].i16sA1 = pEnt[i].i32uDefault;
			break;
		case AIO_OFFSET_RTD2Divisor:
			aioConfig_s[i8uConfigured_s].sAioRtdConfig[1].i16uA2 = pEnt[i].i32uDefault;
			break;
		case AIO_OFFSET_RTD2Offset:
			aioConfig_s[i8uConfigured_s].sAioRtdConfig[1].i16sB = pEnt[i].i32uDefault;
			break;
		case AIO_OFFSET_Output1Range:
			aioConfig_s[i8uConfigured_s].sAioOutputConfig[0].eOutputRange = pEnt[i].i32uDefault;
			break;
		case AIO_OFFSET_Output1EnableSlew:
			aioConfig_s[i8uConfigured_s].sAioOutputConfig[0].bSlewRateEnabled = pEnt[i].i32uDefault;
			break;
		case AIO_OFFSET_Output1SlewStepSize:
			aioConfig_s[i8uConfigured_s].sAioOutputConfig[0].eSlewRateStepSize = pEnt[i].i32uDefault;
			break;
		case AIO_OFFSET_Output1SlewUpdateFreq:
			aioConfig_s[i8uConfigured_s].sAioOutputConfig[0].eSlewRateFrequency = pEnt[i].i32uDefault;
			break;
		case AIO_OFFSET_Output1Factor:
			aioConfig_s[i8uConfigured_s].sAioOutputConfig[0].i16sA1 = pEnt[i].i32uDefault;
			break;
		case AIO_OFFSET_Output1Divisor:
			aioConfig_s[i8uConfigured_s].sAioOutputConfig[0].i16uA2 = pEnt[i].i32uDefault;
			break;
		case AIO_OFFSET_Output1Offset:
			aioConfig_s[i8uConfigured_s].sAioOutputConfig[0].i16sB = pEnt[i].i32uDefault;
			break;
		case AIO_OFFSET_Output2Range:
			aioConfig_s[i8uConfigured_s].sAioOutputConfig[1].eOutputRange = pEnt[i].i32uDefault;
			break;
		case AIO_OFFSET_Output2EnableSlew:
			aioConfig_s[i8uConfigured_s].sAioOutputConfig[1].bSlewRateEnabled = pEnt[i].i32uDefault;
			break;
		case AIO_OFFSET_Output2SlewStepSize:
			aioConfig_s[i8uConfigured_s].sAioOutputConfig[1].eSlewRateStepSize = pEnt[i].i32uDefault;
			break;
		case AIO_OFFSET_Output2SlewUpdateFreq:
			aioConfig_s[i8uConfigured_s].sAioOutputConfig[1].eSlewRateFrequency = pEnt[i].i32uDefault;
			break;
		case AIO_OFFSET_Output2Factor:
			aioConfig_s[i8uConfigured_s].sAioOutputConfig[1].i16sA1 = pEnt[i].i32uDefault;
			break;
		case AIO_OFFSET_Output2Divisor:
			aioConfig_s[i8uConfigured_s].sAioOutputConfig[1].i16uA2 = pEnt[i].i32uDefault;
			break;
		case AIO_OFFSET_Output2Offset:
			aioConfig_s[i8uConfigured_s].sAioOutputConfig[1].i16sB = pEnt[i].i32uDefault;
			break;
		default:
			pr_err("piAIOComm_Config: Unknown parameter %d in rsc-file\n", pEnt[i].i16uOffset);
		}
	}
	aioConfig_s[i8uConfigured_s].i8uCrc =
	    piIoComm_Crc8((INT8U *) & aioConfig_s[i8uConfigured_s], sizeof(SAioConfig) - 1);

	aioIn1Config_s[i8uConfigured_s].i8uCrc =
	    piIoComm_Crc8((INT8U *) & aioIn1Config_s[i8uConfigured_s], sizeof(SAioInConfig) - 1);

	aioIn2Config_s[i8uConfigured_s].i8uCrc =
	    piIoComm_Crc8((INT8U *) & aioIn2Config_s[i8uConfigured_s], sizeof(SAioInConfig) - 1);

	i8uConfigured_s++;
	pr_info_aio("piAIOComm_Config done %d addr %d\n", i8uConfigured_s, i8uAddress);

	return 0;
}

INT32U piAIOComm_Init(INT8U i8uDevice_p)
{
	u8 addr = RevPiDevice_getDev(i8uDevice_p)->i8uAddress;
	int ret;
	SIOGeneric sResponse_l;
	INT8U i, len_l;

	pr_info_aio("piAIOComm_Init %d of %d  addr %d\n", i8uDevice_p, i8uConfigured_s,
		    RevPiDevice_getDev(i8uDevice_p)->i8uAddress);

	for (i = 0; i < i8uConfigured_s; i++) {
		if (aioConfig_s[i].uHeader.sHeaderTyp1.bitAddress == RevPiDevice_getDev(i8uDevice_p)->i8uAddress) {
			pr_info_aio("piAIOComm_Init send configIn1\n");
			ret = piIoComm_send((INT8U *) & aioIn1Config_s[i], sizeof(SAioInConfig));
			if (ret == 0) {
				len_l = 0;	// empty config telegram

				ret = piIoComm_recv((INT8U *) & sResponse_l, IOPROTOCOL_HEADER_LENGTH + len_l + 1);
				if (ret > 0) {
					if (piIoComm_response_valid(&sResponse_l, addr, len_l)) {
						pr_info_aio("piAIOComm_Init done configIn1\n");
						// success -> continue
					} else {
						return 1;	// wrong crc
					}
				} else {
					return 2;	// no response
				}
			} else {
				return 3;	// could not send
			}

			pr_info_aio("piAIOComm_Init send configIn2\n");
			ret = piIoComm_send((INT8U *) & aioIn2Config_s[i], sizeof(SAioInConfig));
			if (ret == 0) {
				len_l = 0;	// empty config telegram

				ret = piIoComm_recv((INT8U *) & sResponse_l, IOPROTOCOL_HEADER_LENGTH + len_l + 1);
				if (ret > 0) {
					if (piIoComm_response_valid(&sResponse_l, addr, len_l)) {
						pr_info_aio("piAIOComm_Init done configIn2\n");
						// success -> continue
					} else {
						return 1;	// wrong crc
					}
				} else {
					return 2;	// no response
				}
			} else {
				return 3;	// could not send
			}


			pr_info_aio("piAIOComm_Init send config\n");
			ret = piIoComm_send((INT8U *) & aioConfig_s[i], sizeof(SAioConfig));
			if (ret == 0) {
				len_l = 0;	// empty config telegram

				ret = piIoComm_recv((INT8U *) & sResponse_l, IOPROTOCOL_HEADER_LENGTH + len_l + 1);
				if (ret > 0) {
					if (piIoComm_response_valid(&sResponse_l, addr, len_l)) {
						pr_info_aio("piAIOComm_Init done config\n");
						return 0;	// success -> leave the function
					} else {
						return 1;	// wrong crc
					}
				} else {
					return 2;	// no response
				}
			} else {
				return 3;	// could not send
			}
		}
	}

	return 4;		// unknown device
}

INT32U piAIOComm_sendCyclicTelegram(INT8U i8uDevice_p)
{
	INT32U i32uRv_l = 0;
	SIOGeneric sRequest_l;
	SIOGeneric sResponse_l;
	INT8U len_l;
	INT8U data_out[sizeof(SAioRequest) - IOPROTOCOL_HEADER_LENGTH - 1];
	INT8U data_in[sizeof(SAioResponse) - IOPROTOCOL_HEADER_LENGTH - 1];
	INT8U i8uAddress;
	int ret;
#ifdef DEBUG_DEVICE_AIO
	static INT8U last_out[40][sizeof(data_out)];
	static INT8U last_in[40][2];
#endif

	if (RevPiDevice_getDev(i8uDevice_p)->sId.i16uFBS_OutputLength != sizeof(data_out)) {
		return 4;
	}

	len_l = sizeof(data_out);
	i8uAddress = RevPiDevice_getDev(i8uDevice_p)->i8uAddress;

	if (piDev_g.stopIO == false) {
		my_rt_mutex_lock(&piDev_g.lockPI);
		memcpy(data_out, piDev_g.ai8uPI + RevPiDevice_getDev(i8uDevice_p)->i16uOutputOffset, len_l);
		rt_mutex_unlock(&piDev_g.lockPI);
	} else {
		memset(data_out, 0, len_l);
	}

	sRequest_l.uHeader.sHeaderTyp1.bitAddress = i8uAddress;
	sRequest_l.uHeader.sHeaderTyp1.bitIoHeaderType = 0;
	sRequest_l.uHeader.sHeaderTyp1.bitReqResp = 0;
	sRequest_l.uHeader.sHeaderTyp1.bitLength = len_l;
	sRequest_l.uHeader.sHeaderTyp1.bitCommand = IOP_TYP1_CMD_DATA;

	memcpy(sRequest_l.ai8uData, data_out, len_l);

	sRequest_l.ai8uData[len_l] = piIoComm_Crc8((INT8U *) & sRequest_l, IOPROTOCOL_HEADER_LENGTH + len_l);

#ifdef DEBUG_DEVICE_AIO
	if (last_out[i8uAddress][0] != sRequest_l.ai8uData[0] || last_out[i8uAddress][1] != sRequest_l.ai8uData[1]) {
		pr_info_aio("dev %2d: send cyclic Data addr %d output 0x%02x 0x%02x\n",
			    i8uAddress, RevPiDevice_getDev(i8uDevice_p].i16uOutputOffset,
			    sRequest_l.ai8uData[0], sRequest_l.ai8uData[1]);
	}
	memcpy(last_out[i8uAddress], data_out, sizeof(data_out));
#endif

	ret = piIoComm_send((INT8U *) & sRequest_l, sizeof(SAioRequest));
	if (ret == 0) {
		len_l = sizeof(SAioResponse) - IOPROTOCOL_HEADER_LENGTH - 1;	// data length only

		ret = piIoComm_recv((INT8U *) & sResponse_l, IOPROTOCOL_HEADER_LENGTH + len_l + 1);
		if (ret > 0) {
			if (piIoComm_response_valid(&sResponse_l, i8uAddress, len_l)) {
				memcpy(data_in, sResponse_l.ai8uData, len_l);

				if (piDev_g.stopIO == false) {
					my_rt_mutex_lock(&piDev_g.lockPI);
					memcpy(piDev_g.ai8uPI + RevPiDevice_getDev(i8uDevice_p)->i16uInputOffset, data_in,
					       sizeof(data_in));
					rt_mutex_unlock(&piDev_g.lockPI);
				}

#ifdef DEBUG_DEVICE_AIO
				if (last_in[i8uAddress][0] != sResponse_l.ai8uData[0]
				    || last_in[i8uAddress][1] != sResponse_l.ai8uData[1]) {
					last_in[i8uAddress][0] = sResponse_l.ai8uData[0];
					last_in[i8uAddress][1] = sResponse_l.ai8uData[1];
					pr_info_aio("dev %2d: recv cyclic Data addr %d input 0x%02x 0x%02x\n\n",
						    i8uAddress, RevPiDevice_getDev(i8uDevice_p].i16uInputOffset,
						    sResponse_l.ai8uData[0], sResponse_l.ai8uData[1]);
				}
#endif
			} else {
				i32uRv_l = 1;
				pr_info_aio("dev %2d: recv ioprotocol crc/len error, %x!=%x, len:%d!=%d\n",
						i8uAddress,
						sResponse_l.ai8uData[len_l],
						piIoComm_Crc8((INT8U *) &sResponse_l, IOPROTOCOL_HEADER_LENGTH + len_l),
						sResponse_l.uHeader.sHeaderTyp1.bitLength,
						len_l);
			}
		} else {
			i32uRv_l = 2;
			pr_info_aio("dev %2d: recv ioprotocol timeout error exp %d\n",
				    i8uAddress, IOPROTOCOL_HEADER_LENGTH + len_l + 1);

		}
	} else {
		i32uRv_l = 3;
		pr_info_aio("dev %2d: send ioprotocol send error %d\n", i8uAddress, ret);
	}
	return i32uRv_l;
}

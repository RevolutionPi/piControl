// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: 2016-2024 KUNBUS GmbH

#include <linux/pibridge_comm.h>
#include <linux/of.h>

#include "RevPiDevice.h"
#include "piAIOComm.h"
#include "piDIOComm.h"
#include "revpi_core.h"
#include "revpi_mio.h"
#include "revpi_ro.h"
#include "picontrol_trace.h"

static SDeviceConfig RevPiDevices_s;

const MODGATECOM_IDResp RevPiCore_ID_g = {
	.i32uSerialnumber = REV_PI_DEV_DEFAULT_SERIAL,
	.i16uModulType = KUNBUS_FW_DESCR_TYP_PI_CORE,
	.i16uHW_Revision = 1,
	.i16uSW_Major = 1,
	.i16uSW_Minor = 2,	//TODO
	.i32uSVN_Revision = 0,
	.i16uFBS_InputLength = 6,
	.i16uFBS_OutputLength = 5,
	.i16uFeatureDescriptor = MODGATE_feature_IODataExchange
};

const MODGATECOM_IDResp RevPiCompact_ID_g = {
	.i32uSerialnumber = REV_PI_DEV_DEFAULT_SERIAL,
	.i16uModulType = KUNBUS_FW_DESCR_TYP_PI_COMPACT,
	.i16uHW_Revision = 1,
	.i16uSW_Major = 1,
	.i16uSW_Minor = 0,
	.i32uSVN_Revision = 0,
	.i16uFBS_InputLength = 23,
	.i16uFBS_OutputLength = 6,
	.i16uFeatureDescriptor = MODGATE_feature_IODataExchange
};

const MODGATECOM_IDResp RevPiConnect_ID_g = {
	.i32uSerialnumber = REV_PI_DEV_DEFAULT_SERIAL,
	.i16uModulType = KUNBUS_FW_DESCR_TYP_PI_CONNECT,
	.i16uHW_Revision = 1,
	.i16uSW_Major = 1,
	.i16uSW_Minor = 0,
	.i32uSVN_Revision = 0,
	.i16uFBS_InputLength = 6,
	.i16uFBS_OutputLength = 5,
	.i16uFeatureDescriptor = MODGATE_feature_IODataExchange
};

const MODGATECOM_IDResp RevPiConnect4_ID_g = {
	.i32uSerialnumber = REV_PI_DEV_DEFAULT_SERIAL,
	.i16uModulType = KUNBUS_FW_DESCR_TYP_PI_CONNECT_4,
	.i16uHW_Revision = 1,
	.i16uSW_Major = 1,
	.i16uSW_Minor = 0,
	.i32uSVN_Revision = 0,
	.i16uFBS_InputLength = 6,
	.i16uFBS_OutputLength = 7,
	.i16uFeatureDescriptor = 0
};

const MODGATECOM_IDResp RevPiConnect5_ID_g = {
	.i32uSerialnumber = 1,
	.i16uModulType = KUNBUS_FW_DESCR_TYP_PI_CONNECT_5,
	.i16uHW_Revision = 1,
	.i16uSW_Major = 1,
	.i16uSW_Minor = 0,
	.i32uSVN_Revision = 0,
	.i16uFBS_InputLength = 6,
	.i16uFBS_OutputLength = 7,
	.i16uFeatureDescriptor = 0
};

const MODGATECOM_IDResp RevPiFlat_ID_g = {
	.i32uSerialnumber = REV_PI_DEV_DEFAULT_SERIAL,
	.i16uModulType = KUNBUS_FW_DESCR_TYP_PI_FLAT,
	.i16uHW_Revision = 1,
	.i16uSW_Major = 1,
	.i16uSW_Minor = 0,
	.i32uSVN_Revision = 0,
	.i16uFBS_InputLength = 6,
	.i16uFBS_OutputLength = 6,
	.i16uFeatureDescriptor = MODGATE_feature_IODataExchange
};

const MODGATECOM_IDResp RevPiGeneric_ID_g = {
	.i32uSerialnumber = REV_PI_DEV_DEFAULT_SERIAL,
	.i16uModulType = KUNBUS_FW_DESCR_TYP_PI_REVPI_GENERIC_PB,
	.i16uHW_Revision = 1,
	.i16uSW_Major = 1,
	.i16uSW_Minor = 0,
	.i32uSVN_Revision = 0,
	.i16uFBS_InputLength = 0,
	.i16uFBS_OutputLength = 0,
	.i16uFeatureDescriptor = 0
};

void RevPiDevice_handle_internal_telegrams(void)
{
	int ret = 0;

	/* If requested by user, send internal io/gate telegram(s) */
	rt_mutex_lock(&piCore_g.lockUserTel);
	if (piCore_g.pendingUserTel == true) {
		SIOGeneric *req = &piCore_g.requestUserTel;
		SIOGeneric *resp = &piCore_g.responseUserTel;
		UIoProtocolHeader *hdr = &req->uHeader;

		/* avoid leaking response of previous telegram to user space */
		memset(resp, 0, sizeof(*resp));

		ret = pibridge_req_io(piCore_g.pibridge,
				      hdr->sHeaderTyp1.bitAddress,
				      hdr->sHeaderTyp1.bitCommand,
				      req->ai8uData,
				      hdr->sHeaderTyp1.bitLength,
				      resp->ai8uData,
				      sizeof(resp->ai8uData) - 1);
		if (ret < 0) {
			piCore_g.statusUserTel = ret;
		} else {
			piCore_g.statusUserTel = 0;
			resp->uHeader.sHeaderTyp1.bitLength = ret;
		}
		piCore_g.pendingUserTel = false;
		up(&piCore_g.semUserTel);
	}
	rt_mutex_unlock(&piCore_g.lockUserTel);

	rt_mutex_lock(&piCore_g.lockGateTel);
	if (piCore_g.pendingGateTel == true) {
		piCore_g.statusGateTel =
			pibridge_req_gate_datagram(piCore_g.pibridge,
						   &piCore_g.gate_req_dgram,
						   &piCore_g.gate_resp_dgram);
		piCore_g.pendingGateTel = false;
		up(&piCore_g.semGateTel);
	}
	rt_mutex_unlock(&piCore_g.lockGateTel);
}


int RevPiDevice_hat_serial(void)
{
	struct device_node *np;
	const char *property;
	int len, serial;

	np = of_find_node_by_path("/hat");
	if (!np) {
		pr_warn("No HAT eeprom detected: Fallback to default serial\n");
		return REV_PI_DEV_DEFAULT_SERIAL;
	}

	property = of_get_property(np, "custom_1", &len);
	if (!property) {
		of_node_put(np);
		pr_warn("Invalid HAT eeprom: Fallback to default serial\n");
		return REV_PI_DEV_DEFAULT_SERIAL;
	}

	if (kstrtoint(property, 10, &serial)) {
		of_node_put(np);
		pr_warn("Unable to parse serial from HAT eeprom: Fallback to default serial\n");
		return REV_PI_DEV_DEFAULT_SERIAL;
	}

	of_node_put(np);
	return serial;
}

void RevPiDevice_init(void)
{
	pr_debug("RevPiDevice_init()\n");

	piCore_g.cycle_num = 0;
	piCore_g.comm_errors = 0;
	piCore_g.i8uLeftMGateIdx = REV_PI_DEV_UNDEF;
	piCore_g.i8uRightMGateIdx = REV_PI_DEV_UNDEF;
	RevPiDevices_s.i8uAddressRight = REV_PI_DEV_FIRST_RIGHT;	// first address of a right side module
	RevPiDevices_s.i8uAddressLeft = REV_PI_DEV_FIRST_RIGHT - 1;	// first address of a left side module
	RevPiDevice_resetDevCnt();	// counter for detected devices
	RevPiDevices_s.i16uErrorCnt = 0;

	// RevPi as first entry to device list
	RevPiDevice_getDev(RevPiDevice_getDevCnt())->i8uAddress = 0;
	RevPiDevice_getDev(RevPiDevice_getDevCnt())->i8uActive = 1;
	RevPiDevice_getDev(RevPiDevice_getDevCnt())->i8uScan = 1;

	switch (piDev_g.machine_type) {
		case REVPI_CORE:
			RevPiDevice_getDev(RevPiDevice_getDevCnt())->sId = RevPiCore_ID_g;
			RevPiDevice_getDev(RevPiDevice_getDevCnt())->i16uInputOffset = 0;
			RevPiDevice_getDev(RevPiDevice_getDevCnt())->i16uOutputOffset = RevPiCore_ID_g.i16uFBS_InputLength;
			break;
		case REVPI_CORE_SE:
			RevPiDevice_getDev(RevPiDevice_getDevCnt())->sId = RevPiCore_ID_g;
			RevPiDevice_getDev(RevPiDevice_getDevCnt())->i16uInputOffset = 0;
			RevPiDevice_getDev(RevPiDevice_getDevCnt())->i16uOutputOffset = RevPiCore_ID_g.i16uFBS_InputLength;
			break;
		case REVPI_COMPACT:
			RevPiDevice_getDev(RevPiDevice_getDevCnt())->sId = RevPiCompact_ID_g;
			RevPiDevice_getDev(RevPiDevice_getDevCnt())->i16uInputOffset = 0;
			RevPiDevice_getDev(RevPiDevice_getDevCnt())->i16uOutputOffset = RevPiCompact_ID_g.i16uFBS_InputLength;
			break;
		case REVPI_CONNECT:
			RevPiDevice_getDev(RevPiDevice_getDevCnt())->sId = RevPiConnect_ID_g;
			RevPiDevice_getDev(RevPiDevice_getDevCnt())->i16uInputOffset = 0;
			RevPiDevice_getDev(RevPiDevice_getDevCnt())->i16uOutputOffset = RevPiConnect_ID_g.i16uFBS_InputLength;
			break;
		case REVPI_CONNECT_SE:
			RevPiDevice_getDev(RevPiDevice_getDevCnt())->sId = RevPiConnect_ID_g;
			RevPiDevice_getDev(RevPiDevice_getDevCnt())->i16uInputOffset = 0;
			RevPiDevice_getDev(RevPiDevice_getDevCnt())->i16uOutputOffset = RevPiConnect_ID_g.i16uFBS_InputLength;
			break;
		case REVPI_CONNECT_4:
			RevPiDevice_getDev(RevPiDevice_getDevCnt())->sId = RevPiConnect4_ID_g;
			RevPiDevice_getDev(RevPiDevice_getDevCnt())->i16uInputOffset = 0;
			RevPiDevice_getDev(RevPiDevice_getDevCnt())->i16uOutputOffset = RevPiConnect4_ID_g.i16uFBS_InputLength;
			break;
		case REVPI_CONNECT_5:
			RevPiDevice_getDev(RevPiDevice_getDevCnt())->sId = RevPiConnect5_ID_g;
			RevPiDevice_getDev(RevPiDevice_getDevCnt())->i16uInputOffset = 0;
			RevPiDevice_getDev(RevPiDevice_getDevCnt())->i16uOutputOffset = RevPiConnect5_ID_g.i16uFBS_InputLength;
			break;
		case REVPI_FLAT:
			RevPiDevice_getDev(RevPiDevice_getDevCnt())->sId = RevPiFlat_ID_g;
			RevPiDevice_getDev(RevPiDevice_getDevCnt())->i16uInputOffset = 0;
			RevPiDevice_getDev(RevPiDevice_getDevCnt())->i16uOutputOffset = RevPiFlat_ID_g.i16uFBS_InputLength;
			break;
		case REVPI_GENERIC_PB:
			RevPiDevice_getDev(RevPiDevice_getDevCnt())->sId = RevPiGeneric_ID_g;
			RevPiDevice_getDev(RevPiDevice_getDevCnt())->i16uInputOffset = 0;
			RevPiDevice_getDev(RevPiDevice_getDevCnt())->i16uOutputOffset = RevPiGeneric_ID_g.i16uFBS_InputLength;
			break;
	}

	// Set device serial number from HAT eeprom (with fallback to default)
	RevPiDevice_getDev(RevPiDevice_getDevCnt())->sId.i32uSerialnumber =
		RevPiDevice_hat_serial();

	RevPiDevice_incDevCnt();
}

void revpi_dev_update_state(INT8U i8uDevice, INT32U r, int *retval)
{
	if (r) {
		if (RevPiDevice_getDev(i8uDevice)->i16uErrorCnt < 255) {
			RevPiDevice_getDev(i8uDevice)->i16uErrorCnt++;
		}
		else
			RevPiDevice_getDev(i8uDevice)->i8uModuleState = IOSTATE_OFFLINE;
		*retval -= 1;	// tell calling function that an error occured
		if (RevPiDevice_getDev(i8uDevice)->i16uErrorCnt > 1) {
			// the first error is ignored
			RevPiDevices_s.i16uErrorCnt += RevPiDevice_getDev(i8uDevice)->i16uErrorCnt;
		}
	} else {
		RevPiDevice_getDev(i8uDevice)->i16uErrorCnt = 0;
		RevPiDevice_getDev(i8uDevice)->i8uModuleState = IOSTATE_CYCLIC_IO;
	}
}

//*************************************************************************************************
//| Function: RevPiDevice_run
//|
//! \brief cyclically called run function
//!
//! \detailed performs the cyclic communication with all modules
//! connected to the RS485 Bus
//!
//!
//! \ingroup
//-------------------------------------------------------------------------------------------------
int RevPiDevice_run(void)
{
	INT8U i8uDevice = 0;
	INT32U r;
	int retval = 0;
	SDevice *dev;

	RevPiDevices_s.i16uErrorCnt = 0;

	for (i8uDevice = 0; i8uDevice < RevPiDevice_getDevCnt(); i8uDevice++) {
		dev = RevPiDevice_getDev(i8uDevice);

		if (dev->i8uActive) {
			trace_picontrol_cyclic_device_data_start(dev->i8uAddress);

			switch (dev->sId.i16uModulType) {
			case KUNBUS_FW_DESCR_TYP_PI_DIO_14:
			case KUNBUS_FW_DESCR_TYP_PI_DI_16:
			case KUNBUS_FW_DESCR_TYP_PI_DO_16:
				r = piDIOComm_sendCyclicTelegram(i8uDevice);
				revpi_dev_update_state(i8uDevice, r, &retval);
				break;

			case KUNBUS_FW_DESCR_TYP_PI_AIO:
				r = piAIOComm_sendCyclicTelegram(i8uDevice);
				revpi_dev_update_state(i8uDevice, r, &retval);
				break;
			case KUNBUS_FW_DESCR_TYP_PI_MIO:
				r = revpi_mio_cycle(i8uDevice);
				revpi_dev_update_state(i8uDevice, r, &retval);
				break;
			case KUNBUS_FW_DESCR_TYP_PI_RO:
				r = revpi_ro_cycle(i8uDevice);
				revpi_dev_update_state(i8uDevice, r, &retval);
				break;

			case KUNBUS_FW_DESCR_TYP_MG_CAN_OPEN:
			case KUNBUS_FW_DESCR_TYP_MG_DEV_NET:
			case KUNBUS_FW_DESCR_TYP_MG_ETHERCAT:
			case KUNBUS_FW_DESCR_TYP_MG_ETHERNET_IP:
			case KUNBUS_FW_DESCR_TYP_MG_POWERLINK:
			case KUNBUS_FW_DESCR_TYP_MG_PROFIBUS:
			case KUNBUS_FW_DESCR_TYP_MG_PROFINET_IRT:
			case KUNBUS_FW_DESCR_TYP_MG_CAN_OPEN_MASTER:
			case KUNBUS_FW_DESCR_TYP_MG_SERCOS3:
			case KUNBUS_FW_DESCR_TYP_MG_SERIAL:
			case KUNBUS_FW_DESCR_TYP_MG_MODBUS_RTU:
			case KUNBUS_FW_DESCR_TYP_MG_MODBUS_TCP:
			case KUNBUS_FW_DESCR_TYP_MG_DMX:
				if (piCore_g.i8uRightMGateIdx == REV_PI_DEV_UNDEF
				    && dev->i8uAddress >= REV_PI_DEV_FIRST_RIGHT) {
					piCore_g.i8uRightMGateIdx = i8uDevice;
				} else if (piCore_g.i8uLeftMGateIdx == REV_PI_DEV_UNDEF
					   && dev->i8uAddress < REV_PI_DEV_FIRST_RIGHT) {
					piCore_g.i8uLeftMGateIdx = i8uDevice;
				}
				break;

			default:
				//TODO
				// user devices are ignored here
				break;
			}
			trace_picontrol_cyclic_device_data_stop(dev->i8uAddress);
		}
	}

	/* If requested by user, send internal io/gate telegram(s) */
	RevPiDevice_handle_internal_telegrams();

	return retval;
}

TBOOL RevPiDevice_writeNextConfiguration(INT8U i8uAddress_p, MODGATECOM_IDResp * pModgateId_p)
{
	INT32U ret_l;
	INT16U i16uLen_l = sizeof(MODGATECOM_IDResp);
	//
	ret_l =
	    piIoComm_sendRS485Tel(eCmdGetDeviceInfo, 77, NULL, 0, (INT8U *) pModgateId_p, &i16uLen_l);
	msleep(3);		// wait a while
	if (ret_l) {
#ifdef DEBUG_DEVICE
		pr_err("piIoComm_sendRS485Tel(GetDeviceInfo) failed %d\n", ret_l);
#endif
		return bFALSE;
	} else {
		pr_debug("GetDeviceInfo: Id %d\n", pModgateId_p->i16uModulType);
	}

	ret_l = piIoComm_sendRS485Tel(eCmdPiIoSetAddress, i8uAddress_p, NULL, 0, NULL, 0);
	msleep(3);		// wait a while
	if (ret_l) {
		ret_l = piIoComm_sendRS485Tel(eCmdPiIoSetAddress, i8uAddress_p, NULL, 0, NULL, 0);
		msleep(3);		// wait a while
		if (ret_l) {
			ret_l = piIoComm_sendRS485Tel(eCmdPiIoSetAddress, i8uAddress_p, NULL, 0, NULL, 0);
			msleep(3);		// wait a while
			if (ret_l) {
#ifdef DEBUG_DEVICE
				pr_err("piIoComm_sendRS485Tel(PiIoSetAddress) failed %d\n", ret_l);
#endif
			}
		}
		return bFALSE;
	}
	return bTRUE;
}

TBOOL RevPiDevice_writeNextConfigurationRight(void)
{
	if (RevPiDevice_writeNextConfiguration(RevPiDevices_s.i8uAddressRight, &RevPiDevice_getDev(RevPiDevice_getDevCnt())->sId)) {
		RevPiDevice_getDev(RevPiDevice_getDevCnt())->i8uAddress = RevPiDevices_s.i8uAddressRight;
		if (RevPiDevice_getDevCnt() == 0) {
			RevPiDevice_getDev(RevPiDevice_getDevCnt())->i16uInputOffset = 0;
			RevPiDevice_getDev(RevPiDevice_getDevCnt())->i16uOutputOffset =
			    RevPiDevice_getDev(RevPiDevice_getDevCnt())->sId.i16uFBS_InputLength;
		} else {
			RevPiDevice_getDev(RevPiDevice_getDevCnt())->i16uInputOffset =
			    RevPiDevice_getDev(RevPiDevice_getDevCnt() - 1)->i16uOutputOffset +
			    RevPiDevice_getDev(RevPiDevice_getDevCnt() - 1)->sId.i16uFBS_OutputLength;
			RevPiDevice_getDev(RevPiDevice_getDevCnt())->i16uOutputOffset =
			    RevPiDevice_getDev(RevPiDevice_getDevCnt())->i16uInputOffset +
			    RevPiDevice_getDev(RevPiDevice_getDevCnt())->sId.i16uFBS_InputLength;
		}
#ifdef DEBUG_DEVICE
		pr_info("found %d. device on right side. Moduletype %d. Designated address %d\n",
			RevPiDevice_getDevCnt() + 1, RevPiDevice_getDev(RevPiDevice_getDevCnt())->sId.i16uModulType,
			RevPiDevices_s.i8uAddressRight);
		pr_debug("input offset  %5d  len %3d\n", RevPiDevice_getDev(RevPiDevice_getDevCnt())->i16uInputOffset,
			RevPiDevice_getDev(RevPiDevice_getDevCnt())->sId.i16uFBS_InputLength);
		pr_debug("output offset %5d  len %3d\n", RevPiDevice_getDev(RevPiDevice_getDevCnt())->i16uOutputOffset,
			RevPiDevice_getDev(RevPiDevice_getDevCnt())->sId.i16uFBS_OutputLength);
#endif
		RevPiDevice_getDev(RevPiDevice_getDevCnt())->i8uActive = 1;
		RevPiDevice_getDev(RevPiDevice_getDevCnt())->i8uScan = 1;
		RevPiDevice_incDevCnt();
		RevPiDevices_s.i8uAddressRight++;
		return bTRUE;
	} else {
		//TODO restart with reset
	}
	return bFALSE;
}

TBOOL RevPiDevice_writeNextConfigurationLeft(void)
{
	if (RevPiDevice_writeNextConfiguration(RevPiDevices_s.i8uAddressLeft, &RevPiDevice_getDev(RevPiDevice_getDevCnt())->sId)) {
		RevPiDevice_getDev(RevPiDevice_getDevCnt())->i8uAddress = RevPiDevices_s.i8uAddressLeft;
		if (RevPiDevice_getDevCnt() == 0) {
			RevPiDevice_getDev(RevPiDevice_getDevCnt())->i16uInputOffset = 0;
			RevPiDevice_getDev(RevPiDevice_getDevCnt())->i16uOutputOffset =
			    RevPiDevice_getDev(RevPiDevice_getDevCnt())->sId.i16uFBS_InputLength;
		} else {
			RevPiDevice_getDev(RevPiDevice_getDevCnt())->i16uInputOffset =
			    RevPiDevice_getDev(RevPiDevice_getDevCnt() - 1)->i16uOutputOffset +
			    RevPiDevice_getDev(RevPiDevice_getDevCnt() - 1)->sId.i16uFBS_OutputLength;
			RevPiDevice_getDev(RevPiDevice_getDevCnt())->i16uOutputOffset =
			    RevPiDevice_getDev(RevPiDevice_getDevCnt())->i16uInputOffset +
			    RevPiDevice_getDev(RevPiDevice_getDevCnt())->sId.i16uFBS_InputLength;
		}
#ifdef DEBUG_DEVICE
		pr_info("found %d. device on left side. Moduletype %d. Designated address %d\n",
			RevPiDevice_getDevCnt() + 1,
			RevPiDevice_getDev(RevPiDevice_getDevCnt())->sId.i16uModulType, RevPiDevices_s.i8uAddressLeft);
		pr_debug("input offset  %5d  len %3d\n",
			RevPiDevice_getDev(RevPiDevice_getDevCnt())->i16uInputOffset,
			RevPiDevice_getDev(RevPiDevice_getDevCnt())->sId.i16uFBS_InputLength);
		pr_debug("output offset %5d  len %3d\n",
			RevPiDevice_getDev(RevPiDevice_getDevCnt())->i16uOutputOffset,
			RevPiDevice_getDev(RevPiDevice_getDevCnt())->sId.i16uFBS_OutputLength);
#endif
		RevPiDevice_getDev(RevPiDevice_getDevCnt())->i8uActive = 1;
		RevPiDevice_getDev(RevPiDevice_getDevCnt())->i8uScan = 1;
		RevPiDevice_incDevCnt();
		RevPiDevices_s.i8uAddressLeft--;
		return bTRUE;
	} else {
		//TODO restart with reset
	}
	return bFALSE;
}

void RevPiDevice_startDataexchange(void)
{
	INT32U ret_l = piIoComm_sendRS485Tel(eCmdPiIoStartDataExchange, MODGATE_RS485_BROADCAST_ADDR, NULL, 0, NULL, 0);
	msleep(90);		// wait a while
	if (ret_l) {
#ifdef DEBUG_DEVICE
		pr_err("piIoComm_sendRS485Tel(PiIoStartDataExchange) failed %d\n", ret_l);
#endif
	}
}

void RevPiDevice_stopDataexchange(void)
{
	INT32U ret_l = piIoComm_sendRS485Tel(eCmdPiIoStartDataExchange, MODGATE_RS485_BROADCAST_ADDR, NULL, 0, NULL, 0);
	msleep(90);		// wait a while
	if (ret_l) {
#ifdef DEBUG_DEVICE
		pr_err("piIoComm_sendRS485Tel(PiIoStartDataExchange) failed %d\n", ret_l);
#endif
	}
}

u8 RevPiDevice_find_by_side_and_type(bool right, u16 module_type)
{
	int i;

	for (i = 0; i < RevPiDevice_getDevCnt(); i++) {
		if (right &&
		    RevPiDevice_getDev(i)->i8uAddress < REV_PI_DEV_FIRST_RIGHT)
			continue;
		if (!right &&
		    RevPiDevice_getDev(i)->i8uAddress >= REV_PI_DEV_FIRST_RIGHT)
			continue;
		if (RevPiDevice_getDev(i)->sId.i16uModulType == module_type)
			return i;
	}
	return REV_PI_DEV_UNDEF;
}

INT8U RevPiDevice_setStatus(INT8U clr, INT8U set)
{
	RevPiDevices_s.i8uStatus &= ~clr;
	RevPiDevices_s.i8uStatus |= set;
	return RevPiDevices_s.i8uStatus;
}

INT8U RevPiDevice_getStatus(void)
{
	return RevPiDevices_s.i8uStatus;
}

SDevice *RevPiDevice_getDev(INT8U idx)
{
	// idx==i8uDeviceCount is allowed. This enables to add data to the next entry before RevPiDevice_incDevCnt is called.
	if (idx <= RevPiDevices_s.i8uDeviceCount)
		return &RevPiDevices_s.dev[idx];
	else
		return &RevPiDevices_s.dev[0];
}

void RevPiDevice_resetDevCnt(void)
{
	RevPiDevices_s.i8uDeviceCount = 0;
}

void RevPiDevice_incDevCnt(void)
{
	if (RevPiDevices_s.i8uDeviceCount < REV_PI_DEV_CNT_MAX-1) {
		RevPiDevices_s.i8uDeviceCount++;
	}
}

INT8U RevPiDevice_getDevCnt(void)
{
	return RevPiDevices_s.i8uDeviceCount;
}

INT8U RevPiDevice_getAddrLeft(void)
{
	return RevPiDevices_s.i8uAddressLeft;
}

INT8U RevPiDevice_getAddrRight(void)
{
	return RevPiDevices_s.i8uAddressRight;
}


INT16U RevPiDevice_getErrCnt(void)
{
	return RevPiDevices_s.i16uErrorCnt;
}

void RevPiDevice_setCoreOffset(unsigned int offset)
{
	RevPiDevices_s.offset = offset;
}

unsigned int RevPiDevice_getCoreOffset(void)
{
	return RevPiDevices_s.offset;
}

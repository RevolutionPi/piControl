// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: 2016-2026 KUNBUS GmbH

#include <linux/pibridge_comm.h>
#include <linux/of.h>
#include <linux/spinlock.h>

#include "RevPiDevice.h"
#include "piAIOComm.h"
#include "piControl.h"
#include "piDIOComm.h"
#include "revpi_core.h"
#include "revpi_mio.h"
#include "revpi_ro.h"
#include "picontrol_trace.h"

static SDeviceConfig RevPiDevices_s;
static DEFINE_SPINLOCK(status_lock);

static const MODGATECOM_IDResp RevPiCore_ID_g = {
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

static const MODGATECOM_IDResp RevPiCompact_ID_g = {
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

static const MODGATECOM_IDResp RevPiConnect_ID_g = {
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

static const MODGATECOM_IDResp RevPiConnect4_ID_g = {
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

static const MODGATECOM_IDResp RevPiConnect5_ID_g = {
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

static const MODGATECOM_IDResp RevPiFlat_ID_g = {
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

static const MODGATECOM_IDResp RevPiGeneric_ID_g = {
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
	scoped_guard(rt_mutex, &piCore_g.lockUserTel) {
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
	}

	scoped_guard(rt_mutex, &piCore_g.lockGateTel) {
		if (piCore_g.pendingGateTel == true) {
			piCore_g.statusGateTel =
				pibridge_req_gate_datagram(piCore_g.pibridge,
							   &piCore_g.gate_req_dgram,
							   &piCore_g.gate_resp_dgram);
			piCore_g.pendingGateTel = false;
			up(&piCore_g.semGateTel);
		}
	}
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
	int i;

	pr_debug("RevPiDevice_init()\n");

	piCore_g.cycle_num = 0;
	piCore_g.i8uLeftMGateIdx = REV_PI_DEV_UNDEF;
	piCore_g.i8uRightMGateIdx = REV_PI_DEV_UNDEF;
	RevPiDevices_s.i8uAddressRight = REV_PI_DEV_FIRST_RIGHT;	// first address of a right side module
	RevPiDevices_s.gatewayRight = false;
	RevPiDevices_s.i8uAddressLeft = REV_PI_DEV_FIRST_LEFT;		// first address of a left side module
	RevPiDevices_s.gatewayLeft = false;
	RevPiDevice_resetDevCnt();	// counter for detected devices
	RevPiDevices_s.i16uErrorCnt = 0;

	// start each (re)configuration with a clean per-module error state
	for (i = 0; i < ARRAY_SIZE(RevPiDevices_s.dev); i++) {
		RevPiDevices_s.dev[i].i16uErrorCnt = 0;
		RevPiDevices_s.dev[i].i8uModuleState = IOSTATE_OFFLINE;
	}

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

void revpi_dev_update_state(u8 i8uDevice, int r, int *retval)
{
	SDevice *dev = RevPiDevice_getDev(i8uDevice);

	if (r < 0) {
		if (dev->i16uErrorCnt < U16_MAX)
			dev->i16uErrorCnt++;
		// the module is reported offline from PiBridgeMaster_checkErrorLimits()
		// once the configured error limit is reached
		*retval -= 1;	// tell calling function that an error occured
		if (dev->i16uErrorCnt > 1) {
			// the first error is ignored
			if ((RevPiDevices_s.i16uErrorCnt + dev->i16uErrorCnt) > U16_MAX)
				RevPiDevices_s.i16uErrorCnt = U16_MAX;
			else
				RevPiDevices_s.i16uErrorCnt += dev->i16uErrorCnt;
		}
	} else {
		u16 offline_limit = piCore_g.image.usr.i16uRS485ErrorLimit2;

		/* report recovery only for a module that had reached the offline limit */
		if (offline_limit && dev->i16uErrorCnt >= offline_limit)
			pr_info("module at address %u back online\n", dev->i8uAddress);
		dev->i16uErrorCnt = 0;
		dev->i8uModuleState = IOSTATE_CYCLIC_IO;
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
	u8 i8uDevice = 0;
	int r;
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

			default:
				// ignore base device, virtual modules and gateways
				break;
			}
			trace_picontrol_cyclic_device_data_stop(dev->i8uAddress);
		}
	}

	/* If requested by user, send internal io/gate telegram(s) */
	RevPiDevice_handle_internal_telegrams();

	return retval;
}

bool RevPiDevice_writeNextConfiguration(u8 i8uAddress_p, MODGATECOM_IDResp * pModgateId_p)
{
	int attempts = 3;
	u32 ret_l;
	u16 i16uLen_l = sizeof(MODGATECOM_IDResp);

	/*
	 * A gateway which booted too late for the master present pulse
	 * answers scan requests as well, its late response corrupts the
	 * following request. Retry silently like PiIoSetAddress does.
	 */
	do {
		ret_l = piIoComm_sendRS485Tel(eCmdGetDeviceInfo, 77, NULL, 0,
					      (u8 *) pModgateId_p, &i16uLen_l);
		msleep(3);	// wait a while
	} while (ret_l && --attempts);

	if (ret_l) {
		pr_err("GetDeviceInfo for designated address %u failed: %d\n",
			i8uAddress_p, ret_l);
		return false;
	} else {
		pr_debug("GetDeviceInfo: Id %d\n", pModgateId_p->i16uModulType);
	}

	ret_l = piIoComm_sendRS485Tel(eCmdPiIoSetAddress, i8uAddress_p, NULL, 0, NULL, NULL);
	msleep(3);		// wait a while
	if (ret_l) {
		ret_l = piIoComm_sendRS485Tel(eCmdPiIoSetAddress, i8uAddress_p, NULL, 0, NULL,
					      NULL);
		msleep(3);		// wait a while
		if (ret_l) {
			ret_l = piIoComm_sendRS485Tel(eCmdPiIoSetAddress, i8uAddress_p, NULL, 0,
						      NULL, NULL);
			msleep(3);		// wait a while
			if (ret_l)
				pr_err("PiIoSetAddress for designated address %u failed: %d\n",
					i8uAddress_p, ret_l);
		}
		return false;
	}
	return true;
}

static bool write_next_config_side(bool right)
{
	SDevice *dev = RevPiDevice_getDev(RevPiDevice_getDevCnt());
	u8 addr = right ? RevPiDevices_s.i8uAddressRight :
			  RevPiDevices_s.i8uAddressLeft;

	if (!RevPiDevice_writeNextConfiguration(addr, &dev->sId))
		return false;

	dev->i8uAddress = addr;
	if (RevPiDevice_getDevCnt() == 0) {
		dev->i16uInputOffset = 0;
		dev->i16uOutputOffset = dev->sId.i16uFBS_InputLength;
	} else {
		SDevice *prev = RevPiDevice_getDev(RevPiDevice_getDevCnt() - 1);

		dev->i16uInputOffset = prev->i16uOutputOffset +
				       prev->sId.i16uFBS_OutputLength;
		dev->i16uOutputOffset = dev->i16uInputOffset +
					dev->sId.i16uFBS_InputLength;
	}

	pr_info("found %d. device on %s side. Moduletype %d. Designated address %d\n",
		RevPiDevice_getDevCnt() + 1, right ? "right" : "left",
		dev->sId.i16uModulType, addr);
	pr_debug("input offset  %5d  len %3d\n", dev->i16uInputOffset,
		 dev->sId.i16uFBS_InputLength);
	pr_debug("output offset %5d  len %3d\n", dev->i16uOutputOffset,
		 dev->sId.i16uFBS_OutputLength);

	dev->i8uActive = 1;
	dev->i8uScan = 1;

	if (dev->sId.i16uFeatureDescriptor & MODGATE_feature_IODataExchange) {
		if (right)
			RevPiDevices_s.gatewayRight = true;
		else
			RevPiDevices_s.gatewayLeft = true;
	}

	RevPiDevice_incDevCnt();
	if (right)
		RevPiDevices_s.i8uAddressRight++;
	else
		RevPiDevices_s.i8uAddressLeft--;

	return true;
}

bool RevPiDevice_writeNextConfigurationRight(void)
{
	return write_next_config_side(true);
}

bool RevPiDevice_writeNextConfigurationLeft(void)
{
	return write_next_config_side(false);
}

void RevPiDevice_startDataexchange(void)
{
	u8 checksum = pibridge_get_iop_crc16(piCore_g.pibridge) ? 1 : 0;
	u32 ret_l = piIoComm_sendRS485Tel(eCmdPiIoStartDataExchange, MODGATE_RS485_BROADCAST_ADDR,
					  &checksum, sizeof(checksum), NULL, NULL);
	msleep(90);		// wait a while
	if (ret_l)
		pr_err("piIoComm_sendRS485Tel(PiIoStartDataExchange) failed %d\n", ret_l);
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

u8 RevPiDevice_setStatus(u8 clr, u8 set)
{
	u8 status;

	spin_lock(&status_lock);
	status = (RevPiDevices_s.i8uStatus & ~clr) | set;
	RevPiDevices_s.i8uStatus = status;
	spin_unlock(&status_lock);

	return status;
}

u8 RevPiDevice_getStatus(void)
{
	return RevPiDevices_s.i8uStatus;
}

SDevice *RevPiDevice_getDev(u8 idx)
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

u8 RevPiDevice_getDevCnt(void)
{
	return RevPiDevices_s.i8uDeviceCount;
}

u8 RevPiDevice_getAddrLeft(void)
{
	return RevPiDevices_s.i8uAddressLeft;
}

u8 RevPiDevice_getAddrRight(void)
{
	return RevPiDevices_s.i8uAddressRight;
}

/*
 * True when no physical device is configured beyond the given address,
 * so the module at this address is the last device on the right side.
 * The device list also contains devices which are missing from the
 * scan, like gateways with old firmware or a module waiting in update
 * mode. The configuration does not change during a firmware update,
 * so it reflects the physical positions.
 */
static bool RevPiDevice_isLastRightDevice(u8 addr)
{
	SDevice *sdev;
	u16 type;
	int i;

	for (i = 0; i < RevPiDevice_getDevCnt(); i++) {
		sdev = RevPiDevice_getDev(i);
		type = sdev->sId.i16uModulType & PICONTROL_NOT_CONNECTED_MASK;

		/* only physical devices occupy a position */
		if (type == 0 || type >= PICONTROL_SW_OFFSET)
			continue;

		if (sdev->i8uAddress > addr)
			return false;
	}

	return true;
}

/*
 * Address used by the bootloader of the module during a firmware
 * update. The bootloader derives it from the sniff 1B pin: 2 when the
 * module is the last device on the right, 1 otherwise.
 */
u8 RevPiDevice_getFwuAddress(u8 addr)
{
	/* modules on the left side never sit at the right end */
	if (addr < REV_PI_DEV_FIRST_RIGHT)
		return 1;

	if (RevPiDevice_isLastRightDevice(addr))
		return 2;

	return 1;
}


u16 RevPiDevice_getErrCnt(void)
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

static int RevPiDevice_setModuleTermination(u8 address, bool terminate)
{
	u8 data;
	int ret;

	data = terminate ? 0 : 1;

	ret = piIoComm_sendRS485Tel(eCmdPiIoSetTermination, address, &data,
				    sizeof(data), NULL, NULL);
	if (ret) {
		pr_err("Failed to %s termination for module (address %d): %d\n",
			str_enable_disable(terminate), address, ret);
		goto fail;
	}

	if (terminate)
		pr_info("PiBridge termination enabled for module %d\n",
			address);
	else
		pr_debug("PiBridge termination disabled for module %d\n",
			address);
fail:

	return ret;
}

int RevPiDevice_setRightModuleTermination(bool terminate)
{
	int ret;

	if ((RevPiDevices_s.i8uAddressRight == REV_PI_DEV_FIRST_RIGHT) ||
	     RevPiDevices_s.gatewayRight)
		return -EOPNOTSUPP;
	/*
	 * The PiBridge protocol requires gaps between messages so wait a while
	 * before and after sending the command for module termination.
	 */
	msleep(3);

	ret = RevPiDevice_setModuleTermination(RevPiDevices_s.i8uAddressRight - 1,
					       terminate);
	msleep(3);

	return ret;
}

int RevPiDevice_setLeftModuleTermination(bool terminate)
{
	int ret;

	if ((RevPiDevices_s.i8uAddressLeft == REV_PI_DEV_FIRST_LEFT) ||
	     RevPiDevices_s.gatewayLeft)
		return -EOPNOTSUPP;
	/*
	 * The PiBridge protocol requires gaps between messages so wait a while
	 * before and after sending the command for module termination.
	 */
	msleep(3);

	ret = RevPiDevice_setModuleTermination(RevPiDevices_s.i8uAddressLeft + 1,
					       terminate);
	msleep(3);

	return ret;
}

int RevPiDevice_setBaseTermination(void)
{
	bool terminable;

	terminable = piCore_g.gpio_rs485_term &&
		     ((RevPiDevices_s.i8uAddressLeft == REV_PI_DEV_FIRST_LEFT) ||
		      (RevPiDevices_s.i8uAddressRight == REV_PI_DEV_FIRST_RIGHT));

	if (!terminable)
		return -EOPNOTSUPP;

	gpiod_set_value_cansleep(piCore_g.gpio_rs485_term, 1);

	return 0;
}

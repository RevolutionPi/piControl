// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: 2016-2024 KUNBUS GmbH

#include <linux/pibridge_comm.h>
#include <linux/cpufreq.h>
#include <linux/thermal.h>

#include "common_define.h"
#include "piAIOComm.h"
#include "piDIOComm.h"
#include "PiBridgeMaster.h"
#include "revpi_common.h"
#include "revpi_core.h"
#include "revpi_gate.h"
#include "revpi_mio.h"
#include "revpi_ro.h"
#include "RS485FwuCommand.h"
#include "piFirmwareUpdate.h"

#define MAX_CONFIG_RETRIES 3		// max. retries for configuring a IO module
#define MAX_INIT_RETRIES 1		// max. retries for configuring all IO modules
#define END_CONFIG_TIME	3000		// max. time for configuring IO modules, same timeout is used in the modules

/* The number of cycles after which the comm error counter is decreased */
#define COMM_ERROR_CYCLES		(1<<3) /* must be power of 2! */
#define COMM_ERROR_CYCLES_MASK		(COMM_ERROR_CYCLES - 1)
/* Error limit for error log message */
#define COMM_ERROR_LOG_LIMIT		10

static int init_retry = MAX_INIT_RETRIES;
static volatile TBOOL bEntering_s = bTRUE;
EPiBridgeMasterStatus eRunStatus_s = enPiBridgeMasterStatus_Init;
static enPiBridgeState eBridgeStateLast_s = piBridgeStop;

static INT32U i32uFWUAddress, i32uFWUSerialNum, i32uFWUFlashAddr, i32uFWUlength, i8uFWUScanned;
static INT32S i32sRetVal;
static char *pcFWUdata;

void PiBridgeMaster_Stop(void)
{
	my_rt_mutex_lock(&piCore_g.lockBridgeState);
	if (piDev_g.revpi_gate_supported)
		revpi_gate_fini();
	piCore_g.eBridgeState = piBridgeStop;
	clear_bit(PICONTROL_DEV_FLAG_RUNNING, &piDev_g.flags);
	rt_mutex_unlock(&piCore_g.lockBridgeState);
}

void PiBridgeMaster_Continue(void)
{
	// this function can only be called, if the driver was running before
	my_rt_mutex_lock(&piCore_g.lockBridgeState);
	if (piDev_g.revpi_gate_supported)
		revpi_gate_init();
	piCore_g.eBridgeState = piBridgeRun;
	set_bit(PICONTROL_DEV_FLAG_RUNNING, &piDev_g.flags);
	eRunStatus_s = enPiBridgeMasterStatus_Continue;	// make no initialization
	bEntering_s = bFALSE;
	rt_mutex_unlock(&piCore_g.lockBridgeState);
}

void PiBridgeMaster_Reset(void)
{
	my_rt_mutex_lock(&piCore_g.lockBridgeState);
	piCore_g.eBridgeState = piBridgeInit;
	clear_bit(PICONTROL_DEV_FLAG_RUNNING, &piDev_g.flags);
	eRunStatus_s = enPiBridgeMasterStatus_Init;
	bEntering_s = bTRUE;
	RevPiDevice_setStatus(0xff, 0);
	init_retry = MAX_INIT_RETRIES;

	RevPiDevice_init();
	rt_mutex_unlock(&piCore_g.lockBridgeState);
}

static void PiBridgeMaster_Configure(void)
{
	SDevice *sdev;
	int ret;
	int i;

	/* configure each module */
	for (i = 0; i < RevPiDevice_getDevCnt(); i++) {
		sdev = RevPiDevice_getDev(i);

		if (!sdev->i8uActive)
			continue;

		switch (sdev->sId.i16uModulType) {
		case KUNBUS_FW_DESCR_TYP_PI_DIO_14:
		case KUNBUS_FW_DESCR_TYP_PI_DI_16:
		case KUNBUS_FW_DESCR_TYP_PI_DO_16:
			ret = piDIOComm_Init(i);

			pr_debug("piDIOComm_Init(%d) done %d\n",
				sdev->i8uAddress, ret);

			if (ret != 0) {
				// init failed -> deactive module
				if (ret == 4) {
					pr_err("piDIOComm_Init(%d): Module not configured in PiCtory\n",
						sdev->i8uAddress);
				} else {
					pr_err("piDIOComm_Init(%d) failed, error %d\n",
						sdev->i8uAddress, ret);
				}
				sdev->i8uActive = 0;
			}
			break;
		case KUNBUS_FW_DESCR_TYP_PI_AIO:
			ret = piAIOComm_Init(i);

			pr_debug("piAIOComm_Init(%d) done %d\n", sdev->i8uAddress, ret);

			if (ret != 0) {
				// init failed -> deactive module
				if (ret == 4) {
					pr_err("piAIOComm_Init(%d): Module not configured in PiCtory\n",
						sdev->i8uAddress);
				} else {
					pr_err("piAIOComm_Init(%d) failed, error %d\n",
						sdev->i8uAddress, ret);
				}
				sdev->i8uActive = 0;
			}
			break;
		case KUNBUS_FW_DESCR_TYP_PI_MIO:
			ret = revpi_mio_init(i);

			if (ret) {
				pr_err("mio init failed in status-Continue(ret:%d)\n",
					ret);
				sdev->i8uActive = 0;
			}
			break;
		case KUNBUS_FW_DESCR_TYP_PI_RO:
			ret = revpi_ro_init(i);

			pr_debug("revpi_ro_init(%d) done %d\n", sdev->i8uAddress, ret);

			if (ret != 0) {
				// init failed -> deactivate module
				if (ret == 4) {
					pr_err("revpi_ro_init(%d): Module not configured in PiCtory\n",
						sdev->i8uAddress);
				} else {
					pr_err("revpi_ro_init(%d) failed, error %d\n",
						sdev->i8uAddress, ret);
				}
				sdev->i8uActive = 0;
			}
			break;
		}
	}
}

int PiBridgeMaster_Adjust(void)
{
	int i, j;
	int result = 0, found;
	uint8_t *state;

	if (piDev_g.devs == NULL || piDev_g.ent == NULL) {
		// config file could not be read, do nothing
		return -1;
	}

	state = kcalloc(piDev_g.devs->i16uNumDevices, sizeof(uint8_t), GFP_KERNEL);

	// Schleife über alle Module die automatisch erkannt wurden
	for (j = 0; j < RevPiDevice_getDevCnt(); j++) {
		// Suche diese Module in der Konfigurationsdatei
		for (i = 0, found = 0; found == 0 && i < piDev_g.devs->i16uNumDevices; i++) {
			// Grundvoraussetzung ist, dass die Adresse gleich ist.
			if (RevPiDevice_getDev(j)->i8uAddress == piDev_g.devs->dev[i].i8uAddress) {
				// Außerdem muss ModuleType, InputLength und OutputLength gleich sein.
				if (RevPiDevice_getDev(j)->sId.i16uModulType != piDev_g.devs->dev[i].i16uModuleType) {
					pr_warn("## address %d: incorrect module type %d != %d\n",
						RevPiDevice_getDev(j)->i8uAddress, RevPiDevice_getDev(j)->sId.i16uModulType,
						piDev_g.devs->dev[i].i16uModuleType);
					result = PICONTROL_CONFIG_ERROR_WRONG_MODULE_TYPE;
					RevPiDevice_setStatus(0, PICONTROL_STATUS_SIZE_MISMATCH);
					break;
				}
				if (RevPiDevice_getDev(j)->sId.i16uFBS_InputLength != piDev_g.devs->dev[i].i16uInputLength) {
					pr_warn("## address %d: incorrect input length %d != %d\n",
						RevPiDevice_getDev(j)->i8uAddress, RevPiDevice_getDev(j)->sId.i16uFBS_InputLength,
						piDev_g.devs->dev[i].i16uInputLength);
					result = PICONTROL_CONFIG_ERROR_WRONG_INPUT_LENGTH;
					RevPiDevice_setStatus(0, PICONTROL_STATUS_SIZE_MISMATCH);
					break;
				}
				if (RevPiDevice_getDev(j)->sId.i16uFBS_OutputLength != piDev_g.devs->dev[i].i16uOutputLength) {
					pr_warn("## address %d: incorrect output length %d != %d\n",
						RevPiDevice_getDev(j)->i8uAddress,
						RevPiDevice_getDev(j)->sId.i16uFBS_OutputLength,
						piDev_g.devs->dev[i].i16uOutputLength);
					result = PICONTROL_CONFIG_ERROR_WRONG_OUTPUT_LENGTH;
					RevPiDevice_setStatus(0, PICONTROL_STATUS_SIZE_MISMATCH);
					break;
				}
				// we found the device in the configuration file
				// -> adjust offsets
				pr_debug("Adjust: base %d in %d out %d conf %d\n",
					       piDev_g.devs->dev[i].i16uBaseOffset,
					       piDev_g.devs->dev[i].i16uInputOffset,
					       piDev_g.devs->dev[i].i16uOutputOffset,
					       piDev_g.devs->dev[i].i16uConfigOffset);

				RevPiDevice_getDev(j)->i16uInputOffset = piDev_g.devs->dev[i].i16uInputOffset;
				RevPiDevice_getDev(j)->i16uOutputOffset = piDev_g.devs->dev[i].i16uOutputOffset;
				RevPiDevice_getDev(j)->i16uConfigOffset = piDev_g.devs->dev[i].i16uConfigOffset;
				RevPiDevice_getDev(j)->i16uConfigLength = piDev_g.devs->dev[i].i16uConfigLength;
				if (j == 0) {
					RevPiDevice_setCoreOffset(RevPiDevice_getDev(0)->i16uInputOffset);
				}

				state[i] = 1;	// dieser Konfigeintrag wurde übernommen
				found = 1;	// innere For-Schrleife verlassen
			}
		}
		if (found == 0) {
			// Falls ein autom. erkanntes Modul in der Konfiguration nicht vorkommt, wird es deakiviert
			RevPiDevice_getDev(j)->i8uActive = 0;
			RevPiDevice_setStatus(0, PICONTROL_STATUS_EXTRA_MODULE);
		}
	}

	// nun wird die Liste der automatisch erkannten Module um die ergänzt, die nur in der Konfiguration vorkommen.
	for (i = 0; i < piDev_g.devs->i16uNumDevices; i++) {
		if (state[i] == 0) {
			j = RevPiDevice_getDevCnt();
			if ( piDev_g.devs->dev[i].i16uModuleType >= PICONTROL_SW_OFFSET
			  || piDev_g.devs->dev[i].i16uModuleType == KUNBUS_FW_DESCR_TYP_PI_CON_CAN
			  || piDev_g.devs->dev[i].i16uModuleType == KUNBUS_FW_DESCR_TYP_PI_CON_BT
			  || piDev_g.devs->dev[i].i16uModuleType == KUNBUS_FW_DESCR_TYP_PI_CON_MBUS) {
				// if a module is already defined as software module in the RAP file,
				// it is handled by user space software and therefore always active
				RevPiDevice_getDev(j)->i8uActive = 1;
				RevPiDevice_getDev(j)->sId.i16uModulType = piDev_g.devs->dev[i].i16uModuleType;
			} else {
				RevPiDevice_setStatus(0, PICONTROL_STATUS_MISSING_MODULE);
				RevPiDevice_getDev(j)->i8uActive = 0;
				RevPiDevice_getDev(j)->sId.i16uModulType =
				    piDev_g.devs->dev[i].i16uModuleType | PICONTROL_NOT_CONNECTED;
			}
			RevPiDevice_getDev(j)->i8uAddress = piDev_g.devs->dev[i].i8uAddress;
			RevPiDevice_getDev(j)->i8uScan = 0;
			RevPiDevice_getDev(j)->i16uInputOffset = piDev_g.devs->dev[i].i16uInputOffset;
			RevPiDevice_getDev(j)->i16uOutputOffset = piDev_g.devs->dev[i].i16uOutputOffset;
			RevPiDevice_getDev(j)->i16uConfigOffset = piDev_g.devs->dev[i].i16uConfigOffset;
			RevPiDevice_getDev(j)->i16uConfigLength = piDev_g.devs->dev[i].i16uConfigLength;
			RevPiDevice_getDev(j)->sId.i32uSerialnumber = piDev_g.devs->dev[i].i32uSerialnumber;
			RevPiDevice_getDev(j)->sId.i16uHW_Revision = piDev_g.devs->dev[i].i16uHW_Revision;
			RevPiDevice_getDev(j)->sId.i16uSW_Major = piDev_g.devs->dev[i].i16uSW_Major;
			RevPiDevice_getDev(j)->sId.i16uSW_Minor = piDev_g.devs->dev[i].i16uSW_Minor;
			RevPiDevice_getDev(j)->sId.i32uSVN_Revision = piDev_g.devs->dev[i].i32uSVN_Revision;
			RevPiDevice_getDev(j)->sId.i16uFBS_InputLength = piDev_g.devs->dev[i].i16uInputLength;
			RevPiDevice_getDev(j)->sId.i16uFBS_OutputLength = piDev_g.devs->dev[i].i16uOutputLength;
			RevPiDevice_getDev(j)->sId.i16uFeatureDescriptor = 0;	// not used
			RevPiDevice_incDevCnt();
		}
	}

	kfree(state);
	return result;
}

void PiBridgeMaster_setDefaults(void)
{
	int i;

	if (piDev_g.ent == NULL)
		return;

	memset(piDev_g.ai8uPIDefault, 0, KB_PI_LEN);

	for (i = 0; i < piDev_g.ent->i16uNumEntries; i++) {
		if (piDev_g.ent->ent[i].i32uDefault != 0) {
			pr_info_master2("addr %2d  type %2x  len %3d  offset %3d+%d  default %x\n",
					piDev_g.ent->ent[i].i8uAddress,
					piDev_g.ent->ent[i].i8uType,
					piDev_g.ent->ent[i].i16uBitLength,
					piDev_g.ent->ent[i].i16uOffset,
					piDev_g.ent->ent[i].i8uBitPos, piDev_g.ent->ent[i].i32uDefault);

			if (piDev_g.ent->ent[i].i16uBitLength == 1) {
				INT8U i8uValue, i8uMask, bit;
				unsigned int offset;

				offset = piDev_g.ent->ent[i].i16uOffset;
				bit = piDev_g.ent->ent[i].i8uBitPos;

				offset += bit / 8;
				bit %= 8;

				i8uValue = piDev_g.ai8uPIDefault[offset];

				i8uMask = (1 << bit);
				if (piDev_g.ent->ent[i].i32uDefault != 0)
					i8uValue |= i8uMask;
				else
					i8uValue &= ~i8uMask;
				piDev_g.ai8uPIDefault[offset] = i8uValue;
			} else if (piDev_g.ent->ent[i].i16uBitLength == 8) {
				piDev_g.ai8uPIDefault[piDev_g.ent->ent[i].i16uOffset] =
				    (INT8U) piDev_g.ent->ent[i].i32uDefault;
			} else if (piDev_g.ent->ent[i].i16uBitLength == 16
				   && piDev_g.ent->ent[i].i16uOffset < KB_PI_LEN - 1) {
				INT16U *pi16uPtr = (INT16U *) & piDev_g.ai8uPIDefault[piDev_g.ent->ent[i].i16uOffset];

				*pi16uPtr = (INT16U) piDev_g.ent->ent[i].i32uDefault;
			} else if (piDev_g.ent->ent[i].i16uBitLength == 32
				   && piDev_g.ent->ent[i].i16uOffset < KB_PI_LEN - 3) {
				INT32U *pi32uPtr = (INT32U *) & piDev_g.ai8uPIDefault[piDev_g.ent->ent[i].i16uOffset];

				*pi32uPtr = (INT32U) piDev_g.ent->ent[i].i32uDefault;
			}
		}
	}
}

static void handle_pibridge_ethernet(void)
{
	piDev_g.pibridge_mode_ethernet_left = false;
	piDev_g.pibridge_mode_ethernet_right = false;

	if (piCore_g.gpio_pbswitch_detect_left) {
		if (gpiod_get_value_cansleep(piCore_g.gpio_pbswitch_detect_left)) {
			gpiod_set_value_cansleep(piCore_g.gpio_pbswitch_mpx_left, 1);
			piDev_g.pibridge_mode_ethernet_left = true;
			pr_info("pileft: Set to ethernet mode\n");
		} else {
			gpiod_set_value_cansleep(piCore_g.gpio_pbswitch_mpx_left, 0);
			pr_info("pileft: Set to backplane mode\n");
		}
	}

	if (piCore_g.gpio_pbswitch_detect_right) {
		if (gpiod_get_value_cansleep(piCore_g.gpio_pbswitch_detect_right)) {
			gpiod_set_value_cansleep(piCore_g.gpio_pbswitch_mpx_right, 1);
			piDev_g.pibridge_mode_ethernet_right = true;
			pr_info("piright: Set to ethernet mode\n");
		} else {
			gpiod_set_value_cansleep(piCore_g.gpio_pbswitch_mpx_right, 0);
			pr_info("piright: Set to backplane mode\n");
		}
	}
}

int PiBridgeMaster_Run(void)
{
	static kbUT_Timer tTimeoutTimer_s;
	static kbUT_Timer tConfigTimeoutTimer_s;
	static int error_cnt;
	static u16 last_led;
	static u8 last_output;
	static unsigned long last_update;
	int ret = 0;
	int i;

	my_rt_mutex_lock(&piCore_g.lockBridgeState);
	if (piCore_g.eBridgeState != piBridgeStop) {
		switch (eRunStatus_s) {
		case enPiBridgeMasterStatus_Init:	// Do some initializations and go to next state
			pr_debug("Enter Init State\n");
			handle_pibridge_ethernet();
			// configure PiBridge Sniff lines as input
			piIoComm_writeSniff1A(enGpioValue_Low, enGpioMode_Input);
			piIoComm_writeSniff1B(enGpioValue_Low, enGpioMode_Input);
			piIoComm_writeSniff2A(enGpioValue_Low, enGpioMode_Input);
			piIoComm_writeSniff2B(enGpioValue_Low, enGpioMode_Input);

			eRunStatus_s = enPiBridgeMasterStatus_MasterIsPresentSignalling1;
			bEntering_s = bTRUE;
			break;
			// *****************************************************************************************

		case enPiBridgeMasterStatus_MasterIsPresentSignalling1:
			if (bEntering_s) {
				pr_debug("Enter PresentSignalling1 State\n");

				bEntering_s = bFALSE;
				piIoComm_writeSniff2A(enGpioValue_High, enGpioMode_Output);
				piIoComm_writeSniff2B(enGpioValue_High, enGpioMode_Output);

				/*
				 * This delay defines the length of the high
				 * pulse at the begin of the present signaling.
				 * The module (at least the DIO module) expects
				 * this pulse to be between 7.5 and 9.5 ms.
				 * Pushing the delay to the upper limit might
				 * cause issues. It might take some time to
				 * switch the pin. Especially on devices which
				 * use an io expander like the Connect 4.
				 */
				usleep_range(8500, 8500);

				piIoComm_writeSniff2A(enGpioValue_Low, enGpioMode_Input);
				piIoComm_writeSniff2B(enGpioValue_Low, enGpioMode_Input);
				kbUT_TimerStart(&tTimeoutTimer_s, 30);
			}
			if (kbUT_TimerExpired(&tTimeoutTimer_s)) {
				kbUT_TimerStart(&tConfigTimeoutTimer_s, END_CONFIG_TIME);
				if (piDev_g.only_left_pibridge) {
					// the RevPi Connect has I/O modules only on the left side
					eRunStatus_s = enPiBridgeMasterStatus_InitialSlaveDetectionLeft;
				} else {
					// start serching for I/O modules on the right side
					eRunStatus_s = enPiBridgeMasterStatus_InitialSlaveDetectionRight;
				}
				bEntering_s = bTRUE;
			}
			break;
			// *****************************************************************************************

		case enPiBridgeMasterStatus_InitialSlaveDetectionRight:
			pr_debug("Enter InitialSlaveDetectionRight State\n");

			if (piDev_g.pibridge_mode_ethernet_right) {
				eRunStatus_s = enPiBridgeMasterStatus_InitialSlaveDetectionLeft;
			} else {
				piIoComm_readSniff1A();
				piIoComm_readSniff1B();

				if (piIoComm_readSniff2B() == enGpioValue_High)
					eRunStatus_s = enPiBridgeMasterStatus_ConfigRightStart;
				else
					eRunStatus_s = enPiBridgeMasterStatus_InitialSlaveDetectionLeft;
			}
			bEntering_s = bTRUE;
			break;
			// *****************************************************************************************

		case enPiBridgeMasterStatus_ConfigRightStart:
			if (bEntering_s) {
				pr_debug("Enter ConfigRightStart State\n");
				bEntering_s = bFALSE;
				piIoComm_writeSniff1B(enGpioValue_Low, enGpioMode_Output);
				kbUT_TimerStart(&tTimeoutTimer_s, 10);
			}
			if (kbUT_TimerExpired(&tTimeoutTimer_s)) {
				eRunStatus_s = enPiBridgeMasterStatus_ConfigDialogueRight;
				bEntering_s = bTRUE;
			}
			break;
			// *****************************************************************************************

		case enPiBridgeMasterStatus_ConfigDialogueRight:
			if (bEntering_s) {
				pr_debug("Enter ConfigDialogueRight State\n");
				error_cnt = 0;
				bEntering_s = bFALSE;
			}
			// Write configuration data to the currently selected slave
			if (RevPiDevice_writeNextConfigurationRight() == bFALSE) {
				error_cnt++;
				if (error_cnt > MAX_CONFIG_RETRIES) {
					// no more slaves on the right side, configure left slaves
					eRunStatus_s = enPiBridgeMasterStatus_InitialSlaveDetectionLeft;
					bEntering_s = bTRUE;
				}
			} else {
				eRunStatus_s = enPiBridgeMasterStatus_SlaveDetectionRight;
				bEntering_s = bTRUE;
			}
			break;
			// *****************************************************************************************

		case enPiBridgeMasterStatus_SlaveDetectionRight:
			if (bEntering_s) {
				pr_debug("Enter SlaveDetectionRight State\n");
				bEntering_s = bFALSE;
				kbUT_TimerStart(&tTimeoutTimer_s, 10);
			}
			if (kbUT_TimerExpired(&tTimeoutTimer_s)) {
				if (piIoComm_readSniff2B() == enGpioValue_High) {
					// configure next right slave
					eRunStatus_s = enPiBridgeMasterStatus_ConfigDialogueRight;
					bEntering_s = bTRUE;
				} else {
					// no more slaves on the right side, configure left slaves
					eRunStatus_s = enPiBridgeMasterStatus_InitialSlaveDetectionLeft;
					bEntering_s = bTRUE;
				}
			}
			break;
			// *****************************************************************************************

		case enPiBridgeMasterStatus_InitialSlaveDetectionLeft:
			pr_debug("Enter InitialSlaveDetectionLeft State\n");

			if (piDev_g.pibridge_mode_ethernet_left) {
				eRunStatus_s = enPiBridgeMasterStatus_EndOfConfig;
			} else {
				piIoComm_writeSniff1B(enGpioValue_Low, enGpioMode_Input);

				if (piIoComm_readSniff2A() == enGpioValue_High) {
					// configure first left slave
					eRunStatus_s = enPiBridgeMasterStatus_ConfigLeftStart;
				} else {
					// no slave on the left side
					eRunStatus_s = enPiBridgeMasterStatus_EndOfConfig;
				}
			}
			bEntering_s = bTRUE;
			break;
			// *****************************************************************************************

		case enPiBridgeMasterStatus_ConfigLeftStart:
			if (bEntering_s) {
				pr_debug("Enter ConfigLeftStart State\n");
				bEntering_s = bFALSE;
				piIoComm_writeSniff1A(enGpioValue_Low, enGpioMode_Output);
				kbUT_TimerStart(&tTimeoutTimer_s, 10);
			}
			if (kbUT_TimerExpired(&tTimeoutTimer_s)) {
				eRunStatus_s = enPiBridgeMasterStatus_ConfigDialogueLeft;
				bEntering_s = bTRUE;
			}
			break;
			// *****************************************************************************************

		case enPiBridgeMasterStatus_ConfigDialogueLeft:
			if (bEntering_s) {
				pr_debug("Enter ConfigDialogueLeft State\n");
				error_cnt = 0;
				bEntering_s = bFALSE;
			}
			// Write configuration data to the currently selected slave
			if (RevPiDevice_writeNextConfigurationLeft() == bFALSE) {
				error_cnt++;
				if (error_cnt > MAX_CONFIG_RETRIES) {
					// no more slaves on the right side, configure left slaves
					eRunStatus_s = enPiBridgeMasterStatus_EndOfConfig;
					bEntering_s = bTRUE;
				}
			} else {
				eRunStatus_s = enPiBridgeMasterStatus_SlaveDetectionLeft;
				bEntering_s = bTRUE;
			}
			break;
			// *****************************************************************************************

		case enPiBridgeMasterStatus_SlaveDetectionLeft:
			if (bEntering_s) {
				pr_debug("Enter SlaveDetectionLeft State\n");
				bEntering_s = bFALSE;
				kbUT_TimerStart(&tTimeoutTimer_s, 10);
			}
			if (kbUT_TimerExpired(&tTimeoutTimer_s)) {
				if (piIoComm_readSniff2A() == enGpioValue_High) {
					// configure next left slave
					eRunStatus_s = enPiBridgeMasterStatus_ConfigDialogueLeft;
					bEntering_s = bTRUE;
				} else {
					// no more slaves on the left
					eRunStatus_s = enPiBridgeMasterStatus_EndOfConfig;
					bEntering_s = bTRUE;
				}
			}
			break;
			// *****************************************************************************************

		case enPiBridgeMasterStatus_Continue:
			msleep(100);	// wait a while
			pr_info("start data exchange\n");
			RevPiDevice_startDataexchange();
			msleep(110);	// wait a while

			PiBridgeMaster_Configure();

			ret = 0;

			eRunStatus_s = enPiBridgeMasterStatus_EndOfConfig;
			bEntering_s = bFALSE;
			break;

		case enPiBridgeMasterStatus_EndOfConfig:
			if (bEntering_s) {
#ifdef DEBUG_MASTER_STATE
				pr_debug("Enter EndOfConfig State\n\n");
				for (i = 0; i < RevPiDevice_getDevCnt(); i++) {
					pr_debug("Device %2d: Addr %2d Type %3d  Act %d  In %3d Out %3d\n",
						       i,
						       RevPiDevice_getDev(i)->i8uAddress,
						       RevPiDevice_getDev(i)->sId.i16uModulType,
						       RevPiDevice_getDev(i)->i8uActive,
						       RevPiDevice_getDev(i)->sId.i16uFBS_InputLength,
						       RevPiDevice_getDev(i)->sId.i16uFBS_OutputLength);
					pr_debug("           input offset  %5d  len %3d\n",
						       RevPiDevice_getDev(i)->i16uInputOffset,
						       RevPiDevice_getDev(i)->sId.i16uFBS_InputLength);
					pr_debug("           output offset %5d  len %3d\n",
						       RevPiDevice_getDev(i)->i16uOutputOffset,
						       RevPiDevice_getDev(i)->sId.i16uFBS_OutputLength);
					pr_debug("           serial number %d  version %d.%d\n",
						       RevPiDevice_getDev(i)->sId.i32uSerialnumber,
						       RevPiDevice_getDev(i)->sId.i16uSW_Major,
						       RevPiDevice_getDev(i)->sId.i16uSW_Minor);
				}

				pr_debug("\n");
#endif

				piIoComm_writeSniff1A(enGpioValue_Low, enGpioMode_Input);

				PiBridgeMaster_Adjust();

#ifdef DEBUG_MASTER_STATE
				pr_debug("After Adjustment\n");
				for (i = 0; i < RevPiDevice_getDevCnt(); i++) {
					pr_info_master("Device %2d: Addr %2d Type %3d  Act %d  In %3d Out %3d\n",
						       i,
						       RevPiDevice_getDev(i)->i8uAddress,
						       RevPiDevice_getDev(i)->sId.i16uModulType,
						       RevPiDevice_getDev(i)->i8uActive,
						       RevPiDevice_getDev(i)->sId.i16uFBS_InputLength,
						       RevPiDevice_getDev(i)->sId.i16uFBS_OutputLength);
					pr_info_master("           input offset  %5d  len %3d\n",
						       RevPiDevice_getDev(i)->i16uInputOffset,
						       RevPiDevice_getDev(i)->sId.i16uFBS_InputLength);
					pr_info_master("           output offset %5d  len %3d\n",
						       RevPiDevice_getDev(i)->i16uOutputOffset,
						       RevPiDevice_getDev(i)->sId.i16uFBS_OutputLength);
				}
				pr_info_master("\n");
#endif
				PiBridgeMaster_setDefaults();

				my_rt_mutex_lock(&piDev_g.lockPI);
				memcpy(piDev_g.ai8uPI, piDev_g.ai8uPIDefault, KB_PI_LEN);
				rt_mutex_unlock(&piDev_g.lockPI);
				msleep(100);	// wait a while
				pr_info("start data exchange\n");
				RevPiDevice_startDataexchange();
				msleep(110);	// wait a while

				// send config messages
				PiBridgeMaster_Configure();

				bEntering_s = bFALSE;
				ret = 0;
			} else {
				/* Start cycle measurement */
				piCore_g.data_exchange_running = true;
			}

			/*
			 * Decrease error counter after each COMM_ERROR_CYCLES
			 * to let the accumulated errors 'drain out' if there
			 * are not more errors at this time.
			 */
			if (piCore_g.comm_errors &&
			    (!(piCore_g.cycle_num & COMM_ERROR_CYCLES_MASK)))
				piCore_g.comm_errors--;

			if (RevPiDevice_run()) {
				piCore_g.comm_errors++;

				if (piCore_g.comm_errors > COMM_ERROR_LOG_LIMIT) {
					pr_warn_ratelimited("Error during piBridge communication\n");
					piCore_g.comm_errors = 0;
				}
				// an error occured, check error limits
				if (piCore_g.image.usr.i16uRS485ErrorLimit2 > 0
				    && piCore_g.image.usr.i16uRS485ErrorLimit2 < RevPiDevice_getErrCnt()) {
					pr_err("too many communication errors -> set state to stopped\n");
					if (piDev_g.revpi_gate_supported)
						revpi_gate_fini();
					piCore_g.eBridgeState = piBridgeStop;
					clear_bit(PICONTROL_DEV_FLAG_RUNNING, &piDev_g.flags);
				} else if (piCore_g.image.usr.i16uRS485ErrorLimit1 > 0
					   && piCore_g.image.usr.i16uRS485ErrorLimit1 < RevPiDevice_getErrCnt()) {
					// bad communication with inputs -> set inputs to default values
					pr_err("too many communication errors -> set inputs to default %d %d %d %d   %d %d %d %d\n",
						RevPiDevice_getDev(0)->i16uErrorCnt, RevPiDevice_getDev(1)->i16uErrorCnt,
						RevPiDevice_getDev(2)->i16uErrorCnt, RevPiDevice_getDev(3)->i16uErrorCnt,
						RevPiDevice_getDev(4)->i16uErrorCnt, RevPiDevice_getDev(5)->i16uErrorCnt,
						RevPiDevice_getDev(6)->i16uErrorCnt, RevPiDevice_getDev(7)->i16uErrorCnt);
				}
			} else {
				ret = 1;
			}
			piCore_g.image.drv.i16uRS485ErrorCnt = RevPiDevice_getErrCnt();
			break;
			// *****************************************************************************************

		case enPiBridgeMasterStatus_InitRetry:
			if (bEntering_s) {
				pr_info_master("Enter Initialization Retry\n");
				bEntering_s = bFALSE;
			}
			if (kbUT_TimerExpired(&tConfigTimeoutTimer_s)) {
				piCore_g.eBridgeState = piBridgeInit;
				clear_bit(PICONTROL_DEV_FLAG_RUNNING, &piDev_g.flags);
				eRunStatus_s = enPiBridgeMasterStatus_Init;
				bEntering_s = bTRUE;
				RevPiDevice_setStatus(0xff, 0);

				RevPiDevice_init();
			}
			break;

		default:
			break;

		}

		if (ret && piCore_g.eBridgeState != piBridgeRun) {
			if (init_retry > 0
			    && (piIoComm_readSniff2A() || piIoComm_readSniff2B()
				|| (RevPiDevice_getStatus() & PICONTROL_STATUS_MISSING_MODULE))) {
				// at least one IO module did not complete the initialization process
				// wait for the timeout in the module
				pr_info("initialization of module not finished (%d,%d,%d) -> retry\n", piIoComm_readSniff2A(), piIoComm_readSniff2B(), (RevPiDevice_getStatus() & PICONTROL_STATUS_MISSING_MODULE));
				eRunStatus_s = enPiBridgeMasterStatus_InitRetry;
				bEntering_s = bTRUE;
				piCore_g.eBridgeState = piBridgeInit;
				clear_bit(PICONTROL_DEV_FLAG_RUNNING, &piDev_g.flags);
				init_retry--;
			} else {
				pr_info("set state to running\n");
				if (piDev_g.revpi_gate_supported)
					revpi_gate_init();
				piCore_g.eBridgeState = piBridgeRun;
				set_bit(PICONTROL_DEV_FLAG_RUNNING, &piDev_g.flags);
			}
		}
	} else	{		// piCore_g.eBridgeState == piBridgeStop
		if (eRunStatus_s == enPiBridgeMasterStatus_EndOfConfig) {
			pr_info("stop data exchange\n");
			ret = piIoComm_gotoGateProtocol();
			pr_info("piIoComm_gotoGateProtocol returned %d\n", ret);
			eRunStatus_s = enPiBridgeMasterStatus_Init;
			piCore_g.data_exchange_running = false;
		} else if (eRunStatus_s == enPiBridgeMasterStatus_FWUMode) {
			if (bEntering_s) {
				if (i8uFWUScanned == 0) {
					// old mGates always use 2
					i32uFWUAddress = 2;
				}

				i32sRetVal = fwuEnterFwuMode(i32uFWUAddress);
				pr_info("fwuEnterFwuMode returned %d\n", i32sRetVal);

				if (i32uFWUAddress < REV_PI_DEV_FIRST_RIGHT) {
					i32uFWUAddress = 1;	// address must be 1 in the following calls
				} else {
					if (i32uFWUAddress == RevPiDevice_getAddrRight() - 1)
						i32uFWUAddress = 2;	// address must be 2 in the following calls
					else
						i32uFWUAddress = 1;	// address must be 1 in the following calls
				}
				pr_info("using address %d\n", i32uFWUAddress);

				ret = 0;	// do not return errors here
				bEntering_s = bFALSE;
			}
		} else if (eRunStatus_s == enPiBridgeMasterStatus_ProgramSerialNum) {
			if (bEntering_s) {
				i32sRetVal = fwuWriteSerialNum(i32uFWUAddress, i32uFWUSerialNum);
				pr_info("fwuWriteSerialNum returned %d\n", i32sRetVal);

				ret = 0;	// do not return errors here
				bEntering_s = bFALSE;
			}
		} else if (eRunStatus_s == enPiBridgeMasterStatus_FWUFlashErase) {
			if (bEntering_s) {
				i32sRetVal = erase_flash(i32uFWUAddress);
				pr_info("fwuEraseFlash returned %d\n", i32sRetVal);

				ret = 0;	// do not return errors here
				bEntering_s = bFALSE;
			}
		} else if (eRunStatus_s == enPiBridgeMasterStatus_FWUFlashWrite) {
			if (bEntering_s) {
				i32sRetVal = flash_firmware(i32uFWUAddress,
							    i32uFWUFlashAddr,
							    pcFWUdata,
							    i32uFWUlength);
				ret = 0;	// do not return errors here
				bEntering_s = bFALSE;
			}
		} else if (eRunStatus_s == enPiBridgeMasterStatus_FWUReset) {
			if (bEntering_s) {
				i32sRetVal = fwuResetModule(i32uFWUAddress);
				pr_info("fwuResetModule returned %d\n", i32sRetVal);

				ret = 0;	// do not return errors here
				bEntering_s = bFALSE;
			}
		}

		/* If requested by user, send internal io/gate telegram(s) */
		RevPiDevice_handle_internal_telegrams();
	}

	rt_mutex_unlock(&piCore_g.lockBridgeState);

	if (test_bit(PICONTROL_DEV_FLAG_STOP_IO, &piDev_g.flags)) {
		revpi_power_led_red_set(REVPI_POWER_LED_FLICKR);
	} else  {
		if (piCore_g.eBridgeState == piBridgeRun) {
			if (RevPiDevice_getErrCnt() == 0) {
				revpi_power_led_red_set(REVPI_POWER_LED_OFF);	// green on
			} else {
				revpi_power_led_red_set(REVPI_POWER_LED_ON_1000MS);	// red for 1s, then green
			}
		} else {
			revpi_power_led_red_set(REVPI_POWER_LED_ON);	// red on
		}
		eBridgeStateLast_s = piCore_g.eBridgeState;
	}

	revpi_power_led_red_run();

	if (isRunning()) {
		RevPiDevice_setStatus(0, PICONTROL_STATUS_RUNNING);
	} else {
		RevPiDevice_setStatus(PICONTROL_STATUS_RUNNING, 0);
	}

	// set LEDs, Status and GPIOs
	if (piDev_g.machine_type == REVPI_CONNECT ||
	    piDev_g.machine_type == REVPI_CONNECT_SE ||
	    piDev_g.machine_type == REVPI_CONNECT_4) {
		if (gpiod_get_value_cansleep(piCore_g.gpio_x2di)) {
			RevPiDevice_setStatus(0, PICONTROL_STATUS_X2_DIN);
		} else {
			RevPiDevice_setStatus(PICONTROL_STATUS_X2_DIN, 0);
		}
	}
	piCore_g.image.drv.i8uStatus = RevPiDevice_getStatus();

	if (piDev_g.machine_type == REVPI_CONNECT_4 ||
	    piDev_g.machine_type == REVPI_CONNECT_5) {
		revpi_rgb_led_trigger_event(last_led, piCore_g.image.usr.rgb_leds);
	} else {
		revpi_led_trigger_event(last_led, piCore_g.image.usr.leds);
	}
	if (piDev_g.machine_type == REVPI_CONNECT ||
	    piDev_g.machine_type == REVPI_CONNECT_SE) {
		if ((last_led ^ piCore_g.image.usr.leds) & PICONTROL_X2_DOUT) {
			gpiod_set_value(piCore_g.gpio_x2do, (piCore_g.image.usr.leds & PICONTROL_X2_DOUT) ? 1 : 0);
		}
		if ((last_led ^ piCore_g.image.usr.leds) & PICONTROL_WD_TRIGGER) {
			gpiod_set_value(piCore_g.gpio_wdtrigger, (piCore_g.image.usr.leds & PICONTROL_WD_TRIGGER) ? 1 : 0);
		}
	}
	if (piDev_g.machine_type == REVPI_CONNECT_4) {
		if ((last_output ^ piCore_g.image.usr.outputs) & PICONTROL_X2_DOUT_CONNECT4) {
			gpiod_set_value(piCore_g.gpio_x2do, (piCore_g.image.usr.outputs & PICONTROL_X2_DOUT_CONNECT4) ? 1 : 0);
		}
	}
	if (piDev_g.machine_type == REVPI_CONNECT_4 ||
            piDev_g.machine_type == REVPI_CONNECT_5) {
		last_led = piCore_g.image.usr.rgb_leds;
		last_output = piCore_g.image.usr.outputs;
	} else {
		last_led = piCore_g.image.usr.leds;
	}

	// update every 1 sec
	if ((kbUT_getCurrentMs() - last_update) > 1000) {
		if (piDev_g.thermal_zone != NULL) {
			int temp, ret;

			ret = thermal_zone_get_temp(piDev_g.thermal_zone, &temp);
			if (ret) {
				pr_err("could not read cpu temperature\n");
			} else {
				piCore_g.image.drv.i8uCPUTemperature = temp / 1000;
			}
		}

		/*
		   Get the CPU clock from CPU0 in kHz
		   and divide it down to fit into 8 bits.
		*/
		piCore_g.image.drv.i8uCPUFrequency =
			cpufreq_quick_get(0) / 10000;


		last_update = kbUT_getCurrentMs();
	}

	if (piCore_g.eBridgeState == piBridgeRun) {
		//flip_process_image(&piCore_g.image, RevPiDevice_getCoreOffset());
		if (!test_bit(PICONTROL_DEV_FLAG_STOP_IO, &piDev_g.flags)) {
			INT8U *p1, *p2;
			SRevPiProcessImage *pI1, *pI2;
			p1 = piDev_g.ai8uPI + RevPiDevice_getCoreOffset();
			p2 = (INT8U *)&piCore_g.image;
			pI1 = (SRevPiProcessImage *)p1;
			pI2 = (SRevPiProcessImage *)p2;
			my_rt_mutex_lock(&piDev_g.lockPI);
			pI1->drv = pI2->drv;
			// The size of _SRevPiProcessImage.usr was 5 bytes before the field rgb_leds was introduced
			// with Connect 4 and the size changed to 7 bytes. In order to maintain compatibility with existing deviecs,
			// only the number of bytes defined in MODGATECOM_IDResp.i16uFBS_OutputLength is copied with memcpy.
			memcpy(&pI2->usr, &pI1->usr, RevPiDevice_getDev(0)->sId.i16uFBS_OutputLength);
			rt_mutex_unlock(&piDev_g.lockPI);
		}
	}

	return ret;
}

//-------------------------------------------------------------------------------------------------------------------------
// the following functions are called from the ioctl funtion which is executed in the application task
// they block using msleep until their task is completed, which is signalled by reseting the flags bEntering_s to bFALSE.

INT32S PiBridgeMaster_FWUModeEnter(INT32U address, INT8U i8uScanned)
{
	if (piCore_g.eBridgeState == piBridgeStop) {
		i32uFWUAddress = address;
		i8uFWUScanned = i8uScanned;
		eRunStatus_s = enPiBridgeMasterStatus_FWUMode;
		bEntering_s = bTRUE;
		do {
			msleep(10);
		} while (bEntering_s);
		return i32sRetVal;
	}
	return -1;
}

INT32S PiBridgeMaster_FWUsetSerNum(INT32U serNum)
{
	if (piCore_g.eBridgeState == piBridgeStop) {
		i32uFWUSerialNum = serNum;
		eRunStatus_s = enPiBridgeMasterStatus_ProgramSerialNum;
		bEntering_s = bTRUE;
		do {
			msleep(10);
		} while (bEntering_s);
		return i32sRetVal;
	}
	return -1;
}

INT32S PiBridgeMaster_FWUflashErase(void)
{
	if (piCore_g.eBridgeState == piBridgeStop) {
		eRunStatus_s = enPiBridgeMasterStatus_FWUFlashErase;
		bEntering_s = bTRUE;
		do {
			msleep(10);
		} while (bEntering_s);
		return i32sRetVal;
	}
	return -1;
}

INT32S PiBridgeMaster_FWUflashWrite(INT32U flashAddr, char *data, INT32U length)
{
	if (piCore_g.eBridgeState == piBridgeStop) {
		i32uFWUFlashAddr = flashAddr;
		pcFWUdata = data;
		i32uFWUlength = length;
		eRunStatus_s = enPiBridgeMasterStatus_FWUFlashWrite;
		bEntering_s = bTRUE;
		do {
			msleep(10);
		} while (bEntering_s);
		return i32sRetVal;
	}
	return -1;
}

INT32S PiBridgeMaster_FWUReset(void)
{
	if (piCore_g.eBridgeState == piBridgeStop) {
		eRunStatus_s = enPiBridgeMasterStatus_FWUReset;
		bEntering_s = bTRUE;
		do {
			msleep(10);
		} while (bEntering_s);
		return i32sRetVal;
	}
	return -1;
}

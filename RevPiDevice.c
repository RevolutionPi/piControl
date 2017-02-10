//+=============================================================================================
//|
//!		\file RevPiDevice.cpp
//!		Device administration for PiBridge master modules
//|
//+---------------------------------------------------------------------------------------------
//|
//|		File-ID:		$Id: RevPiDevice.c 506 2017-01-31 07:03:32Z mduckeck $
//|		Location:		$URL: http://srv-kunbus03.de.pilz.local/raspi/trunk/products/PiCore/piKernelMod/RevPiDevice.c $
//|		Company:		$Cpn:$
//|
//+=============================================================================================

//#include <stdio.h>
#include <linux/module.h>    // included for all kernel modules
#include <linux/delay.h>

#include <project.h>

#include <common_define.h>
#include <kbUtilities.h>
#include <ModGateRS485.h>
#include <ModGateComMain.h>

#include <PiBridgeMaster.h>
#include <RevPiDevice.h>
#include <piControlMain.h>
#include <piDIOComm.h>

SDeviceConfig RevPiScan;

static const MODGATECOM_IDResp RevPi_ID_s =
{
    .i32uSerialnumber           = 1,
    .i16uModulType              = KUNBUS_FW_DESCR_TYP_PI_CORE,
    .i16uHW_Revision            = 1,
    .i16uSW_Major               = 1,
    .i16uSW_Minor               = 1,  //TODO
    .i32uSVN_Revision           = 0,
    .i16uFBS_InputLength        = 3,
    .i16uFBS_OutputLength       = 5,
    .i16uFeatureDescriptor      = MODGATE_feature_IODataExchange
};


void RevPiDevice_init(void)
{
    printk("RevPiDevice_init()\n");
    piDev_g.i8uLeftMGateIdx = REV_PI_DEV_UNDEF;
    piDev_g.i8uRightMGateIdx = REV_PI_DEV_UNDEF;
    RevPiScan.i8uAddressRight = REV_PI_DEV_FIRST_RIGHT;    // first address of a right side module
    RevPiScan.i8uAddressLeft  = REV_PI_DEV_FIRST_RIGHT-1;  // first address of a left side module
    RevPiScan.i8uDeviceCount = 0;   // counter for detected devices
    RevPiScan.i16uErrorCnt = 0;

    // RevPi as first entry to device list
    RevPiScan.dev[RevPiScan.i8uDeviceCount].i8uAddress = 0;
    RevPiScan.dev[RevPiScan.i8uDeviceCount].sId = RevPi_ID_s;
    RevPiScan.dev[RevPiScan.i8uDeviceCount].i16uInputOffset = 0;
    RevPiScan.dev[RevPiScan.i8uDeviceCount].i16uOutputOffset = (INT16U)((int)&((SRevPiCoreImage *)0)->i8uLED);
    RevPiScan.dev[RevPiScan.i8uDeviceCount].i8uActive = 1;
    RevPiScan.i8uDeviceCount++;
}

void RevPiDevice_finish(void)
{
    //printk("RevPiDevice_finish()\n");
    piIoComm_finish();
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

    RevPiScan.i16uErrorCnt = 0;

    for (i8uDevice = 0; i8uDevice < RevPiScan.i8uDeviceCount; i8uDevice++)
    {
        if (RevPiScan.dev[i8uDevice].i8uActive)
        {
            switch (RevPiScan.dev[i8uDevice].sId.i16uModulType)
            {
            case KUNBUS_FW_DESCR_TYP_PI_DIO_14:
            case KUNBUS_FW_DESCR_TYP_PI_DI_16:
            case KUNBUS_FW_DESCR_TYP_PI_DO_16:
                r = piDIOComm_sendCyclicTelegram(i8uDevice);
                if (r)
                {
                    if (RevPiScan.dev[i8uDevice].i16uErrorCnt < 255)
                    {
                        RevPiScan.dev[i8uDevice].i16uErrorCnt++;
                        retval = -1;    // tell calling function that an error occured
                    }
                }
                else
                {
                    RevPiScan.dev[i8uDevice].i16uErrorCnt = 0;
                }
                break;

            case KUNBUS_FW_DESCR_TYP_MG_CAN_OPEN:
            case KUNBUS_FW_DESCR_TYP_MG_CCLINK:
            case KUNBUS_FW_DESCR_TYP_MG_DEV_NET:
            case KUNBUS_FW_DESCR_TYP_MG_ETHERCAT:
            case KUNBUS_FW_DESCR_TYP_MG_ETHERNET_IP:
            case KUNBUS_FW_DESCR_TYP_MG_POWERLINK:
            case KUNBUS_FW_DESCR_TYP_MG_PROFIBUS:
            case KUNBUS_FW_DESCR_TYP_MG_PROFINET_RT:
            case KUNBUS_FW_DESCR_TYP_MG_PROFINET_IRT:
            case KUNBUS_FW_DESCR_TYP_MG_CAN_OPEN_MASTER:
            case KUNBUS_FW_DESCR_TYP_MG_SERCOS3:
            case KUNBUS_FW_DESCR_TYP_MG_SERIAL:
            case KUNBUS_FW_DESCR_TYP_MG_PROFINET_RT_MASTER:
            case KUNBUS_FW_DESCR_TYP_MG_PROFINET_IRT_MASTER:
            case KUNBUS_FW_DESCR_TYP_MG_ETHERCAT_MASTER:
            case KUNBUS_FW_DESCR_TYP_MG_MODBUS_RTU:
            case KUNBUS_FW_DESCR_TYP_MG_MODBUS_TCP:
            case KUNBUS_FW_DESCR_TYP_MG_DMX:
                if (  piDev_g.i8uRightMGateIdx == REV_PI_DEV_UNDEF
                   && RevPiScan.dev[i8uDevice].i8uAddress >= REV_PI_DEV_FIRST_RIGHT)
                {
                    piDev_g.i8uRightMGateIdx = i8uDevice;
                }
                else if (piDev_g.i8uLeftMGateIdx == REV_PI_DEV_UNDEF
                      && RevPiScan.dev[i8uDevice].i8uAddress < REV_PI_DEV_FIRST_RIGHT)
                {
                    piDev_g.i8uLeftMGateIdx = i8uDevice;
                }
                break;

            default:
                // user devices are ignored here
            break;
            }
            RevPiScan.i16uErrorCnt += RevPiScan.dev[i8uDevice].i16uErrorCnt;
        }
	}
    return retval;
}

TBOOL RevPiDevice_writeNextConfiguration(INT8U i8uAddress_p, MODGATECOM_IDResp *pModgateId_p)
{
    INT32U ret_l;
    // 
    ret_l = piIoComm_sendRS485Tel(eCmdGetDeviceInfo, i8uAddress_p, NULL, 0, (INT8U*)pModgateId_p, sizeof(MODGATECOM_IDResp));
    msleep(10);    // wait a while
    if (ret_l)
    {
#ifdef DEBUG_DEVICE
        DF_PRINTK("piIoComm_sendRS485Tel(GetDeviceInfo) failed %d\n", ret_l);
#endif
        return bFALSE;
    }

    ret_l = piIoComm_sendRS485Tel(eCmdPiIoConfigure, i8uAddress_p, NULL, 0, NULL, 0);
    msleep(10);    // wait a while
    if (ret_l)
    {
#ifdef DEBUG_DEVICE
        DF_PRINTK("piIoComm_sendRS485Tel(PiIoConfigure) failed %d\n", ret_l);
#endif
        return bFALSE;
	}

    ret_l = piIoComm_sendRS485Tel(eCmdPiIoSetAddress, i8uAddress_p, NULL, 0, NULL, 0);
    msleep(10);    // wait a while
    if (ret_l)
    {
#ifdef DEBUG_DEVICE
        DF_PRINTK("piIoComm_sendRS485Tel(PiIoSetAddress) failed %d\n", ret_l);
#endif
        return bFALSE;
	}
	return bTRUE;
}

TBOOL RevPiDevice_writeNextConfigurationRight(void)
{
    if (RevPiDevice_writeNextConfiguration(RevPiScan.i8uAddressRight, &RevPiScan.dev[RevPiScan.i8uDeviceCount].sId))
	{
        RevPiScan.dev[RevPiScan.i8uDeviceCount].i8uAddress = RevPiScan.i8uAddressRight;
        if (RevPiScan.i8uDeviceCount == 0)
        {
            RevPiScan.dev[RevPiScan.i8uDeviceCount].i16uInputOffset = 0;
            RevPiScan.dev[RevPiScan.i8uDeviceCount].i16uOutputOffset = RevPiScan.dev[RevPiScan.i8uDeviceCount].sId.i16uFBS_InputLength;
        }
        else
        {
            RevPiScan.dev[RevPiScan.i8uDeviceCount].i16uInputOffset  = RevPiScan.dev[RevPiScan.i8uDeviceCount-1].i16uOutputOffset
                                                                         + RevPiScan.dev[RevPiScan.i8uDeviceCount-1].sId.i16uFBS_OutputLength;
            RevPiScan.dev[RevPiScan.i8uDeviceCount].i16uOutputOffset = RevPiScan.dev[RevPiScan.i8uDeviceCount].i16uInputOffset
                                                                         + RevPiScan.dev[RevPiScan.i8uDeviceCount].sId.i16uFBS_InputLength;
        }
#ifdef DEBUG_DEVICE
        DF_PRINTK("found %d. device on right side. Moduletype %d. Designated address %d\n",
            RevPiScan.i8uDeviceCount+1, RevPiScan.dev[RevPiScan.i8uDeviceCount].sId.i16uModulType, RevPiScan.i8uAddressRight);
        DF_PRINTK("input offset  %5d  len %3d\n",
                  RevPiScan.dev[RevPiScan.i8uDeviceCount].i16uInputOffset,
                  RevPiScan.dev[RevPiScan.i8uDeviceCount].sId.i16uFBS_InputLength);
        DF_PRINTK("output offset %5d  len %3d\n",
                  RevPiScan.dev[RevPiScan.i8uDeviceCount].i16uOutputOffset,
                  RevPiScan.dev[RevPiScan.i8uDeviceCount].sId.i16uFBS_OutputLength);
#endif
        RevPiScan.dev[RevPiScan.i8uDeviceCount].i8uActive = 1;
        RevPiScan.i8uDeviceCount++;
        RevPiScan.i8uAddressRight++;
		return bTRUE;
	}
	return bFALSE;
}

TBOOL RevPiDevice_writeNextConfigurationLeft(void)
{
    if (RevPiDevice_writeNextConfiguration(RevPiScan.i8uAddressLeft, &RevPiScan.dev[RevPiScan.i8uDeviceCount].sId))
	{
        RevPiScan.dev[RevPiScan.i8uDeviceCount].i8uAddress = RevPiScan.i8uAddressLeft;
        if (RevPiScan.i8uDeviceCount == 0)
        {
            RevPiScan.dev[RevPiScan.i8uDeviceCount].i16uInputOffset = 0;
            RevPiScan.dev[RevPiScan.i8uDeviceCount].i16uOutputOffset = RevPiScan.dev[RevPiScan.i8uDeviceCount].sId.i16uFBS_InputLength;
        }
        else
        {
            RevPiScan.dev[RevPiScan.i8uDeviceCount].i16uInputOffset  = RevPiScan.dev[RevPiScan.i8uDeviceCount-1].i16uOutputOffset
                                                                         + RevPiScan.dev[RevPiScan.i8uDeviceCount-1].sId.i16uFBS_OutputLength;
            RevPiScan.dev[RevPiScan.i8uDeviceCount].i16uOutputOffset = RevPiScan.dev[RevPiScan.i8uDeviceCount].i16uInputOffset
                                                                         + RevPiScan.dev[RevPiScan.i8uDeviceCount].sId.i16uFBS_InputLength;
        }
#ifdef DEBUG_DEVICE
        DF_PRINTK("found %d. device on left side. Moduletype %d. Designated address %d\n",
            RevPiScan.i8uDeviceCount+1,
            RevPiScan.dev[RevPiScan.i8uDeviceCount].sId.i16uModulType,
            RevPiScan.i8uAddressLeft);
        DF_PRINTK("input offset  %5d  len %3d\n",
                  RevPiScan.dev[RevPiScan.i8uDeviceCount].i16uInputOffset,
                  RevPiScan.dev[RevPiScan.i8uDeviceCount].sId.i16uFBS_InputLength);
        DF_PRINTK("output offset %5d  len %3d\n",
                  RevPiScan.dev[RevPiScan.i8uDeviceCount].i16uOutputOffset,
                  RevPiScan.dev[RevPiScan.i8uDeviceCount].sId.i16uFBS_OutputLength);
#endif
        RevPiScan.dev[RevPiScan.i8uDeviceCount].i8uActive = 1;
        RevPiScan.i8uDeviceCount++;
        RevPiScan.i8uAddressLeft--;
		return bTRUE;
	}
	return bFALSE;
}

void RevPiDevice_startDataexchange(void)
{
    INT32U ret_l = piIoComm_sendRS485Tel(eCmdPiIoStartDataExchange, MODGATE_RS485_BROADCAST_ADDR, NULL, 0, NULL, 0);
    msleep(90);    // wait a while
    if (ret_l)
    {
#ifdef DEBUG_DEVICE
        DF_PRINTK("piIoComm_sendRS485Tel(PiIoStartDataExchange) failed %d\n", ret_l);
#endif
    }
}

void RevPiDevice_stopDataexchange(void)
{
    INT32U ret_l = piIoComm_sendRS485Tel(eCmdPiIoStartDataExchange, MODGATE_RS485_BROADCAST_ADDR, NULL, 0, NULL, 0);
    msleep(90);    // wait a while
    if (ret_l)
    {
#ifdef DEBUG_DEVICE
        DF_PRINTK("piIoComm_sendRS485Tel(PiIoStartDataExchange) failed %d\n", ret_l);
#endif
    }
}

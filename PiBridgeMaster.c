//+=============================================================================================
//|
//!		\file PiBridgeSlave.c
//!		Statemachine for PiBridge master modules
//|
//+---------------------------------------------------------------------------------------------
//|
//|		File-ID:		$Id: PiBridgeMaster.c 11244 2016-12-07 09:11:48Z mduckeck $
//|		Location:		$URL: http://srv-kunbus03.de.pilz.local/feldbus/software/trunk/platform/ModGateCom/sw/PiBridgeMaster.c $
//|
//+=============================================================================================

#include <project.h>

#include <common_define.h>
#include <linux/module.h>    // included for all kernel modules
#include <linux/kernel.h>    // included for KERN_INFO
#include <linux/slab.h>    // included for KERN_INFO
#include <linux/delay.h>
#include <linux/gpio.h>

#include <kbUtilities.h>
#include <PiBridgeMaster.h>
#include <RevPiDevice.h>
#include <piControlMain.h>
#include <piControl.h>
#include <piIOComm.h>
#include <piDIOComm.h>

static TBOOL bEntering_s = bTRUE;
static EPiBridgeMasterStatus eRunStatus_s = enPiBridgeMasterStatus_Init;
static enPiBridgeState eBridgeStateLast_s = piBridgeStop;
static INT32U i32uSerAddress, i32uSerialNum;

void PiBridgeMaster_Stop(void)
{
    rt_mutex_lock(&piDev_g.lockBridgeState);
    piDev_g.eBridgeState = piBridgeStop;
    rt_mutex_unlock(&piDev_g.lockBridgeState);
}


void PiBridgeMaster_Reset(void)
{
    rt_mutex_lock(&piDev_g.lockBridgeState);
    piDev_g.eBridgeState = piBridgeInit;
    eRunStatus_s = enPiBridgeMasterStatus_Init;
    RevPiScan.i8uStatus = 0;
    //piDev_g.ai8uPI[RevPiScan.dev[0].i16uInputOffset] = 0;
    bEntering_s = bTRUE;
    RevPiDevice_init();
    rt_mutex_unlock(&piDev_g.lockBridgeState);
}


int PiBridgeMaster_Adjust(void)
{
    int i, j, done;
    int result = 0, found;
    uint8_t *state;

    if (piDev_g.devs == NULL || piDev_g.ent == NULL)
    {
        // config file could not be read, do nothing
        return 0;
    }

    state = kcalloc(piDev_g.devs->i16uNumDevices, sizeof(uint8_t), GFP_KERNEL);

    // Schleife über alle Module die automatisch erkannt wurden
    for (j=0; j<RevPiScan.i8uDeviceCount; j++)
    {
        // Suche diese Module in der Konfigurationsdatei
        for (i=0, found=0, done = 0; found == 0 && i<piDev_g.devs->i16uNumDevices && !done; i++)
        {
            // Grundvoraussetzung ist, dass die Adresse gleich ist.
            if (RevPiScan.dev[j].i8uAddress == piDev_g.devs->dev[i].i8uAddress)
            {
                // Außerdem muss ModuleType, InputLength und OutputLength gleich sein.
                if (RevPiScan.dev[j].sId.i16uModulType != piDev_g.devs->dev[i].i16uModuleType)
                {
                    DF_PRINTK("## address %d: incorrect module type %d != %d\n", RevPiScan.dev[j].i8uAddress, RevPiScan.dev[j].sId.i16uModulType, piDev_g.devs->dev[i].i16uModuleType);
                    result = PICONTROL_CONFIG_ERROR_WRONG_MODULE_TYPE;
                    RevPiScan.i8uStatus |= PICONTROL_STATUS_SIZE_MISMATCH;
                    //piDev_g.ai8uPI[RevPiScan.dev[0].i16uInputOffset] |= PICONTROL_STATUS_SIZE_MISMATCH;
                    done = 1;
                    break;
                }
                if (RevPiScan.dev[j].sId.i16uFBS_InputLength != piDev_g.devs->dev[i].i16uInputLength)
                {
                    DF_PRINTK("## address %d: incorrect input length %d != %d\n", RevPiScan.dev[j].i8uAddress, RevPiScan.dev[j].sId.i16uFBS_InputLength, piDev_g.devs->dev[i].i16uInputLength);
                    result = PICONTROL_CONFIG_ERROR_WRONG_INPUT_LENGTH;
                    RevPiScan.i8uStatus |= PICONTROL_STATUS_SIZE_MISMATCH;
                    //piDev_g.ai8uPI[RevPiScan.dev[0].i16uInputOffset] |= PICONTROL_STATUS_SIZE_MISMATCH;
                    done = 1;
                    break;
                }
                if (RevPiScan.dev[j].sId.i16uFBS_OutputLength != piDev_g.devs->dev[i].i16uOutputLength)
                {
                    DF_PRINTK("## address %d: incorrect output length %d != %d\n", RevPiScan.dev[j].i8uAddress, RevPiScan.dev[j].sId.i16uFBS_OutputLength, piDev_g.devs->dev[i].i16uOutputLength);
                    result = PICONTROL_CONFIG_ERROR_WRONG_OUTPUT_LENGTH;
                    RevPiScan.i8uStatus |= PICONTROL_STATUS_SIZE_MISMATCH;
                    //piDev_g.ai8uPI[RevPiScan.dev[0].i16uInputOffset] |= PICONTROL_STATUS_SIZE_MISMATCH;
                    done = 1;
                    break;
                }

                // we found the device in the configuration file
                // -> adjust offsets
#ifdef DEBUG_MASTER_STATE
                DF_PRINTK("Adjust: base %d in %d out %d conf %d\n",
                          piDev_g.devs->dev[i].i16uBaseOffset,
                          piDev_g.devs->dev[i].i16uInputOffset,
                          piDev_g.devs->dev[i].i16uOutputOffset,
                          piDev_g.devs->dev[i].i16uConfigOffset);
#endif
                RevPiScan.dev[j].i16uInputOffset  = piDev_g.devs->dev[i].i16uInputOffset;
                RevPiScan.dev[j].i16uOutputOffset = piDev_g.devs->dev[i].i16uOutputOffset;
                RevPiScan.dev[j].i16uConfigOffset = piDev_g.devs->dev[i].i16uConfigOffset;
                RevPiScan.dev[j].i16uConfigLength = piDev_g.devs->dev[i].i16uConfigLength;
                if (j == 0)
                {
                    RevPiScan.pCoreData = (SRevPiCoreImage *)&piDev_g.ai8uPI[RevPiScan.dev[0].i16uInputOffset];
                }

                state[i] = 1;   // dieser Konfigeintrag wurde übernommen
                found = 1;      // innere For-Schrleife verlassen
            }
        }
        if (found == 0)
        {
            // Falls ein autom. erkanntes Modul in der Konfiguration nicht vorkommt, wird es deakiviert
            RevPiScan.dev[j].i8uActive = 0;
            RevPiScan.i8uStatus |= PICONTROL_STATUS_EXTRA_MODULE;
            //piDev_g.ai8uPI[RevPiScan.dev[0].i16uInputOffset] |= PICONTROL_STATUS_EXTRA_MODULE;
        }
    }

    // nun wird die Liste der automatisch erkannten Module um die ergänzt, die nur in der Konfiguration vorkommen.
    for (i=0; i<piDev_g.devs->i16uNumDevices; i++)
    {
        if (state[i] == 0)
        {
            RevPiScan.i8uStatus |= PICONTROL_STATUS_MISSING_MODULE;
            //piDev_g.ai8uPI[RevPiScan.dev[0].i16uInputOffset] |= PICONTROL_STATUS_MISSING_MODULE;

            j = RevPiScan.i8uDeviceCount;
            RevPiScan.dev[j].i8uAddress                = piDev_g.devs->dev[i].i8uAddress;
            RevPiScan.dev[j].i16uInputOffset           = piDev_g.devs->dev[i].i16uInputOffset;
            RevPiScan.dev[j].i16uOutputOffset          = piDev_g.devs->dev[i].i16uOutputOffset;
            RevPiScan.dev[j].i16uConfigOffset          = piDev_g.devs->dev[i].i16uConfigOffset;
            RevPiScan.dev[j].i16uConfigLength          = piDev_g.devs->dev[i].i16uConfigLength;
            RevPiScan.dev[j].sId.i32uSerialnumber      = piDev_g.devs->dev[i].i32uSerialnumber;
            RevPiScan.dev[j].sId.i16uModulType         = piDev_g.devs->dev[i].i16uModuleType | PICONTROL_USER_MODULE_TYPE;    // higher module type because it is a user module
            RevPiScan.dev[j].sId.i16uHW_Revision       = piDev_g.devs->dev[i].i16uHW_Revision;
            RevPiScan.dev[j].sId.i16uSW_Major          = piDev_g.devs->dev[i].i16uSW_Major;
            RevPiScan.dev[j].sId.i16uSW_Minor          = piDev_g.devs->dev[i].i16uSW_Minor;
            RevPiScan.dev[j].sId.i32uSVN_Revision      = piDev_g.devs->dev[i].i32uSVN_Revision;
            RevPiScan.dev[j].sId.i16uFBS_InputLength   = piDev_g.devs->dev[i].i16uInputLength;
            RevPiScan.dev[j].sId.i16uFBS_OutputLength  = piDev_g.devs->dev[i].i16uOutputLength;
            RevPiScan.dev[j].sId.i16uFeatureDescriptor = 0; // not used
            RevPiScan.i8uDeviceCount++;
        }
    }

    kfree(state);
    return result;
}


int PiBridgeMaster_Run(void)
{
	static kbUT_Timer tTimeoutTimer_s;
    static int error_cnt;
    INT8U led;
    static INT8U last_led;
    int ret = 0;

    rt_mutex_lock(&piDev_g.lockBridgeState);
    if (piDev_g.eBridgeState != piBridgeStop)
    {
        switch (eRunStatus_s)
        {
        case enPiBridgeMasterStatus_Init: // Do some initializations and go to next state
            if (bEntering_s)
            {
    #ifdef DEBUG_MASTER_STATE
                DF_PRINTK("Enter Init State\n");
    #endif
                bEntering_s = bFALSE;
                // configure PiBridge Sniff lines as input
                piIoComm_writeSniff1A(enGpioValue_Low, enGpioMode_Input);
                piIoComm_writeSniff1B(enGpioValue_Low, enGpioMode_Input);
                piIoComm_writeSniff2A(enGpioValue_Low, enGpioMode_Input);
                piIoComm_writeSniff2B(enGpioValue_Low, enGpioMode_Input);
            }
            eRunStatus_s = enPiBridgeMasterStatus_MasterIsPresentSignalling1;
            bEntering_s = bTRUE;
            break;
            // *****************************************************************************************

        case enPiBridgeMasterStatus_MasterIsPresentSignalling1:
            if (bEntering_s)
            {
    #ifdef DEBUG_MASTER_STATE
                DF_PRINTK("Enter PresentSignalling1 State\n");
    #endif
                bEntering_s = bFALSE;
                piIoComm_writeSniff2A(enGpioValue_High, enGpioMode_Output);
                piIoComm_writeSniff2B(enGpioValue_High, enGpioMode_Output);
#if 0
                kbUT_TimerStart(&tTimeoutTimer_s, 8);
            }
            if (kbUT_TimerExpired(&tTimeoutTimer_s))
            {
                eRunStatus_s = enPiBridgeMasterStatus_MasterIsPresentSignalling2;
                bEntering_s = bTRUE;
            }
            break;
            // *****************************************************************************************

        case enPiBridgeMasterStatus_MasterIsPresentSignalling2:
            if (bEntering_s)
            {
    #ifdef DEBUG_MASTER_STATE
                DF_PRINTK("Enter PresentSignalling2 State\n");
    #endif
                bEntering_s = bFALSE;
#else
                usleep_range(9000, 9000);
#endif
                piIoComm_writeSniff2A(enGpioValue_Low, enGpioMode_Input);
                piIoComm_writeSniff2B(enGpioValue_Low, enGpioMode_Input);
                kbUT_TimerStart(&tTimeoutTimer_s, 30);
            }
            if (kbUT_TimerExpired(&tTimeoutTimer_s))
            {
                eRunStatus_s = enPiBridgeMasterStatus_InitialSlaveDetectionRight;
                bEntering_s = bTRUE;
            }
            break;
            // *****************************************************************************************

        case enPiBridgeMasterStatus_InitialSlaveDetectionRight:
            if (bEntering_s)
            {
    #ifdef DEBUG_MASTER_STATE
                DF_PRINTK("Enter InitialSlaveDetectionRight State\n");
    #endif
                bEntering_s = bFALSE;
            }
            if (piIoComm_readSniff2B() == enGpioValue_High)
            {
                eRunStatus_s = enPiBridgeMasterStatus_ConfigRightStart;
                bEntering_s = bTRUE;
            }
            else
            {
                eRunStatus_s = enPiBridgeMasterStatus_InitialSlaveDetectionLeft;
                bEntering_s = bTRUE;
            }
            break;
            // *****************************************************************************************

        case enPiBridgeMasterStatus_ConfigRightStart:
            if (bEntering_s)
            {
    #ifdef DEBUG_MASTER_STATE
                DF_PRINTK("Enter ConfigRightStart State\n");
    #endif
                bEntering_s = bFALSE;
                piIoComm_writeSniff1B(enGpioValue_Low, enGpioMode_Output);
                kbUT_TimerStart(&tTimeoutTimer_s, 10);
            }
            if (kbUT_TimerExpired(&tTimeoutTimer_s))
            {
                eRunStatus_s = enPiBridgeMasterStatus_ConfigDialogueRight;
                bEntering_s = bTRUE;
            }
            break;
            // *****************************************************************************************

        case enPiBridgeMasterStatus_ConfigDialogueRight:
            if (bEntering_s)
            {
    #ifdef DEBUG_MASTER_STATE
                DF_PRINTK("Enter ConfigDialogueRight State\n");
    #endif
                error_cnt = 0;
                bEntering_s = bFALSE;
            }
            // Write configuration data to the currently selected slave
            if (RevPiDevice_writeNextConfigurationRight() == bFALSE)
            {
                error_cnt++;
                if (error_cnt > 5)
                {
                    // no more slaves on the right side, configure left slaves
                    eRunStatus_s = enPiBridgeMasterStatus_InitialSlaveDetectionLeft;
                    bEntering_s = bTRUE;
                }
            }
            else
            {
                eRunStatus_s = enPiBridgeMasterStatus_SlaveDetectionRight;
                bEntering_s = bTRUE;
            }
            break;
            // *****************************************************************************************

        case enPiBridgeMasterStatus_SlaveDetectionRight:
            if (bEntering_s)
            {
    #ifdef DEBUG_MASTER_STATE
                DF_PRINTK("Enter SlaveDetectionRight State\n");
    #endif
                bEntering_s = bFALSE;
            }
            if (piIoComm_readSniff2B() == enGpioValue_High)
            {
                // configure next right slave
                eRunStatus_s = enPiBridgeMasterStatus_ConfigDialogueRight;
                bEntering_s = bTRUE;
            }
            else
            {
                // no more slaves on the right side, configure left slaves
                eRunStatus_s = enPiBridgeMasterStatus_InitialSlaveDetectionLeft;
                bEntering_s = bTRUE;
            }
            break;
            // *****************************************************************************************

        case enPiBridgeMasterStatus_InitialSlaveDetectionLeft:
            if (bEntering_s)
            {
    #ifdef DEBUG_MASTER_STATE
                DF_PRINTK("Enter InitialSlaveDetectionLeft State\n");
    #endif
                bEntering_s = bFALSE;
                piIoComm_writeSniff1B(enGpioValue_Low, enGpioMode_Input);
            }
            if (piIoComm_readSniff2A() == enGpioValue_High)
            {
                // configure first left slave
                eRunStatus_s = enPiBridgeMasterStatus_ConfigLeftStart;
                bEntering_s = bTRUE;
            }
            else
            {
                // no slave on the left side
                eRunStatus_s = enPiBridgeMasterStatus_EndOfConfig;
                bEntering_s = bTRUE;
            }
            break;
            // *****************************************************************************************

        case enPiBridgeMasterStatus_ConfigLeftStart:
            if (bEntering_s)
            {
    #ifdef DEBUG_MASTER_STATE
                DF_PRINTK("Enter ConfigLeftStart State\n");
    #endif
                bEntering_s = bFALSE;
                piIoComm_writeSniff1A(enGpioValue_Low, enGpioMode_Output);
                kbUT_TimerStart(&tTimeoutTimer_s, 10);
            }
            if (kbUT_TimerExpired(&tTimeoutTimer_s))
            {
                eRunStatus_s = enPiBridgeMasterStatus_ConfigDialogueLeft;
                bEntering_s = bTRUE;
            }
            break;
            // *****************************************************************************************

        case enPiBridgeMasterStatus_ConfigDialogueLeft:
            if (bEntering_s)
            {
    #ifdef DEBUG_MASTER_STATE
                DF_PRINTK("Enter ConfigDialogueLeft State\n");
    #endif
                error_cnt = 0;
                bEntering_s = bFALSE;
            }
            // Write configuration data to the currently selected slave
            if (RevPiDevice_writeNextConfigurationLeft() == bFALSE)
            {
                error_cnt++;
                if (error_cnt > 5)
                {
                    // no more slaves on the right side, configure left slaves
                    eRunStatus_s = enPiBridgeMasterStatus_EndOfConfig;
                    bEntering_s = bTRUE;
                }
            }
            else
            {
                eRunStatus_s = enPiBridgeMasterStatus_SlaveDetectionLeft;
                bEntering_s = bTRUE;
            }
            break;
            // *****************************************************************************************

        case enPiBridgeMasterStatus_SlaveDetectionLeft:
            if (bEntering_s)
            {
    #ifdef DEBUG_MASTER_STATE
                DF_PRINTK("Enter SlaveDetectionLeft State\n");
    #endif
                bEntering_s = bFALSE;
            }
            if (piIoComm_readSniff2A() == enGpioValue_High)
            {
                // configure next left slave
                eRunStatus_s = enPiBridgeMasterStatus_ConfigDialogueLeft;
                bEntering_s = bTRUE;
            }
            else
            {
                // no more slaves on the left
                eRunStatus_s = enPiBridgeMasterStatus_EndOfConfig;
                bEntering_s = bTRUE;
            }
            break;
            // *****************************************************************************************

        case enPiBridgeMasterStatus_EndOfConfig:
            if (bEntering_s)
            {
    #ifdef DEBUG_MASTER_STATE
                int i;
                DF_PRINTK("Enter EndOfConfig State\n\n");
                for (i = 0; i < RevPiScan.i8uDeviceCount; i++)
                {
                    DF_PRINTK("Device %2d: Addr %d Type %x  Act %d  In %d Out %d\n",
                        i,
                        RevPiScan.dev[i].i8uAddress,
                        RevPiScan.dev[i].sId.i16uModulType,
                        RevPiScan.dev[i].i8uActive,
                        RevPiScan.dev[i].sId.i16uFBS_InputLength,
                        RevPiScan.dev[i].sId.i16uFBS_OutputLength);
                    DF_PRINTK("           input offset  %5d  len %3d\n",
                        RevPiScan.dev[i].i16uInputOffset,
                        RevPiScan.dev[i].sId.i16uFBS_InputLength);
                    DF_PRINTK("           output offset %5d  len %3d\n",
                        RevPiScan.dev[i].i16uOutputOffset,
                        RevPiScan.dev[i].sId.i16uFBS_OutputLength);
                    DF_PRINTK("           serial number %d\n",
                        RevPiScan.dev[i].sId.i32uSerialnumber);
                }

                DF_PRINTK("\n");
    #endif

                piIoComm_writeSniff1A(enGpioValue_Low, enGpioMode_Input);

                PiBridgeMaster_Adjust();

    #ifdef DEBUG_MASTER_STATE
                DF_PRINTK("After Adjustment\n");
                for (i = 0; i < RevPiScan.i8uDeviceCount; i++)
                {
                    DF_PRINTK("Device %2d: Addr %d Type %x  Act %d  In %d Out %d\n",
                        i,
                        RevPiScan.dev[i].i8uAddress,
                        RevPiScan.dev[i].sId.i16uModulType,
                        RevPiScan.dev[i].i8uActive,
                        RevPiScan.dev[i].sId.i16uFBS_InputLength,
                        RevPiScan.dev[i].sId.i16uFBS_OutputLength);
                    DF_PRINTK("           input offset  %5d  len %3d\n",
                        RevPiScan.dev[i].i16uInputOffset,
                        RevPiScan.dev[i].sId.i16uFBS_InputLength);
                    DF_PRINTK("           output offset %5d  len %3d\n",
                        RevPiScan.dev[i].i16uOutputOffset,
                        RevPiScan.dev[i].sId.i16uFBS_OutputLength);
                }
                DF_PRINTK("\n");
    #endif

                msleep(100);    // wait a while
                DF_PRINTK("start data exchange\n");
                RevPiDevice_startDataexchange();
                msleep(110);    // wait a while

                // send config messages
                for (i = 0; i < RevPiScan.i8uDeviceCount; i++)
                {
                    if (RevPiScan.dev[i].i8uActive)
                    {
                        switch (RevPiScan.dev[i].sId.i16uModulType)
                        {
                        case KUNBUS_FW_DESCR_TYP_PI_DIO_14:
                        case KUNBUS_FW_DESCR_TYP_PI_DI_16:
                        case KUNBUS_FW_DESCR_TYP_PI_DO_16:
                            ret = piDIOComm_Init(i);
#ifdef DEBUG_DEVICE_DIO
                            DF_PRINTK("piDIOComm_Init done %d\n", ret);
#endif
                            break;
                        }
                    }
                }
                bEntering_s = bFALSE;
                ret = 0;
            }

            if (RevPiDevice_run())
            {
                // an error occured, check error limits
                if (RevPiScan.pCoreData != NULL)
                {
                    if (RevPiScan.pCoreData->i16uRS485ErrorLimit2 > 0
                     && RevPiScan.pCoreData->i16uRS485ErrorLimit2 < RevPiScan.i16uErrorCnt)
                    {
                        DF_PRINTK("too many communication errors -> set BridgeState to stopped\n");
                        piDev_g.eBridgeState = piBridgeStop;
                    }
                    else if (RevPiScan.pCoreData->i16uRS485ErrorLimit1 > 0
                          && RevPiScan.pCoreData->i16uRS485ErrorLimit1 < RevPiScan.i16uErrorCnt)
                    {
                        // bad communication with inputs -> set inputs to default values
                        DF_PRINTK("too many communication errors -> set inputs to default\n");
                    }
                }

//                msleep(120);    // wait a while
//                DF_PRINTK("start data exchange\n");
//                RevPiDevice_startDataexchange();
//                msleep(130);    // wait a while

                //PiBridgeMaster_Reset();
//                piDev_g.eBridgeState = piBridgeInit;
//                eRunStatus_s = enPiBridgeMasterStatus_Init;
//                piDev_g.ai8uPI[RevPiScan.dev[0].i16uInputOffset] = 0;
//                bEntering_s = bTRUE;
//                RevPiDevice_init();
            }
            else
            {
                ret = 1;
            }
            if (RevPiScan.pCoreData != NULL)
            {
                RevPiScan.pCoreData->i16uRS485ErrorCnt = RevPiScan.i16uErrorCnt;
            }
            break;
            // *****************************************************************************************

        default:
            break;

        }

        if (ret && piDev_g.eBridgeState != piBridgeRun)
        {
            DF_PRINTK("set BridgeState to running\n");
            piDev_g.eBridgeState = piBridgeRun;
        }
    }
    else // piDev_g.eBridgeState == piBridgeStop
    {
        if (eRunStatus_s == enPiBridgeMasterStatus_EndOfConfig)
        {
            DF_PRINTK("stop data exchange\n");
            ret = piIoComm_gotoGateProtocol();
            DF_PRINTK("piIoComm_gotoGateProtocol returned %d\n", ret);
            eRunStatus_s = enPiBridgeMasterStatus_Init;
        }
        else if (eRunStatus_s == enPiBridgeMasterStatus_FWUMode)
        {
            if (bEntering_s)
            {
                ret = piIoComm_gotoFWUMode(i32uSerAddress);
                DF_PRINTK("piIoComm_gotoFWUMode returned %d\n", ret);

                if (i32uSerAddress >= 32)
                    i32uSerAddress = 2; // address must be 2 in the following calls
                else
                    i32uSerAddress = 1; // address must be 1 in the following calls

                ret = 0;    // ignore errors here
                bEntering_s = bFALSE;
            }
        }
        else if (eRunStatus_s == enPiBridgeMasterStatus_ProgramSerialNum)
        {
            if (bEntering_s)
            {
                ret = piIoComm_setSerNum(i32uSerAddress, i32uSerialNum);
                DF_PRINTK("piIoComm_setSerNum returned %d\n", ret);

                ret = 0;    // ignore errors here
                bEntering_s = bFALSE;
            }
        }
        else if (eRunStatus_s == enPiBridgeMasterStatus_FWUReset)
        {
            if (bEntering_s)
            {
                ret = piIoComm_fwuReset(i32uSerAddress);
                DF_PRINTK("piIoComm_fwuReset returned %d\n", ret);

                ret = 0;    // ignore errors here
                bEntering_s = bFALSE;
            }
        }
    }

    rt_mutex_unlock(&piDev_g.lockBridgeState);
    if (RevPiScan.pCoreData != NULL)
    {
        RevPiScan.pCoreData->i8uStatus = RevPiScan.i8uStatus;
    }

    if (eBridgeStateLast_s != piDev_g.eBridgeState)
    {
        if (piDev_g.eBridgeState == piBridgeRun)
        {
            RevPiScan.i8uStatus |= PICONTROL_STATUS_RUNNING;
            gpio_set_value_cansleep(GPIO_LED_PWRRED, 0);
        }
        else
        {
            RevPiScan.i8uStatus &= ~PICONTROL_STATUS_RUNNING;
            gpio_set_value_cansleep(GPIO_LED_PWRRED, 1);
        }
        eBridgeStateLast_s = piDev_g.eBridgeState;
    }

    // set LED
    if (RevPiScan.pCoreData != NULL)
    {
        led = RevPiScan.pCoreData->i8uLED;
        if (led != last_led)
        {
            //DF_PRINTK("%d: LED %02x\n", RevPiScan.dev[0].i16uOutputOffset, led);

            gpio_set_value_cansleep(GPIO_LED_AGRN, (led & PICONTROL_LED_A1_GREEN) ? 1 : 0);
            gpio_set_value_cansleep(GPIO_LED_ARED, (led & PICONTROL_LED_A1_RED)   ? 1 : 0);
            gpio_set_value_cansleep(GPIO_LED_BGRN, (led & PICONTROL_LED_A2_GREEN) ? 1 : 0);
            gpio_set_value_cansleep(GPIO_LED_BRED, (led & PICONTROL_LED_A2_RED)   ? 1 : 0);

            last_led = led;
        }
    }

    return ret;
}

int PiBridgeMaster_gotoFWUMode(INT32U address)
{
    if (piDev_g.eBridgeState == piBridgeStop)
    {
        i32uSerAddress = address;
        eRunStatus_s = enPiBridgeMasterStatus_FWUMode;
        bEntering_s = bTRUE;
    }
    return 0;
}

int PiBridgeMaster_setSerNum(INT32U serNum)
{
    if (piDev_g.eBridgeState == piBridgeStop)
    {
        i32uSerialNum = serNum;
        eRunStatus_s = enPiBridgeMasterStatus_ProgramSerialNum;
        bEntering_s = bTRUE;
    }
    return 0;
}

int PiBridgeMaster_fwuReset(void)
{
    if (piDev_g.eBridgeState == piBridgeStop)
    {
        eRunStatus_s = enPiBridgeMasterStatus_FWUReset;
        bEntering_s = bTRUE;
    }
    return 0;
}




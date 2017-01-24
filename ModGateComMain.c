//+=============================================================================================
//|
//!		\file ModGateComMain.c
//!		Communication layers of mGate
//|
//+---------------------------------------------------------------------------------------------
//|
//|		File-ID:		$Id: ModGateComMain.c 11087 2016-11-01 12:42:22Z mduckeck $
//|		Location:		$URL: http://srv-kunbus03.de.pilz.local/feldbus/software/trunk/platform/ModGateCom/sw/ModGateComMain.c $
//|		Company:		$Cpn: KUNBUS GmbH $
//|
//+=============================================================================================

#include <common_define.h>

#include <project.h>

#ifdef __KUNBUSPI_KERNEL__
#include <linux/kernel.h>
#include <linux/string.h>
#include <ModGateComMain.h>
#include <piControlMain.h>
#include <RevPiDevice.h>
#elif defined(__KUNBUSPI__)
#include <wiringPi.h>
#include <memory.h>
#else
#include <string.h>

#include <bsp\bspConfig.h>
#include <bsp\gpio\gpio.h>

#include <bsp\setjmp\BspSetJmp.h>
#include <bsp\led\led.h>
#include <bsp\flash\FlashDriver.h>
#include <bsp\timer\timer.h>

#include <kbEndianess.h>
#include <application.h>
#endif

#ifdef PRINT_MODGATE_COM_STATE
#if defined(__KUNBUSPI__)
#define DF_PRINTK(...)      printf(__VA_ARGS__)
#endif
#endif

#include <bsp/bspError.h>
#include <bsp/mGateDecode/mGateDecode.h>
#include <kbUtilities.h>
#include <kbAlloc.h>

#include <bsp/spi/spi.h>
#include <bsp/ksz8851/ksz8851.h>

#include <ModGateComMain.h>
#include <ModGateComError.h>
#include <ModGateRS485.h>

#include <bsp/systick/systick.h>

void *LL_pvPacketHeapHandle_g;
static MODGATECOM_Packet asuPacketMem_s[7]; // 6 packets plus malloc-overhead

#define T1_ACK_TIMEOUT      20
#define T2_ACK_TIMEOUT      40
#define T2_ACK_RETRY         2

#define MG_AL_MAX_RETRY      5
#define MG_AL_TIMEOUT       80

MODGATECOM_IDResp MODGATE_OwnID_g;     //!< ID-Data of this mGate
static INT8U    i8uOwnFieldbusState_s;      //!< Fieldbus State of this mGate


//-----------------------------------------------------------------------------

#define ID_REQUEST_TIME      100 // time in ms for ID-Request
#define LED_GREEN_TIME      2000 // time in ms for green led at start in ID-Request status
#define PD_TIMEOUT_MAX         5

typedef enum
{
    MODGATECOM_enPLR_INIT,
    MODGATECOM_enPLR_GREEN_PHASE,
    MODGATECOM_enPLR_RUN
}  MODGATECOM_EPowerLedRun;


sALData AL_Data_s[MODGATECOM_MAX_MODULES];

static MODGATECOM_EPowerLedRun MODGATECOM_enLedRunState_s;
static kbUT_Timer  MODGATECOM_tGreenTimer_s;

//**********************************************************************************************
// Link Layer
//**********************************************************************************************

INT32U MG_LL_init (LLHandle llHdl)
{
    // Ethernet destination
    llHdl->Header.i8uDestination[0] = 0xFF;
    llHdl->Header.i8uDestination[1] = 0xFF;
    llHdl->Header.i8uDestination[2] = 0xFF;
    llHdl->Header.i8uDestination[3] = 0xFF;
    llHdl->Header.i8uDestination[4] = 0xFF;
    llHdl->Header.i8uDestination[5] = 0xFF;

    // Ethernet source
    llHdl->Header.i8uSource[0] = 0xC8;
    llHdl->Header.i8uSource[1] = 0x3E;
    llHdl->Header.i8uSource[2] = 0xA7;
    llHdl->Header.i8uSource[3] = 0x00;
#ifdef __KUNBUSPI_KERNEL__
    llHdl->Header.i8uSource[4] = KUNBUS_FW_DESCR_TYP_PI_CORE;
#elif defined(__KUNBUSPI__)
    llHdl->Header.i8uSource[4] = KUNBUS_FW_DESCR_TYP_PI_CORE;
#else
    llHdl->Header.i8uSource[4] = ctKunbusFirmwareDescription_g.i16uDeviceType;
#endif

    if (BSP_getMgateDecode() == MGATE_MODUL_LEFT)
        llHdl->Header.i8uSource[5] = 0x01;
    else
        llHdl->Header.i8uSource[5] = 0x02;

    llHdl->Header.i16uType = 0x9C41;    //Ethernet type
    llHdl->Header.i8uACK = 0;
    llHdl->Header.i8uCounter = 1;

    llHdl->state = 0;
    llHdl->tick = 0;
    llHdl->timed_out = bFALSE;
    llHdl->retry = 0;
    
    llHdl->pLastData = 0;

    if (LL_pvPacketHeapHandle_g == 0)
    {
        LL_pvPacketHeapHandle_g = kbUT_initHeap((INT8U*)asuPacketMem_s, sizeof(asuPacketMem_s));
    }

    if (!ksz8851Init())
    {
        MODGATECOM_error(BSPE_KSZ8851_RESET, bTRUE, 0);
        // returns never
    }

    return MODGATECOM_NO_ERROR;
}

TBOOL  MG_LL_pending (LLHandle llHdl)
{
    if (llHdl->state == 1)
    {
        return bTRUE;
    }
    else
    {
        return bFALSE;
    }
}

void MG_LL_abort (LLHandle llHdl)
{
    if (llHdl->pLastData)
    {
        kbUT_free(llHdl->pLastData);
        llHdl->pLastData = 0;
    }
    llHdl->state = 0;
}

// return TRUE once, if a timeout occurred
TBOOL MG_LL_timed_out (LLHandle llHdl)
{
    TBOOL r = llHdl->timed_out;
    llHdl->timed_out = bFALSE;
    return r;
}


INT32U MG_LL_send (LLHandle llHdl, MODGATECOM_Packet *pData_p)
{
    if (llHdl->state == 0 || llHdl->state == 2)
    {
        if (!ksz8851Link())
        {
            kbUT_free(pData_p);
            MG_LL_abort(llHdl);
            return MODGATECOM_ERROR_NO_LINK;
        }

        memcpy(pData_p, &llHdl->Header, sizeof(llHdl->Header));
        if (ksz8851PacketSend((INT8U *)pData_p, MODGATE_TL_HEADER_LEN + pData_p->strTransportLayer.i16uDataLength))
        {   // success
            //DF_PRINTK("sendPacket done\n");
            llHdl->state = 1;     // wait for ack
            llHdl->retry = 0;

            llHdl->Header.i8uCounter++;
            if (llHdl->Header.i8uCounter == 0)
                llHdl->Header.i8uCounter = 1;

            if (llHdl->pLastData)
            {
                kbUT_free(llHdl->pLastData);
            }
            llHdl->pLastData = pData_p;

            llHdl->tick = kbUT_getCurrentMs();
            return MODGATECOM_NO_ERROR;
        }
        else
        {   // send failed
            //DF_PRINTK("sendPacket failed\n");
            MODGATECOM_error(MODGATECOM_ERROR_SEND_FAILED, bFALSE, 0);
            llHdl->state = 0;
            kbUT_free(pData_p);
            return MODGATECOM_ERROR_SEND_FAILED;
        }
    }
    else
    {
        kbUT_free(pData_p);
        return MODGATECOM_ERROR_SEND_UNACKED;
    }
}

MODGATECOM_Packet *MG_LL_recv(LLHandle llHdl, INT32U *pi32uStatus_p, INT16U *pi16uLen_p)
{
    INT16U i16uPackelLen_l = MODGATE_LL_MAX_LEN;

    MODGATECOM_Packet *pPacket_l = (MODGATECOM_Packet *)kbUT_malloc(LL_pvPacketHeapHandle_g, 10, i16uPackelLen_l);
    if (pPacket_l == 0)
    {
        MODGATECOM_error(MODGATECOM_ERROR_OUT_OF_MEMORY_1, bTRUE, 0);
        *pi32uStatus_p = MODGATECOM_ERROR_OUT_OF_MEMORY_1;
        return 0;
    }

    (void)pi16uLen_p;    // ignore parameter

    if (!ksz8851Link())
    {
        kbUT_free(pPacket_l);
        MG_LL_abort(llHdl);
        *pi32uStatus_p = MODGATECOM_ERROR_NO_LINK;
        return 0;
    }

    if (!ksz8851PacketRead((INT8U *)pPacket_l, &i16uPackelLen_l))
    {
        kbUT_free(pPacket_l);
        // no packet received -> check for timeout now
        if (llHdl->state == 1)
        {
            if ((kbUT_getCurrentMs() - llHdl->tick) >= T2_ACK_TIMEOUT)
            {
                // ack missing -> send packet again up to T2_ACK_RETRY times
                if (llHdl->retry > T2_ACK_RETRY)
                {
                    // ignore missing ACK an go back to idle state
                    MG_LL_abort(llHdl);
                    llHdl->timed_out = bTRUE;
                }
                else if (ksz8851PacketSend((INT8U *)llHdl->pLastData, MODGATE_TL_HEADER_LEN + llHdl->pLastData->strTransportLayer.i16uDataLength))
                {   // success
                    llHdl->tick = kbUT_getCurrentMs();
                    llHdl->retry++;
                }
                else
                {
                    MODGATECOM_error(MODGATECOM_ERROR_SEND_FAILED, bTRUE, 0);
                    *pi32uStatus_p = MODGATECOM_ERROR_SEND_FAILED;
                    return 0;
                }
            }
        }
        else if (llHdl->state == 2)
        {
            if ((kbUT_getCurrentMs() - llHdl->tick) >= T1_ACK_TIMEOUT)
            {
                MODGATECOM_Packet *pSendPacket_l;

                // send an empty ack
                if (llHdl->pLastData)
                {
                    kbUT_free(llHdl->pLastData);
                    llHdl->pLastData = 0;
                }

                pSendPacket_l = (MODGATECOM_Packet *)kbUT_malloc(LL_pvPacketHeapHandle_g, 1, MODGATE_TL_HEADER_LEN);
                if (pSendPacket_l == 0)
                {
                    MODGATECOM_error(MODGATECOM_ERROR_OUT_OF_MEMORY_2, bTRUE, 0);
                    *pi32uStatus_p = MODGATECOM_ERROR_OUT_OF_MEMORY_2;
                    return 0;
                }

                memcpy(&pSendPacket_l->strLinkLayer, &llHdl->Header, MODGATE_LL_HEADER_LEN);
                pSendPacket_l->strLinkLayer.i8uCounter = 0;   // for an ack is no additional ack necessary
                pSendPacket_l->strTransportLayer.i16uCmd = 0;
                pSendPacket_l->strTransportLayer.i16uDataLength = 0;
                pSendPacket_l->strTransportLayer.i32uError = MODGATECOM_NO_ERROR;

                ksz8851PacketSend((INT8U *)pSendPacket_l, MODGATE_TL_HEADER_LEN);
                kbUT_free(pSendPacket_l);
                llHdl->state = 0;
            }
        }
        *pi32uStatus_p = MODGATECOM_ERROR_NO_PACKET;
        return 0;
    }
    else
    {
        if (llHdl->state == 2)
        {
            // we should not receive packets in state 2 because the last packet was not ACKed yet
            //MODGATECOM_error(MODGATECOM_ERROR_ACK_MISSING, bFALSE, 0);
        }
        if (pPacket_l->strLinkLayer.i8uCounter == 0)
        {
            // empty ack
            kbUT_free(pPacket_l);
            llHdl->state = 0;
            return 0;
        }
        else if (i16uPackelLen_l < MODGATE_TL_HEADER_LEN + pPacket_l->strTransportLayer.i16uDataLength)
        {
            // incomplete packet -> ignore
            kbUT_free(pPacket_l);
            return 0;
        }
        else
        {
            // store number for next ACK
            llHdl->Header.i8uACK = pPacket_l->strLinkLayer.i8uCounter;

            llHdl->tick = kbUT_getCurrentMs();    // start timer for T1
            llHdl->state = 2;
            return pPacket_l;
        }
    }
}

//**********************************************************************************************
// Transport/Application Layer
//**********************************************************************************************
//#define USE_TEST_CODE // test code
#ifdef USE_TEST_CODE // test code
INT8U data[530];
int l=60, j;
#endif

INT32U MODGATECOM_init ( INT8U *pi8uInData_p,  INT16U i16uInDataLen_p,
                    INT8U *pi8uOutData_p, INT16U i16uOutDataLen_p)
{
    INT32U i32uRet_l = MODGATECOM_NO_ERROR;
    int inst;

    // ID response
#ifdef __KUNBUSPI_KERNEL__
    MODGATE_OwnID_g.i32uSerialnumber =      0xffffffff;
    MODGATE_OwnID_g.i16uModulType =         KUNBUS_FW_DESCR_TYP_PI_CORE;
    MODGATE_OwnID_g.i16uHW_Revision =       0;
    MODGATE_OwnID_g.i16uSW_Major =          0;
    MODGATE_OwnID_g.i16uSW_Minor =          0;
    MODGATE_OwnID_g.i32uSVN_Revision =      0;
#elif defined(__KUNBUSPI__)
    MODGATE_OwnID_g.i32uSerialnumber =      0xffffffff;
    MODGATE_OwnID_g.i16uModulType =         KUNBUS_FW_DESCR_TYP_PI_CORE;
    MODGATE_OwnID_g.i16uHW_Revision =       0;
    MODGATE_OwnID_g.i16uSW_Major =          0;
    MODGATE_OwnID_g.i16uSW_Minor =          0;
    MODGATE_OwnID_g.i32uSVN_Revision =      0;
#else
    MODGATE_OwnID_g.i32uSerialnumber =      ctKunbusFirmwareDescription_g.i32uSerialNumber;
    MODGATE_OwnID_g.i16uModulType =         ctKunbusFirmwareDescription_g.i16uDeviceType;
    MODGATE_OwnID_g.i16uHW_Revision =       ctKunbusFirmwareDescription_g.i16uHwRevision;
    MODGATE_OwnID_g.i16uSW_Major =          ctKunbusApplDescription_g.i8uSwMajor;
    MODGATE_OwnID_g.i16uSW_Minor =          ctKunbusApplDescription_g.i16uSwMinor;
    MODGATE_OwnID_g.i32uSVN_Revision =      ctKunbusApplDescription_g.i32uSvnRevision;
#endif

    MODGATE_OwnID_g.i16uFBS_InputLength =   i16uInDataLen_p;
    MODGATE_OwnID_g.i16uFBS_OutputLength =  i16uOutDataLen_p;
    MODGATE_OwnID_g.i16uFeatureDescriptor = MODGATE_feature_IODataExchange;

    ksz8851HardwareReset();
    
    for (inst = 0; inst < MODGATECOM_MAX_MODULES; inst++)
    {
#if MODGATECOM_MAX_MODULES > 1
#ifdef __KUNBUSPI_KERNEL__
        spi_select_chip(inst);
#elif defined(__KUNBUSPI__)
        spi_select_chip(inst);
#else
#error SPI chip select must be implemented
#endif
#endif
        // initialize fieldbus states
        AL_Data_s[inst].i8uState = MODGATE_ST_HW_CHECK;          //!< modular Gateway state machine state
        i8uOwnFieldbusState_s = FBSTATE_OFFLINE;
        AL_Data_s[inst].i8uOtherFieldbusState = FBSTATE_OFFLINE;
        MODGATECOM_enLedRunState_s = MODGATECOM_enPLR_INIT;

        AL_Data_s[inst].enLedStateAct = MODGATECOM_enPLS_INIT;
        AL_Data_s[inst].enLedStateOld = MODGATECOM_enPLS_INIT;

#ifdef __KUNBUSPI_KERNEL__
#elif defined(__KUNBUSPI__)
#else
#if !defined PI_SLAVE_MODULE
        if (ModGateRs485Init() != MODGATECOM_NO_ERROR)
        {
            i32uRet_l = MODGATECOM_ERROR_RS485_INIT;
            goto laError;
        }
#endif
#endif


        i32uRet_l = MG_LL_init(&AL_Data_s[inst].llParas);
        if (i32uRet_l != MODGATECOM_NO_ERROR)
        {
            goto laError;
        }
    
        AL_Data_s[inst].pi8uInData            = pi8uInData_p + (inst * i16uInDataLen_p);
        AL_Data_s[inst].i16uInDataLen         = i16uInDataLen_p;
        AL_Data_s[inst].i16uInDataLenActive   = 0;      // unknown yet
        AL_Data_s[inst].pi8uOutData           = pi8uOutData_p + (inst * i16uOutDataLen_p);
        AL_Data_s[inst].i16uOutDataLen        = i16uOutDataLen_p;
        AL_Data_s[inst].i16uOutDataLenActive  = 0;      // unknown yet

        //init buffer
        memset(AL_Data_s[inst].pi8uInData, 0, i16uInDataLen_p);
        memset(AL_Data_s[inst].pi8uOutData, 0, i16uOutDataLen_p);
    }

    return MODGATECOM_NO_ERROR;

laError:
    MODGATECOM_enLedRunState_s = MODGATECOM_enPLS_ERROR;
    MODGATECOM_managePowerLedRun ();
    return i32uRet_l;
}


void   MODGATECOM_run (void)
{
    static TBOOL bSend=bTRUE;
    INT32U i32uStatus_l;
    INT16U i16uLen_l;
    MODGATECOM_Packet *pPacket_l = 0;
    INT32U oldState, newState;
    int inst;

#ifdef __KUNBUSPI_KERNEL__
#elif defined(__KUNBUSPI__)
#else
#if !defined PI_SLAVE_MODULE
    ModGateRs485Run();
#endif
#endif

    oldState = 0;
    for (inst = 0; inst < MODGATECOM_MAX_MODULES; inst++)
    {
#ifdef __KUNBUSPI_KERNEL__
        spi_select_chip(inst);
#elif defined(__KUNBUSPI__)
        spi_select_chip(inst);
#else
#endif
        oldState <<= 8;
        if (AL_Data_s[inst].i8uState == MODGATE_ST_ID_RESP)
        {
            oldState += MODGATE_ST_ID_REQ;
        }
        else
        {
            oldState += AL_Data_s[inst].i8uState;
        }

        // try to receive a packet first
        pPacket_l = MG_LL_recv(&AL_Data_s[inst].llParas, &i32uStatus_l, &i16uLen_l);
#ifdef __KUNBUSPI_KERNEL__
        if (pPacket_l && piDev_g.eBridgeState != piBridgeRun)
        {
            // while the modules are not enumerated, all packets are ignored
            kbUT_free(pPacket_l);
            pPacket_l = 0;
        }
#endif
        if (pPacket_l)
        {
            switch (pPacket_l->strTransportLayer.i16uCmd)
            {
            case MODGATE_AL_CMD_ID_Req:
                // send response in all states
                MODGATECOM_send_ID_Resp(&AL_Data_s[inst]);
                break;

            case MODGATE_AL_CMD_ID_Resp:
                if (AL_Data_s[inst].i8uState == MODGATE_ST_ID_RESP)
                {
                    // if we are waiting for the response, precess it and go to run state
                    if (MODGATECOM_recv_Id_Resp(&AL_Data_s[inst], pPacket_l))
                    {
                        AL_Data_s[inst].i8uState = MODGATE_ST_RUN_NO_DATA;
                        AL_Data_s[inst].enLedStateAct = MODGATECOM_enPLS_DATA_MISSING;
                        bSend = bTRUE;
                    }
                    pPacket_l = 0;
                }
                // otherwise ignore message
                break;

            case MODGATE_AL_CMD_cyclicPD:
                if (AL_Data_s[inst].i8uState == MODGATE_ST_RUN_NO_DATA
                 || AL_Data_s[inst].i8uState == MODGATE_ST_RUN)
                {
                    MODGATECOM_recv_cyclicPD(&AL_Data_s[inst], pPacket_l);
                    pPacket_l = 0;
                    AL_Data_s[inst].enLedStateAct = MODGATECOM_enPLS_RUN;
                    AL_Data_s[inst].i8uState = MODGATE_ST_RUN;
                    bSend = bTRUE;
     #if 0 //MD Testcode:
        // this code was implemented for the EtherNet/IP Module because the LED 1 is not used there
        // the LED 1 will become red, when the time between two packets with cyclic process data
        // is much higher than the average of the last 10 packets
        // the LED is turned on for 2 seconds
        // if more packets a missing the power LED will start blinking
                    static INT32U tick, cnt, diff, diffAvg;
                    if (cnt == 0)
                    {
                        BSP_count_up_timer_init(1);
                        BSP_count_up_timer_start();
                        cnt++;
                    }
                    else
                    {
                        diff = BSP_count_up_timer_get();
                        BSP_count_up_timer_start();
                        if (cnt > 10)
                        {
                            diffAvg = ((diffAvg * 9) / 10) + diff;
                            if (diff * 6 > diffAvg)
                            {
                                LED_setLed(1, LED_ST_RED_ON);
                                cnt = 1;
                                diffAvg = 0;
                                tick = kbUT_getCurrentMs();
                            }
                            else
                            {
                                if ((kbUT_getCurrentMs() - tick) > 2000)
                                {
                                    LED_setLed(1, LED_ST_RED_OFF);
                                }
                            }
                        }
                        else
                        {
                            diffAvg += diff;
                            cnt++;
                        }
                    }
    #endif
                }
                break;

            case MODGATE_AL_CMD_ST_Req:     // not used
            case MODGATE_AL_CMD_ST_Resp:    // not used
            case MODGATE_AL_CMD_updatePD:   // not used
            default:
                // ignore packet
                kbUT_free(pPacket_l);
                pPacket_l = 0;
                return;
            }
            if (pPacket_l != 0)
            {
                // ignore packet
                kbUT_free(pPacket_l);
                pPacket_l = 0;
            }
        }
        else
        {
            if (i32uStatus_l == MODGATECOM_ERROR_NO_LINK)
            {
                AL_Data_s[inst].i8uState = MODGATE_ST_LINK_CHECK;
            }
        }

        // check state
#ifdef __KUNBUSPI_KERNEL__
        if (piDev_g.eBridgeState == piBridgeRun)
#endif
        {
            switch (AL_Data_s[inst].i8uState)
            {
            case MODGATE_ST_HW_CHECK:
                {
                    AL_Data_s[inst].enLedStateAct = MODGATECOM_enPLS_LINK_MISSING;
                    // nothing to do
                    AL_Data_s[inst].i8uState = MODGATE_ST_LINK_CHECK;
                }    break;

            case MODGATE_ST_LINK_CHECK:
                {
                    // set input data to 0
                    memset(AL_Data_s[inst].pi8uInData, 0, AL_Data_s[inst].i16uInDataLenActive);

                    if (ksz8851Link())
                    {
                        AL_Data_s[inst].enLedStateAct = MODGATECOM_enPLS_DATA_MISSING;
                        AL_Data_s[inst].i8uState = MODGATE_ST_ID_REQ;
                    }
                    else
                    {
                        AL_Data_s[inst].enLedStateAct = MODGATECOM_enPLS_LINK_MISSING;
                    }
                }   break;

            case MODGATE_ST_ID_REQ:
                {
                    if (MODGATECOM_send_ID_Req(&AL_Data_s[inst]) == MODGATECOM_NO_ERROR)
                    {
                        AL_Data_s[inst].i8uState = MODGATE_ST_ID_RESP;
                        kbUT_TimerStart(&AL_Data_s[inst].AL_Timeout, MG_AL_TIMEOUT);
                    }
                }   break;

            case MODGATE_ST_ID_RESP:
                {
                    if (kbUT_TimerExpired(&AL_Data_s[inst].AL_Timeout))
                    {
                        AL_Data_s[inst].i8uState = MODGATE_ST_ID_REQ;
                        AL_Data_s[inst].enLedStateAct = MODGATECOM_enPLS_DATA_MISSING;
                    }
                }   break;

            case MODGATE_ST_RUN_NO_DATA:
                // set input data to 0
                memset(AL_Data_s[inst].pi8uInData, 0, AL_Data_s[inst].i16uInDataLenActive);
            case MODGATE_ST_RUN:
                if (bSend)
                {
                    if (!MG_LL_pending(&AL_Data_s[inst].llParas))
                    {
                        INT32U ret = MODGATECOM_send_cyclicPD(&AL_Data_s[inst]);
                        if (ret == MODGATECOM_NO_ERROR)
                        {
                            kbUT_TimerStart(&AL_Data_s[inst].AL_Timeout, MG_AL_TIMEOUT);
                            bSend = bFALSE;
                        }
                        else if (ret == MODGATECOM_ERROR_NO_LINK)
                        {
                            AL_Data_s[inst].i8uState = MODGATE_ST_LINK_CHECK;
                            AL_Data_s[inst].enLedStateAct = MODGATECOM_enPLS_LINK_MISSING;
                        }
                    }
                    else
                    {
                        if (MG_LL_timed_out(&AL_Data_s[inst].llParas))
                        {
                            AL_Data_s[inst].i8uState = MODGATE_ST_ID_REQ;
                            AL_Data_s[inst].enLedStateAct = MODGATECOM_enPLS_DATA_MISSING;
                        }
                    }
                }
                else
                {
                    if (kbUT_TimerExpired(&AL_Data_s[inst].AL_Timeout))
                    {
                        AL_Data_s[inst].i8uState = MODGATE_ST_ID_REQ;
                        AL_Data_s[inst].enLedStateAct = MODGATECOM_enPLS_DATA_MISSING;
                    }
                }
                break;

            default:
                break;
            }
        }
    }

    newState = 0;
    if (AL_Data_s[0].i8uState == MODGATE_ST_ID_RESP)
    {
        newState += MODGATE_ST_ID_REQ;
    }
    else
    {
        newState += AL_Data_s[0].i8uState;
    }

#ifdef PRINT_MODGATE_COM_STATE
#if MODGATECOM_MAX_MODULES == 1
        if (oldState != newState)
        {
            switch (AL_Data_s[0].i8uState)
            {
            case MODGATE_ST_HW_CHECK:
                DF_PRINTK("Module %d: HW check\n", 0);
                break;
            case MODGATE_ST_LINK_CHECK:
                DF_PRINTK("Module %d: Link check\n", 0);
                break;
            case MODGATE_ST_ID_REQ:     // id request und response werden für die Ausgabe nicht unterschieden
            case MODGATE_ST_ID_RESP:
                DF_PRINTK("Module %d: id request\n", 0);
                break;
            case MODGATE_ST_RUN_NO_DATA:
                DF_PRINTK("Module %d: run no data\n", 0);
                break;
            case MODGATE_ST_RUN:
                DF_PRINTK("Module %d: run\n", 0);
                break;
            }
        }
#else
        newState <<= 8;
        if (AL_Data_s[1].i8uState == MODGATE_ST_ID_RESP)
        {
            newState += MODGATE_ST_ID_REQ;
        }
        else
        {
            newState += AL_Data_s[1].i8uState;
        }

        if (oldState != newState)
        {
            INT8U set, clr;
            set = 0;
            clr = 0;

            switch (AL_Data_s[0].i8uState)
            {
            case MODGATE_ST_HW_CHECK:
                DF_PRINTK("Module %d: HW check     ", 0);
                clr |= PICONTROL_STATUS_RIGHT_GATEWAY;
                break;
            case MODGATE_ST_LINK_CHECK:
                DF_PRINTK("Module %d: Link check   ", 0);
                clr |= PICONTROL_STATUS_RIGHT_GATEWAY;
                break;
            case MODGATE_ST_ID_REQ:     // id request und response werden für die Ausgabe nicht unterschieden
            case MODGATE_ST_ID_RESP:
                DF_PRINTK("Module %d: id request   ", 0);
                clr |= PICONTROL_STATUS_RIGHT_GATEWAY;
                break;
            case MODGATE_ST_RUN_NO_DATA:
                DF_PRINTK("Module %d: run no data  ", 0);
                clr |= PICONTROL_STATUS_RIGHT_GATEWAY;
                break;
            case MODGATE_ST_RUN:
                DF_PRINTK("Module %d: run          ", 0);
                set |= PICONTROL_STATUS_RIGHT_GATEWAY;
                break;
            }

            switch (AL_Data_s[1].i8uState)
            {
            case MODGATE_ST_HW_CHECK:
                DF_PRINTK("Module %d: HW check\n", 1);
                clr |= PICONTROL_STATUS_LEFT_GATEWAY;
                break;
            case MODGATE_ST_LINK_CHECK:
                DF_PRINTK("Module %d: Link check\n", 1);
                clr |= PICONTROL_STATUS_LEFT_GATEWAY;
                break;
            case MODGATE_ST_ID_REQ:     // id request und response werden für die Ausgabe nicht unterschieden
            case MODGATE_ST_ID_RESP:
                DF_PRINTK("Module %d: id request\n", 1);
                clr |= PICONTROL_STATUS_LEFT_GATEWAY;
                break;
            case MODGATE_ST_RUN_NO_DATA:
                DF_PRINTK("Module %d: run no data\n", 1);
                clr |= PICONTROL_STATUS_LEFT_GATEWAY;
                break;
            case MODGATE_ST_RUN:
                DF_PRINTK("Module %d: run\n", 1);
                set |= PICONTROL_STATUS_LEFT_GATEWAY;
                break;
            }
            piDev_g.ai8uPI[RevPiScan.dev[0].i16uInputOffset] &= ~clr;
            piDev_g.ai8uPI[RevPiScan.dev[0].i16uInputOffset] |= set;
        }
#endif
#endif

    MODGATECOM_managePowerLedRun ();
}

//*************************************************************************************************
//| Function: MODGATECOM_managePowerLedRun
//|
//! \brief
//!
//! \detailed
//!
//!
//!
//! \ingroup
//-------------------------------------------------------------------------------------------------
void MODGATECOM_managePowerLedRun (
    void)

{
#if defined(__KUNBUSPI_KERNEL__) || defined(__KUNBUSPI__)

    switch (MODGATECOM_enLedRunState_s)
    {
        case MODGATECOM_enPLR_INIT:
        {
            kbUT_TimerStart (&MODGATECOM_tGreenTimer_s, LED_GREEN_TIME);
            MODGATECOM_enLedRunState_s = MODGATECOM_enPLR_GREEN_PHASE;
        }   break;
        case MODGATECOM_enPLR_GREEN_PHASE:
        {
            if (kbUT_TimerExpired (&MODGATECOM_tGreenTimer_s))
            {
                MODGATECOM_enLedRunState_s = MODGATECOM_enPLR_RUN;
                AL_Data_s[0].enLedStateOld = MODGATECOM_enPLS_INIT; // give new state a chance to react on the state of the communication
                AL_Data_s[1].enLedStateOld = MODGATECOM_enPLS_INIT; // give new state a chance to react on the state of the communication
            }
        }   break;
        case MODGATECOM_enPLR_RUN:
        {
            MODGATECOM_EPowerLedState ledStateAct;

            if (AL_Data_s[0].enLedStateAct <= MODGATECOM_enPLS_LINK_MISSING)
            {
                // if the state is less or equal to link check the module is missing and the led shows the state of the other module only.
                ledStateAct = AL_Data_s[1].enLedStateAct;
            }
            else if (AL_Data_s[1].enLedStateAct <= MODGATECOM_enPLS_LINK_MISSING)
            {
                // if the state is less or equal to link check the module is missing and the led shows the state of the other module only.
                ledStateAct = AL_Data_s[0].enLedStateAct;
            }
            else
            {
                // if both modules are present, we show the lower state
                if (AL_Data_s[0].enLedStateAct <= AL_Data_s[1].enLedStateAct)
                {
                    ledStateAct = AL_Data_s[0].enLedStateAct;
                }
                else
                {
                    ledStateAct = AL_Data_s[1].enLedStateAct;
                }
            }

            if (ledStateAct != AL_Data_s[0].enLedStateOld)
            {
                switch (ledStateAct)
                {
                    case MODGATECOM_enPLS_RUN:
                    {
                    }   break;
                case MODGATECOM_enPLS_LINK_MISSING:
                case MODGATECOM_enPLS_DATA_MISSING:
                    default:
                    {
                    }   break;
                }
                AL_Data_s[0].enLedStateOld = ledStateAct;
            }

            if (ledStateAct != AL_Data_s[0].enLedStateOld)
            {
                switch (ledStateAct)
                {
                    case MODGATECOM_enPLS_RUN:
                    {
                    }   break;
                case MODGATECOM_enPLS_LINK_MISSING:
                case MODGATECOM_enPLS_DATA_MISSING:
                    default:
                    {
                    }   break;
                }
                AL_Data_s[0].enLedStateOld = ledStateAct;
            }
    }   break;
        default:
        {

        }   break;
    }

#else
    switch (MODGATECOM_enLedRunState_s)
    {
        case MODGATECOM_enPLR_INIT:
        {
            kbUT_TimerStart (&MODGATECOM_tGreenTimer_s, LED_GREEN_TIME);
            MODGATECOM_enLedRunState_s = MODGATECOM_enPLR_GREEN_PHASE;
            LED_setLed (MODGATE_LED_POWER, LED_ST_GREEN_BLINK_500);
        }   break;
        case MODGATECOM_enPLR_GREEN_PHASE:
        {
            if (AL_Data_s[0].enLedStateAct != AL_Data_s[0].enLedStateOld)
            {
                switch (AL_Data_s[0].enLedStateAct)
                {
                    case MODGATECOM_enPLS_RUN:
                    {
                        LED_setLed (MODGATE_LED_POWER, LED_ST_GREEN_ON);
                    }   break;
                    case MODGATECOM_enPLS_ERROR:    // an real error is an error, always!
                    {
                        LED_setLed (MODGATE_LED_POWER, LED_ST_RED_ON);
                    }   break;
                    default:
                    {
                        LED_setLed (MODGATE_LED_POWER, LED_ST_GREEN_BLINK_500);
                    }   break;
                }
                AL_Data_s[0].enLedStateOld = AL_Data_s[0].enLedStateAct;
            }

            if (kbUT_TimerExpired (&MODGATECOM_tGreenTimer_s))
            {
                MODGATECOM_enLedRunState_s = MODGATECOM_enPLR_RUN;
                AL_Data_s[0].enLedStateOld = MODGATECOM_enPLS_INIT; // give new state a chance to react on the state of the communication
            }
        }   break;
        case MODGATECOM_enPLR_RUN:
        {
            if (AL_Data_s[0].enLedStateAct != AL_Data_s[0].enLedStateOld)
            {
                switch (AL_Data_s[0].enLedStateAct)
                {
                    case MODGATECOM_enPLS_LINK_MISSING:
                    {
                        LED_setLed (MODGATE_LED_POWER, LED_ST_RED_BLINK_150);
                    }   break;
                    case MODGATECOM_enPLS_DATA_MISSING:
                    {
                        LED_setLed (MODGATE_LED_POWER, LED_ST_RED_BLINK_500);
                    }   break;
                    case MODGATECOM_enPLS_RUN:
                    {
                        LED_setLed (MODGATE_LED_POWER, LED_ST_GREEN_ON);
                    }   break;
                    default:
                    {
                        LED_setLed (MODGATE_LED_POWER, LED_ST_RED_ON);
                    }   break;
                }
                AL_Data_s[0].enLedStateOld = AL_Data_s[0].enLedStateAct;
            }
        }   break;
        default:
        {

        }   break;
    }
#endif // __STM32GENERIC__
}


//*************************************************************************************************
//| Function: MODGATECOM_Send_ID_Req
//|
//! \brief
//!
//! \detailed
//!
//!
//!
//! \ingroup
//-------------------------------------------------------------------------------------------------
INT32U MODGATECOM_send_ID_Req (ALHandle alHdl)
{
    MODGATECOM_Packet *pPacket_l = (MODGATECOM_Packet *)kbUT_malloc(LL_pvPacketHeapHandle_g, 2, MODGATE_TL_HEADER_LEN);
    if (pPacket_l == 0)
    {
        MODGATECOM_error(MODGATECOM_ERROR_OUT_OF_MEMORY_3, bTRUE, 0);
        return MODGATECOM_ERROR_OUT_OF_MEMORY_3;
    }

    pPacket_l->strTransportLayer.i16uCmd = MODGATE_AL_CMD_ID_Req;
    pPacket_l->strTransportLayer.i16uDataLength = 0;
    pPacket_l->strTransportLayer.i32uError = MODGATECOM_NO_ERROR;

    return MG_LL_send(&alHdl->llParas, pPacket_l);
}


//*************************************************************************************************
//| Function: MODGATECOM_Send_ID_Resp
//|
//! \brief
//!
//! \detailed
//!
//!
//!
//! \ingroup
//-------------------------------------------------------------------------------------------------
INT32U MODGATECOM_send_ID_Resp (ALHandle alHdl)
{
    MODGATECOM_Packet *pPacket_l = (MODGATECOM_Packet *)kbUT_malloc(LL_pvPacketHeapHandle_g, 3, MODGATE_TL_HEADER_LEN + sizeof(MODGATECOM_IDResp));
    if (pPacket_l == 0)
    {
        MODGATECOM_error(MODGATECOM_ERROR_OUT_OF_MEMORY_4, bTRUE, 0);
        return MODGATECOM_ERROR_OUT_OF_MEMORY_4;
    }

    pPacket_l->strTransportLayer.i16uCmd = MODGATE_AL_CMD_ID_Resp;
    pPacket_l->strTransportLayer.i16uDataLength = sizeof(MODGATECOM_IDResp);
    pPacket_l->strTransportLayer.i32uError = MODGATECOM_NO_ERROR;
    memcpy(pPacket_l->i8uData, &MODGATE_OwnID_g, sizeof(MODGATECOM_IDResp));

    return MG_LL_send(&alHdl->llParas, pPacket_l);
}


//*************************************************************************************************
//| Function: MG_recv_Id_Resp
//|
//! \brief
//!
//! \detailed
//!
//!
//!
//! \ingroup
//-------------------------------------------------------------------------------------------------
TBOOL MODGATECOM_recv_Id_Resp(ALHandle alHdl, MODGATECOM_Packet *pPacket_p)
{
    memcpy(&alHdl->OtherID, pPacket_p->i8uData, sizeof(MODGATECOM_IDResp));

    // Test feature descriptor
    if (!(alHdl->OtherID.i16uFeatureDescriptor & MODGATE_feature_IODataExchange))
    {
        kbUT_free(pPacket_p);
        return bFALSE;
    }

    // Output Datenlänge an Input Datenlänge des anderen mGates anpassen
    if (alHdl->OtherID.i16uFBS_InputLength < alHdl->i16uOutDataLen)
    {
        alHdl->i16uOutDataLenActive = alHdl->OtherID.i16uFBS_InputLength;
    }
    else
    {
        alHdl->i16uOutDataLenActive = alHdl->i16uOutDataLen;
    }

    // Input Datenlänge an Output Datenlänge des anderen mGates anpassen
    if (alHdl->OtherID.i16uFBS_OutputLength < alHdl->i16uInDataLen)
    {
        alHdl->i16uInDataLenActive = alHdl->OtherID.i16uFBS_OutputLength;
    }
    else
    {
        alHdl->i16uInDataLenActive = alHdl->i16uInDataLen;
    }

    kbUT_free(pPacket_p);
    return bTRUE;
}

//*************************************************************************************************
//| Function: MODGATECOM_Send_cyclicPD
//|
//! \brief
//!
//! \detailed
//!
//!
//!
//! \ingroup
//-------------------------------------------------------------------------------------------------
INT32U MODGATECOM_send_cyclicPD (ALHandle alHdl)
{
    MODGATECOM_CyclicPD *strCyclicPd;
    MODGATECOM_Packet *pPacket_l = (MODGATECOM_Packet *)kbUT_malloc(LL_pvPacketHeapHandle_g, 4, MODGATE_TL_HEADER_LEN + sizeof(MODGATECOM_CyclicPD) + alHdl->i16uOutDataLenActive);
    if (pPacket_l == 0)
    {
        MODGATECOM_error(MODGATECOM_ERROR_OUT_OF_MEMORY_5, bTRUE, 0);
        return  MODGATECOM_ERROR_OUT_OF_MEMORY_5;
    }
    strCyclicPd = (MODGATECOM_CyclicPD *)pPacket_l->i8uData;

    pPacket_l->strTransportLayer.i16uCmd = MODGATE_AL_CMD_cyclicPD;
    pPacket_l->strTransportLayer.i16uDataLength = sizeof(MODGATECOM_CyclicPD) + alHdl->i16uOutDataLenActive;
    pPacket_l->strTransportLayer.i32uError = MODGATECOM_NO_ERROR;

    // Eigener Feldbusstatus
    strCyclicPd->i8uFieldbusStatus = i8uOwnFieldbusState_s;
    // Maximale Datenlänge des anderen mGate verwenden
    strCyclicPd->i16uDataLen = alHdl->i16uOutDataLenActive;
    // TODO: Offset immer 0
    strCyclicPd->i16uOffset = 0;
    // Aktuelle Prozessdaten kopieren
    memcpy(strCyclicPd->i8uData, alHdl->pi8uOutData, alHdl->i16uOutDataLenActive);

    return MG_LL_send(&alHdl->llParas, pPacket_l);
}

//*************************************************************************************************
//| Function: MG_recv_cyclicPD
//|
//! \brief
//!
//! \detailed
//!
//!
//!
//! \ingroup
//-------------------------------------------------------------------------------------------------
TBOOL MODGATECOM_recv_cyclicPD(ALHandle alHdl, MODGATECOM_Packet *pPacket_p)
{
    MODGATECOM_CyclicPD *strCyclicPd = (MODGATECOM_CyclicPD *)pPacket_p->i8uData;

    if (alHdl->i16uInDataLenActive < strCyclicPd->i16uDataLen)
    {
        MODGATECOM_error(MODGATECOM_ERROR_INVALID_DATA_SIZE, bFALSE, 0);
    }
    else if (strCyclicPd->i16uOffset + strCyclicPd->i16uDataLen > alHdl->i16uInDataLenActive)
    {
        MODGATECOM_error(MODGATECOM_ERROR_INVALID_DATA_OFFSET, bFALSE, 0);
    }
    else
    {
        memcpy(alHdl->pi8uInData + strCyclicPd->i16uOffset, strCyclicPd->i8uData, strCyclicPd->i16uDataLen); //FBS_Input to PLC
        alHdl->i8uOtherFieldbusState = strCyclicPd->i8uFieldbusStatus;
    }

    kbUT_free(pPacket_p);
    return bTRUE;
}

//*************************************************************************************************
//| Function: MODGATECOM_SetOwnFieldbusState
//|
//! \brief  Set the own fieldbus state. It will be sent to the partner in the next cyclicPD packet
//!
//! \detailed
//!
//!
//!
//! \ingroup
//-------------------------------------------------------------------------------------------------
void MODGATECOM_SetOwnFieldbusState(INT8U i8uOwnFieldbusState_p)
{
    i8uOwnFieldbusState_s = i8uOwnFieldbusState_p;
}


//*************************************************************************************************
//| Function: MODGATECOM_GetOwnFieldbusState
//|
//! \brief  Return the own fieldbus state.
//!
//! \detailed
//!
//!
//!
//! \ingroup
//-------------------------------------------------------------------------------------------------
INT8U MODGATECOM_GetOwnFieldbusState(void)
{
    return i8uOwnFieldbusState_s;
}


//*************************************************************************************************
//| Function: MODGATECOM_GetOtherFieldbusState
//|
//! \brief  Return the fieldbus state of the partner.
//! It is updated with each cyclicPD packet from partner.
//!
//! \detailed
//!
//!
//!
//! \ingroup
//-------------------------------------------------------------------------------------------------
INT8U MODGATECOM_GetOtherFieldbusState(INT8U i8uInst_p)
{
    return AL_Data_s[i8uInst_p].i8uOtherFieldbusState;
}


//*************************************************************************************************
//| Function: MODGATECOM_GetState
//|
//! \brief  Return the state of the mGate state machine
//!
//!
//! \detailed
//!
//!
//!
//! \ingroup
//-------------------------------------------------------------------------------------------------
MODGATE_AL_Status MODGATECOM_GetState(INT8U i8uInst_p)
{
#ifdef __STM32GENERIC__
    if (MODGATECOM_enLedRunState_s != MODGATECOM_enPLR_RUN)
    {
        // in the first time after boot up we report RUN
        // to avoid that a red LED is on before the initialization is complete
        return MODGATE_ST_RUN;
    }
#endif // __STM32GENERIC__
    return (MODGATE_AL_Status)AL_Data_s[i8uInst_p].i8uState;
}

//*************************************************************************************************
//| Function: MODGATECOM_GetLedState
//|
//! \brief  Return the state of the mGate LED state machine
//!
//!
//! \detailed
//!
//!
//!
//! \ingroup
//-------------------------------------------------------------------------------------------------
MODGATECOM_EPowerLedState MODGATECOM_GetLedState(void)
{
#ifdef __STM32GENERIC__
    return AL_Data_s[0].enLedStateAct;
#else
    return MODGATECOM_enPLS_INIT;
#endif // __STM32GENERIC__
}

//*************************************************************************************************
//| Function: MODGATECOM_GetInputDataLengthActive
//|
//! \brief  Return the active length of the (own) input data (minimum negotiated with peer)
//!
//!
//! \detailed
//!
//!
//!
//! \ingroup
//-------------------------------------------------------------------------------------------------
INT16U MODGATECOM_GetInputDataLengthActive(INT8U i8uInst_p)   // in modular Gateways, always 0 must be passed
{
    return AL_Data_s[i8uInst_p].i16uInDataLenActive;
}

//*************************************************************************************************
//| Function: MODGATECOM_GetOutputDataLengthActive
//|
//! \brief  Return the active length of the (own) output data (minimum negotiated with peer)
//!
//!
//! \detailed
//!
//!
//!
//! \ingroup
//-------------------------------------------------------------------------------------------------
INT16U MODGATECOM_GetOutputDataLengthActive(INT8U i8uInst_p)   // in modular Gateways, always 0 must be passed
{
    return AL_Data_s[i8uInst_p].i16uOutDataLenActive;
}


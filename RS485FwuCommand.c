//+=============================================================================================
//|
//|    SerialFwuCommand.cpp : Serial Firmware Update Command.
//|
//+---------------------------------------------------------------------------------------------
//|
//|    File-ID:    $Id: RS485FwuCommand.c 477 2017-01-10 11:47:36Z mduckeck $
//|    Location:   $URL: http://srv-kunbus03.de.pilz.local/raspi/trunk/products/PiCore/piKernelMod/RS485FwuCommand.c $
//|    Copyright:  KUNBUS GmbH
//|
//+=============================================================================================

#include <project.h>
#include <common_define.h>

#include <linux/module.h>    // included for all kernel modules
#include <linux/kernel.h>    // included for KERN_INFO
#include <linux/slab.h>    // included for KERN_INFO
#include <linux/delay.h>

#include "piIOComm.h"
#include "RS485FwuCommand.h"


#define TEL_MAX_BUF_LEN  300

//*************************************************************************************************
//CRs485FwuCommand::CRs485FwuCommand (
//    void)

//{
//    CArgs ^ptArgs_l = CArgs::getSingleton ();

//    if (ptArgs_l->p_pstrCom)
//    {
//        CComSerialUart ^ptUart_l = gcnew CComSerialUart;
//        ptUart_l->init (ptArgs_l->p_pstrCom);
//        pcoCom_m = ptUart_l;
//    }
//    else if (ptArgs_l->p_ptIpAddr)
//    {
//        m_ptDrvModGate = gcnew CModGateTestCommand (ptArgs_l->p_ptIpAddr);
//        m_ptDrvModGate->ping ();
//    }

//    i8uDstAdr_m = (ptArgs_l->p_enFamily == CArgs::EFamily::enKUNBUS_MGATE_LEFT) ? 1 : 2;
//}

////*************************************************************************************************
//CRs485FwuCommand::~CRs485FwuCommand (
//    void)

//{

//}

////*************************************************************************************************
//void CRs485FwuCommand::fwuInitDrv (
//    void)

//{

//}

////*************************************************************************************************
INT32S fwuEnterFwuMode (INT8U address)
{
    fwuSendTel(address, eCmdSetFwUpdateMode, NULL, 0);

    msleep(100);

    // there is no response for this command
    return 0;
}


////*************************************************************************************************
INT32S fwuWriteSerialNum (
    INT8U address,
    INT32U i32uSerNum_p)

{
    SRs485Telegram suRecvTelegram_l;
    INT32S i32sErr_l;

    fwuSendTel(address, eCmdWriteSerialNumber, (INT8U*)&i32uSerNum_p, sizeof (INT32U));

    msleep(100);

    i32sErr_l = fwuReceiveTel(&suRecvTelegram_l);
    if (i32sErr_l != 0)
    {   // Error occurred
	return i32sErr_l;
    }

    if (suRecvTelegram_l.i16uCmd & MODGATE_RS485_COMMAND_ANSWER_ERROR)
    {
	if (suRecvTelegram_l.i8uDataLen == 4)
	{
	    i32sErr_l = *((INT32S *)suRecvTelegram_l.ai8uData);
	    pr_err("fwuWriteSerialNum: module reported error 0x%08x", i32sErr_l);
	    return -12;
	}
	else
	{
	    return -13;
	}
    }
    return 0;
}


INT32S fwuEraseFlash (INT8U address)
{
	SRs485Telegram suRecvTelegram_l;
	INT32S i32sErr_l;

	fwuSendTel(address, eCmdEraseFwFlash, 0, 0);

	i32sErr_l = fwuReceiveTelTimeout(&suRecvTelegram_l, 6000);
	if (i32sErr_l != 0)
	{
		// Error occurred
		return i32sErr_l;
	}

	if (suRecvTelegram_l.i16uCmd & MODGATE_RS485_COMMAND_ANSWER_ERROR)
	{
	    if (suRecvTelegram_l.i8uDataLen == 4)
	    {
		i32sErr_l = *((INT32S *)suRecvTelegram_l.ai8uData);
		pr_err("fwuEraseFlash: module reported error 0x%08x", i32sErr_l);
		return -12;
	    }
	    else
	    {
		return -13;
	    }
	}
	return 0;
}

INT32S fwuWrite(INT8U address, INT32U flashAddr, char *data, INT32U length)
{
	SRs485Telegram suRecvTelegram_l;
	INT32S i32sErr_l;
	INT8U ai8uSendBuf_l[TEL_MAX_BUF_LEN];

	memcpy (ai8uSendBuf_l, &flashAddr, sizeof (flashAddr));
	if (length <= 0 || length > TEL_MAX_BUF_LEN-sizeof(flashAddr))
		return -14;

	memcpy (ai8uSendBuf_l + sizeof (flashAddr), data, length);

	fwuSendTel(address, eCmdWriteFwFlash, ai8uSendBuf_l, sizeof (flashAddr) + length);

	i32sErr_l = fwuReceiveTelTimeout(&suRecvTelegram_l, 1000);
	if (i32sErr_l != 0)
	{   // Error occurred
		return i32sErr_l;
	}

	if (suRecvTelegram_l.i16uCmd & MODGATE_RS485_COMMAND_ANSWER_ERROR)
	{
	    if (suRecvTelegram_l.i8uDataLen == 4)
	    {
		i32sErr_l = *((INT32S *)suRecvTelegram_l.ai8uData);
		pr_err("fwuWrite: module reported error 0x%08x", i32sErr_l);
		return -12;
	    }
	    else
	    {
		return -13;
	    }
	}
	return 0;
}

INT32S fwuResetModule (INT8U address)
{
    SRs485Telegram suRecvTelegram_l;
    INT32S i32sErr_l;

    fwuSendTel(address, eCmdResetModule, NULL, 0);

    msleep(100);

    i32sErr_l = fwuReceiveTelTimeout(&suRecvTelegram_l, 10000);
    if (i32sErr_l != 0)
    {   // Error occurred
	return i32sErr_l;
    }

    if (suRecvTelegram_l.i16uCmd & MODGATE_RS485_COMMAND_ANSWER_ERROR)
    {
	if (suRecvTelegram_l.i8uDataLen == 4)
	{
	    i32sErr_l = *((INT32S *)suRecvTelegram_l.ai8uData);
	    pr_err("fwuResetModule: module reported error 0x%08x", i32sErr_l);
	    return -12;
	}
	else
	{
	    return -13;
	}
    }
    return 0;
}


////*************************************************************************************************
////| Function: crc
////|
////! \brief
////!
////! \detailed
////!
////!
////!
////! \ingroup
////-------------------------------------------------------------------------------------------------
INT8U fwuCrc(INT8U *piData, INT16U len)
{
    INT8U i8uCrc = 0;
    INT16U i16uI;
    for(i16uI = 0; i16uI < len; i16uI++)
    {
	i8uCrc ^= piData[i16uI];
    }
    return i8uCrc;
}

////*************************************************************************************************
////| Function: sendTel
////|
////! \brief
////!
////! \detailed
////!
////!
////!
////! \ingroup
////-------------------------------------------------------------------------------------------------
INT32U fwuSendTel (
    INT8U address,
    INT8U i16uCmd_p,
    INT8U *pi8uData_p,
    INT8U i8uDataLen_p)

{
    SRs485Telegram suSendTelegram_l;
    INT32U i32uRv_l = 0;

    memset(&suSendTelegram_l, 0, sizeof(SRs485Telegram));
    suSendTelegram_l.i8uDstAddr = address;       // destination
    suSendTelegram_l.i8uSrcAddr = 0;             // sender PC/RevPi
    suSendTelegram_l.i16uCmd = i16uCmd_p;       // command
    if(pi8uData_p != 0)
    {
	suSendTelegram_l.i8uDataLen = i8uDataLen_p;
	memcpy(suSendTelegram_l.ai8uData, pi8uData_p, i8uDataLen_p);
    }
    else
    {
	suSendTelegram_l.i8uDataLen = 0;
    }
    suSendTelegram_l.ai8uData[i8uDataLen_p] = fwuCrc((INT8U*)&suSendTelegram_l, RS485_HDRLEN + i8uDataLen_p);

    i32uRv_l = piIoComm_send((INT8U*)&suSendTelegram_l, RS485_HDRLEN + suSendTelegram_l.i8uDataLen + 1);

    return i32uRv_l;
}

////*************************************************************************************************
INT32S fwuReceiveTelTimeout (
    SRs485Telegram *psuRecvTelegram_p, INT16U timeout_p)
{
    INT8U i8uLen_l;

    if (piIoComm_recv_timeout((INT8U *)psuRecvTelegram_p, RS485_HDRLEN, timeout_p) == RS485_HDRLEN)
    {
	// header was received -> receive data part
	i8uLen_l = piIoComm_recv(psuRecvTelegram_p->ai8uData, psuRecvTelegram_p->i8uDataLen + 1);
	if (i8uLen_l != psuRecvTelegram_p->i8uDataLen + 1)
	{
	    return -2;
	}

	if (psuRecvTelegram_p->ai8uData[psuRecvTelegram_p->i8uDataLen] != fwuCrc((INT8U*)psuRecvTelegram_p, RS485_HDRLEN + psuRecvTelegram_p->i8uDataLen))
	{
	    return -3;
	}
    }
    else
    {
	return -1;
    }

    return (0);
}

INT32S fwuReceiveTel (
    SRs485Telegram *psuRecvTelegram_p)
{
    INT8U i8uLen_l;

    if (piIoComm_recv((INT8U *)psuRecvTelegram_p, RS485_HDRLEN) == RS485_HDRLEN)
    {
	// header was received -> receive data part
	i8uLen_l = piIoComm_recv(psuRecvTelegram_p->ai8uData, psuRecvTelegram_p->i8uDataLen + 1);
	if (i8uLen_l != psuRecvTelegram_p->i8uDataLen + 1)
	{
	    return -2;
	}

	if (psuRecvTelegram_p->ai8uData[psuRecvTelegram_p->i8uDataLen] != fwuCrc((INT8U*)psuRecvTelegram_p, RS485_HDRLEN + psuRecvTelegram_p->i8uDataLen))
	{
	    return -3;
	}
    }
    else
    {
	return -1;
    }

    return (0);
}

////*************************************************************************************************

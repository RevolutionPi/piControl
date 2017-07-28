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

#pragma once

#include <common_define.h>
#include <ModGateRS485.h>

INT32S fwuEnterFwuMode (INT8U address);
INT32S fwuWriteSerialNum (INT8U address, INT32U i32uSerNum_p);
INT32S fwuEraseFlash (INT8U address);
INT32S fwuWrite(INT8U address, INT32U flashAddr, char *data, INT32U length);
INT32S fwuResetModule (INT8U address);

INT8U  fwuCrc(INT8U *piData, INT16U len);
INT32U fwuSendTel (INT8U address, INT8U i8uCmd_p, INT8U *pi8uData_p, INT8U i8uDataLen_p);
INT32S fwuReceiveTel (SRs485Telegram *psuRecvTelegram_p);
INT32S fwuReceiveTelTimeout (SRs485Telegram *psuRecvTelegram_p, INT16U timeout_p);

#if 0
    // IRegFwuInterface implementation
    virtual void fwuInitDrv (void);
    virtual void fwuEnterFwuMode (void);
    virtual void fwuPing (void);
    virtual IRegFwuInterface::SFwuInfo ^fwuGetInfo (void);
    virtual array<INT8U> ^fwuRead (INT32U i32uAddr_p);
#ifdef STM_FWU_FPGA
    virtual void fwuEraseFlashFpga(void);
    virtual void fwuWriteFpga(INT32U i32uAddr_p, array<INT8U> ^ai8uData_p);
#endif
    virtual void fwuEraseFlash (void);
    virtual void fwuWrite (INT32U i32uAddr_p, array<INT8U> ^ai8uData_p);
    virtual void fwuWriteSerialNum (INT32U i32uSerNum_p);
    virtual void fwuWriteMacAddr (array<INT8U> ^ai8uMacAddr_p);
    virtual void fwuWriteDeviceType (INT16U i16uDeviceType_p);
    virtual void fwuWriteHwRevision (INT16U i16uHwRevision_p);
    virtual void fwuWriteApplEnd (INT32U i32uApplEndAddr_p);
    virtual void fwuResetModule (void);

    void fwuGetErrorLog (INT32U ai32uErrorLog[8]);

protected:
    INT8U crc(INT8U *piData, INT16U len);
    INT32U sendTel (INT8U i8uCmd_p, INT8U *pi8uData_p, INT8U i8uDataLen_p);
    INT32U receiveTel (SRs485Telegram *psuRecvTelegram_p);//INT8U *pi8uData_p, INT32U *pi32uDataLen_p);


protected:
    IComSerial ^pcoCom_m;
    INT8U i8uDstAdr_m; // left or right mGate module
    CModGateTestCommand ^m_ptDrvModGate;
#endif

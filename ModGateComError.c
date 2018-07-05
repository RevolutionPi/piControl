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

#include <project.h>

#include <linux/kernel.h>

#include <common_define.h>

#include <stdarg.h>

#include <ModGateComError.h>

#if defined (STM_WITH_EEPROM)
#include <bsp/eeprom/eeprom.h>
#endif

static BSP_TJumpBuf *MODGATECOM_ptJumpBuf_s = (BSP_TJumpBuf *) 0;
static void (*MODGATECOM_cbErrHandler_s) (INT32U i32uErrorCode_p, TBOOL bFatal_p, INT8U i8uParaCnt_p, va_list argptr_p);
static INT32U i32uFatalError_s;

//*************************************************************************************************
//| Function: MODGATECOM_errorSetUp
//|
//! \brief
//!
//! \detailed
//!
//!
//!
//! \ingroup
//-------------------------------------------------------------------------------------------------
void MODGATECOM_errorInit(BSP_TJumpBuf * ptExceptionPoint_p,
			  void (*cbErrHandler_p) (INT32U i32uErrorCode_p,
						  TBOOL bFatal_p, INT8U i8uParaCnt_p, va_list argptr_p))
{

	MODGATECOM_ptJumpBuf_s = ptExceptionPoint_p;
	MODGATECOM_cbErrHandler_s = cbErrHandler_p;
}

//*************************************************************************************************
//| Function: MODGATECOM_error
//|
//! \brief
//!
//! \detailed
//!
//!
//!
//! \ingroup
//-------------------------------------------------------------------------------------------------
void MODGATECOM_error(INT32U i32uErrCode_p,	//!< [in] common error code, defined in \ref kbError.h
		      TBOOL bFatal_p,	//!< [in] = bTRUE: fatal error, do not return to caller
		      //!       = bFALSE: no fatal error, continue program flow
		      INT8U i8uParaCnt_p,	//!< [in] number of parameters in bytes
		      ...)	//!< [in] variable list of INT8U parameters
{
	if (bFatal_p) {
		i32uFatalError_s = i32uErrCode_p;
		pr_err("MODGATECOM_error fatal 0x%08x\n", i32uErrCode_p);
	} else {
		pr_err("MODGATECOM_error 0x%08x\n", i32uErrCode_p);
	}
}

//*************************************************************************************************
//| Function: MODGATECOM_has_fatal_error
//|
//! \brief
//!
//! \detailed
//!
//!
//!
//! \ingroup
//-------------------------------------------------------------------------------------------------
INT32U MODGATECOM_has_fatal_error(void)
{
	return i32uFatalError_s;
}

#if defined(MGATE_ERROR_STACK) && defined(STM_WITH_EEPROM)
//+=============================================================================================
//|             Function:       MODGATECOM_getErrorStackEntryVirtualAddress( INT8U i8uStackIndex_p )
//+---------------------------------------------------------------------------------------------
//!             Returns the virtual address of the stack entry on the EEPROM
//+=============================================================================================
static INT16U MODGATECOM_getErrorStackEntryVirtualAddress(INT8U i8uStackIndex_p)
{
	return BSP_EEPROM_ADDR(EE_i32uErrorStackEntry0) + i8uStackIndex_p * sizeof(INT32U);
}

//+=============================================================================================
//|             Function:       MODGATECOM_errorLogEEPROM( INT32U i32uErrCode_p )
//+---------------------------------------------------------------------------------------------
//!             Logs the reported error code to the error stack which resides on the EEPROM
//!             Identical errors in sequence are not logged
//!             The oldest values will be overwritten if necessary
//+=============================================================================================
void MODGATECOM_errorLogEEPROM(INT32U i32uErrCode_p)	//!< [in] 32-Bit Error Code
{
	INT8U i8uErrorStackIndex_l = 0;	//!< error stack index: last written entry on stack
	INT16U i16uVirtualEEPROMAddress_l = 0;	//!< virtual EEPROM address
	INT32U i32uLastReportedErrorCode_l = 0;	//!< last reported error code

	i8uErrorStackIndex_l = BSP_EEPROM_readByte(BSP_EEPROM_ADDR(EE_i8uErrorStackIndex));
	i16uVirtualEEPROMAddress_l = MODGATECOM_getErrorStackEntryVirtualAddress(i8uErrorStackIndex_l);
	i32uLastReportedErrorCode_l = BSP_EEPROM_readDWord(i16uVirtualEEPROMAddress_l);

	if (0 == i32uLastReportedErrorCode_l)	// this is the first access because the log is empty
	{
		i16uVirtualEEPROMAddress_l = MODGATECOM_getErrorStackEntryVirtualAddress(i8uErrorStackIndex_l);
		BSP_EEPROM_writeDWord(i16uVirtualEEPROMAddress_l, i32uErrCode_p);
	} else if (i32uLastReportedErrorCode_l != i32uErrCode_p) {
		i8uErrorStackIndex_l++;
		i8uErrorStackIndex_l %= MGATE_ERROR_STACK_ELEMENTS;
		BSP_EEPROM_writeByte(BSP_EEPROM_ADDR(EE_i8uErrorStackIndex), i8uErrorStackIndex_l);

		i16uVirtualEEPROMAddress_l = MODGATECOM_getErrorStackEntryVirtualAddress(i8uErrorStackIndex_l);
		BSP_EEPROM_writeDWord(i16uVirtualEEPROMAddress_l, i32uErrCode_p);
	}
}

//+=============================================================================================
//|             Function:       MODGATECOM_getErrorLogEEPROM( INT32U *pai32uErrStackBuf_p )
//+---------------------------------------------------------------------------------------------
//!             Copies the error stack from the EEPROM into the given buffer
//!             The buffer size has to be MGATE_ERROR_STACK_ELEMENTS big
//+=============================================================================================
void MODGATECOM_getErrorLogEEPROM(INT32U * pai32uErrStackBuf_p)
{
	INT8U i8uCnt_l = 0;	//!< counter
	INT8U i8uErrorStackIndex_l = 0;	//!< error stack index
	INT16U i16uVirtualEEPROMAddress_l = 0;	//!< virtual EEPROM address

	if (NULL == pai32uErrStackBuf_p) {
		return;
	}
	//start reading stack entries starting from the most actual entry
	i8uErrorStackIndex_l = BSP_EEPROM_readByte(BSP_EEPROM_ADDR(EE_i8uErrorStackIndex));

	for (i8uCnt_l = 0; i8uCnt_l < MGATE_ERROR_STACK_ELEMENTS; i8uCnt_l++) {
		i16uVirtualEEPROMAddress_l = MODGATECOM_getErrorStackEntryVirtualAddress(i8uErrorStackIndex_l);
		pai32uErrStackBuf_p[i8uCnt_l] = BSP_EEPROM_readDWord(i16uVirtualEEPROMAddress_l);

		if (0 == i8uErrorStackIndex_l) {
			i8uErrorStackIndex_l = MGATE_ERROR_STACK_ELEMENTS - 1;
		} else {
			i8uErrorStackIndex_l--;
		}
	}
}
#endif //#if defined(MGATE_ERROR_STACK) && defined(STM_WITH_EEPROM)

//*************************************************************************************************

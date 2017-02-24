//+=============================================================================================
//|
//!             \file ModGateComError.c
//!             Error handling in communication layers of mGate
//|
//+---------------------------------------------------------------------------------------------
//|
//|             File-ID:                $Id: ModGateComError.c 10632 2016-07-14 13:39:42Z mduckeck $
//|             Location:               $URL: http://srv-kunbus03.de.pilz.local/feldbus/software/trunk/platform/ModGateCom/sw/ModGateComError.c $
//|             Company:                $Cpn: KUNBUS GmbH $
//|
//+=============================================================================================

#include <project.h>

#ifdef __KUNBUSPI_KERNEL__
#include <linux/kernel.h>
#elif defined(__KUNBUSPI__)
#include <stdio.h>
#include <stdlib.h>
#define pr_err(...)      printf(__VA_ARGS__)
#endif // __KUNBUSPI__

#include <common_define.h>

#ifdef __STM32GENERIC__
#include <bsp\setjmp\BspSetJmp.h>
#endif // __STM32GENERIC__

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
#ifdef __KUNBUSPI_KERNEL__
	if (bFatal_p) {
		i32uFatalError_s = i32uErrCode_p;
		pr_err("MODGATECOM_error fatal 0x%08x\n", i32uErrCode_p);
	} else {
		pr_err("MODGATECOM_error 0x%08x\n", i32uErrCode_p);
	}
#elif defined(__KUNBUSPI__)
	if (bFatal_p) {
		pr_err("MODGATECOM_error fatal 0x%08x\n", i32uErrCode_p);
		exit(i32uErrCode_p);
	} else {
		pr_err("MODGATECOM_error 0x%08x\n", i32uErrCode_p);
	}
#else
	va_list argptr_l;

	if (MODGATECOM_cbErrHandler_s) {
		va_start(argptr_l, i8uParaCnt_p);
		MODGATECOM_cbErrHandler_s(i32uErrCode_p, bFatal_p, i8uParaCnt_p, argptr_l);
	}

	if (bFatal_p == bTRUE) {
//        kbBSP_LED_setFatalError (kbBSP_LED_RED_ON);

		if (MODGATECOM_ptJumpBuf_s) {
			bspLongJmp(*MODGATECOM_ptJumpBuf_s, 1);
		}

		for (;;) {
		}
	}
#endif // __KUNBUSPI__
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

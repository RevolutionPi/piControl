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

#include <linux/string.h>
#include "common_define.h"
#include <bsp/systick/systick.h>

#include "kbUtilities.h"

//*************************************************************************************************
//| Function: kbUT_getCurrentMs
//|
//! reads out the current value of the tick counter (1ms)
//!
//! detailed
//!
//!
//! ingroup. Util
//-------------------------------------------------------------------------------------------------
INT32U kbUT_getCurrentMs (
    void)           //! \return tick count

{
    INT32U i32uRv_l;

    i32uRv_l = kbGetTickCount ();

    return (i32uRv_l);
}

//*************************************************************************************************
//| Function: kbUT_TimerInit
//|
//! initializes a timer variable
//!
//!
//! ingroup. Util
//-------------------------------------------------------------------------------------------------
void kbUT_TimerInit (
    kbUT_Timer *ptTimer_p)      //!< [inout] pointer to timer struct

{

    memset (ptTimer_p, 0, sizeof (kbUT_Timer));
}

//*************************************************************************************************
//| Function: kbUT_TimerStart
//|
//! starts a timer.
//!
//!
//! ingroup. Util
//-------------------------------------------------------------------------------------------------
void kbUT_TimerStart (
    kbUT_Timer *ptTimer_p,          //!< [inout] pointer to timer struct
    INT32U i32uDuration_p)          //!< [in] duration of timer to run (ms)

{

    ptTimer_p->i32uStartTime = kbUT_getCurrentMs ();
    ptTimer_p->i32uDuration = i32uDuration_p;
    ptTimer_p->bExpired = bFALSE;
    ptTimer_p->bRun = bTRUE;
}

//*************************************************************************************************
//| Function: kbUT_TimerRunning
//|
//! checks if a timer is actually running
//!
//!
//! ingroup. Util
//-------------------------------------------------------------------------------------------------
TBOOL kbUT_TimerRunning (
    kbUT_Timer *ptTimer_p)      //!< [in] pointer to timer struct
				//! \return = bTRUE: timer is actual running, = bFALSE: timer is
				//! either not started or expired.

{
    INT32U i32uTimeDiff_l;

    if (ptTimer_p->bRun == bTRUE)
    {
	i32uTimeDiff_l = (kbUT_getCurrentMs () - ptTimer_p->i32uStartTime);
	if (i32uTimeDiff_l >= ptTimer_p->i32uDuration)
	{  // Timer expired
	    ptTimer_p->bExpired = bTRUE;
	    ptTimer_p->bRun = bFALSE;
	}
    }

    return (ptTimer_p->bRun);
}

//*************************************************************************************************
//| Function: kbUT_TimerExpired
//|
//! checks if a timer is expired.
//!
//! Tests if the Timer just expires. Return value is true only for one call. Can be used to
//! check the timer cyclically, but do an action only once.
//!
//! ingroup. Util
//-------------------------------------------------------------------------------------------------
TBOOL kbUT_TimerExpired (
    kbUT_Timer *ptTimer_p)

{
    INT32U i32uTimeDiff_l;
    TBOOL bRv_l = bFALSE;

    if (ptTimer_p->bRun == bTRUE)
    {
	i32uTimeDiff_l = (kbUT_getCurrentMs () - ptTimer_p->i32uStartTime);
	if (i32uTimeDiff_l >= ptTimer_p->i32uDuration)
	{  // Timer expired
	    ptTimer_p->bExpired = bTRUE;
	    ptTimer_p->bRun = bFALSE;
	    bRv_l = bTRUE;
	}
    }

    bRv_l = ptTimer_p->bExpired;

    ptTimer_p->bExpired = bFALSE; // Reset Flag, so that it is only once TRUE

    return (bRv_l);

}

//*************************************************************************************************
//| Function: kbUT_TimeElapsed
//|
//! calculates the time since the timer was started
//!
//! if a timer is not started or it is expired, zero is given back
//!
//!
//! ingroup. Util
//-------------------------------------------------------------------------------------------------
INT32U kbUT_TimeElapsed (
    kbUT_Timer *ptTimer_p)      //!< [in] pointer to timer
				//! \return time elapsed since timer started in ms

{

    INT32U i32uTimeDiff_l = 0;

    if (ptTimer_p->bRun == bTRUE)
    {
	i32uTimeDiff_l = (kbUT_getCurrentMs () - ptTimer_p->i32uStartTime);
    }

    return (i32uTimeDiff_l);
}

//*************************************************************************************************
//| Function: kbUT_TimerInUse
//|
//! checks if a timer is actually running and the expired flag has not been reset.
//! if false is returned, the timer can be started again.
//!
//! ingroup. Util
//-------------------------------------------------------------------------------------------------
TBOOL kbUT_TimerInUse (
    kbUT_Timer *ptTimer_p)      //!< [in] pointer to timer struct
				//! \return = bTRUE: timer is actual running, = bFALSE: timer is
				//! either not started or expired and can be restarted now

{
  return (ptTimer_p->bRun || ptTimer_p->bExpired);
}

//*************************************************************************************************
//| Function: kbUT_crc32
//|
//! calculates a 32Bit CRC over a data block
//!
//! The Polynom is the Ethernet Polynom  0xEDB88320
//!
//!
//! ingroup. Util
//-------------------------------------------------------------------------------------------------
void kbUT_crc32 (
    INT8U *pi8uData_p,        //!< [in] pointer to data
    INT16U i16uCnt_p,         //!< [in] number of bytes
    INT32U *pi32uCrc_p)       //!< [inout] CRC sum and inital value

{

    INT16U i;
    INT16U j;
    INT32U i32uCrc_l = *pi32uCrc_p;
#define CRC32_POLY  0xEDB88320

    for (i = 0; i < i16uCnt_p; i++)
    {
	i32uCrc_l ^= (INT32U)pi8uData_p[i];
	for (j = 0; j < 8; j++)
	{
	    if (i32uCrc_l & 0x00000001)
	    {
		i32uCrc_l = (i32uCrc_l >> 1) ^ CRC32_POLY;
	    }
	    else
	    {
		i32uCrc_l >>= 1;
	    }
	}
    }

    *pi32uCrc_p = i32uCrc_l;
}

//*************************************************************************************************
//| Function: kbUT_uitoa
//|
//! Converts an unsigned integer into a string.
//! Returns true in case of success.
//!
//! ingroup. Util
//-------------------------------------------------------------------------------------------------
TBOOL kbUT_uitoa(INT32U p_value, INT8U* p_string, INT8U p_radix)
{
  INT8U* stringPtr  = p_string;
  INT8U* stringPtr1 = p_string;
  INT8U  tmp_char;
  INT32U oldValue;

  // Check base
  if (p_radix != 2 && p_radix != 10 && p_radix != 16)
  {
    // Error, invalid base
    return bFALSE;
  }

  do
  {
    oldValue = p_value;
    p_value /= p_radix;
    *stringPtr++ = "0123456789abcdef" [oldValue - p_value * p_radix];
  } while ( p_value );

  *stringPtr-- = '\0';

  // Reverse string
  while(stringPtr1 < stringPtr)
  {
    // Swap chars
    tmp_char = *stringPtr;
    *stringPtr--   = *stringPtr1;
    *stringPtr1++  = tmp_char;
  }

  return bTRUE;
}

//*************************************************************************************************
//| Function: kbUT_atoi
//|
//! Converts a string into an integer.
//! If the string starts with '0x', it is interpreted an hexadecimal number.
//! The function is able to read negative numbers, they are returned as two's complement.
//!
//!
//! ingroup. Util
//-------------------------------------------------------------------------------------------------
unsigned long kbUT_atoi(const char *start, int *success)
{
    unsigned long val = 0;         /* value we're accumulating */
    unsigned long base = 10;
    unsigned long max;
    int neg=0;              /* set to true if we see a minus sign */
    const char *s = start;

    *success = 1;

    /* skip whitespace */
    while (*s==' ' || *s=='\t') {
	s++;
    }

    /* check for sign */
    if (*s=='-') {
	neg=1;
	s++;
    } else if (*s=='0') {
	s++;
	if (*s=='x' || *s=='X') {
	    base = 16;
	    s++;
	}
    } else if (*s=='+') {
	s++;
    }

    max = 0xffffffff / base;

    /* process each digit */
    while (*s) {
	unsigned digit = 0;
	if (*s >= '0' && *s <= '9')
	{
	    digit = *s - '0';
	}
	else if (base == 16 && *s >= 'a' && *s <= 'f')
	{
	    digit = *s - 'a' + 10;
	}
	else if (base == 16 && *s >= 'A' && *s <= 'F')
	{
	    digit = *s - 'A' + 10;
	}
	else
	{
	    // unknown character -> stop conversion
	    break;
	}
	if (val > max)
	    *success = 0; // overflow

	/* shift the number over and add in the new digit */
	val = val*base + digit;

	/* look at the next character */
	s++;
    }

    if (s == start)
    {
	// not a single character has been converted, this is not successful
	*success = 0;
	return val;
    }

    /* handle negative numbers */
    if (neg) {
	val = ~(val-1);
	return val;
    }

    /* done */
    return val;
}

//*************************************************************************************************
//| Function: kbUT_itoa
//|
//! Converts an signed or unsigned integer into a string.
//! If radix is less than 0, val is interpreted as signed value,
//! otherwise, val is interpreted as unsigned value,
//!
//! Returns a pointer to a static buffer. The content of the buffer is only valid
//! until the next call of the function.
//! ingroup. Util
//-------------------------------------------------------------------------------------------------
char *kbUT_itoa(INT32U val, INT16S radix, INT16U len)
{
  static char buffer[13];
  unsigned int i = 0, j;
  char sign = 0;

  if (radix < 0)
  {
    radix = -radix;
    if ((INT32S)val < 0)
    {
      val = ~(val-1);
      sign = 1;
    }
  }
  if (radix > 16 || radix <= 7)
  {
    return 0;
  }

  while (val > 0)
  {
    j = val % radix;
    val = val / radix;
    if (j < 10)
      buffer[i++] = (char) ('0' + j);
    else
      buffer[i++] = (char) ('a' + j - 10);
  }
  if (i == 0)
    buffer[i++] = '0';
  if (sign)
    buffer[i++] = '-';

  while (i<len && i < sizeof(buffer)-1)
  {
    buffer[i++] = ' ';
  }

  buffer[i] = 0;

  // Reverse string
  i--;
  j = 0;
  while(j < i)
  {
    // Swap chars
    sign = buffer[j];
    buffer[j] = buffer[i];
    buffer[i] = sign;
    j++;
    i--;
  }
  return buffer;
}


//*************************************************************************************************


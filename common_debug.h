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
 * For licencing details see COPYING
 *
 *=======================================================================================
 *
 * This file defines global macros for several levels of debug messages and runtime checks.
 * It should be included in all files after project.h.
 * CMN_CHECK_LEVEL can be set in project.h
 *
 *=======================================================================================
 */

#ifndef _COMMON_DEBUG_H_
#define _COMMON_DEBUG_H_

/*
CMN_CHECK_LEVEL defines how checks and debug messages are active
0		only critical check, for release version
1		only checks that are not performance critical
2		extensive checks that may influence the runtime behaviour
3		same checks as at level 2, but with printf

CMN_DEBUG calls printf only on CMN_CHECK_LEVEL 3 (for targets with stdout)
The second parameter is a printf parameter list.
kbDebugLevelMask_g must be defined in the main programm and set to an appropriate
bit mask. The first parameter of kbDEBUGis linked kbDebugLevelMask_g by bitwise 'and'.
So debug messages can be turned on and off even at runtime.

Example:
INT32U CMN_DebugLevelMask_g = 0x03;

CMN_DEBUG(0x10, ("This message will not be printed!\n"));
CMN_DEBUG(0x01, ("You will see this message! num has the value %d.\n", num));

CMN_ASSERT(checkLink(), "no link");


*/

#define CMN_ERROR				1
#define CMN_WARNING			2

#ifndef  CMN_CHECK_LEVEL
#ifdef _WIN32
#define CMN_CHECK_LEVEL		3	// on Windows we do all debugging as default
#else
#define CMN_CHECK_LEVEL		0	// on debugging on the ST as default
#endif
#endif

#if CMN_CHECK_LEVEL >= 0 || CMN_CHECK_LEVEL <= 2

#define CMN_ASSERT(assertion, errCode, message)	if(!(assertion)) platformError(errCode, bTRUE, 0);
#define CMN_DEBUG(level, X)

#elif CMN_CHECK_LEVEL == 3

#ifdef __cplusplus
extern "C" INT32U CMN_DebugLevelMask_g;	// must be defined in globally in main()
#else
extern INT32U CMN_DebugLevelMask_g;	// must be defined in globally in main()
#endif

#include <stdio.h>

#define CMN_ASSERT(assertion, errCode, message)	if(!(assertion)) { printf("ASSERT: %s\n", message); while(1); }
#define CMN_DEBUG(level, X)						if (CMN_DebugLevelMask_g & level)	{ printf X; }

#endif

#endif   // _COMMON_DEBUG_H_

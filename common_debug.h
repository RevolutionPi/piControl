//+=============================================================================================
//|
//|    This file defines global macros for several levels of debug messages and runtime checks.
//|    It should be included in all files after project.h. 
//|    CMN_CHECK_LEVEL can be set in project.h
//|
//+---------------------------------------------------------------------------------------------
//|
//|    File-ID:    $Id: common_debug.h 7218 2014-10-10 13:43:52Z mduckeck $
//|    Location:   $URL: http://srv-kunbus03.de.pilz.local/feldbus/software/trunk/platform/common/sw/common_debug.h $
//|
//|    Copyright:  KUNBUS GmbH
//|
//+=============================================================================================

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

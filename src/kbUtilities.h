/* SPDX-License-Identifier: GPL-2.0-only
 * SPDX-FileCopyrightText: 2016-2023 KUNBUS GmbH
 */

#ifndef KBUTILITIES_H_INC
#define KBUTILITIES_H_INC

#include "common_define.h"
//+=============================================================================================
//|		Typen / types
//+=============================================================================================

typedef struct kbUT_StrTimer
{
    u32 i32uStartTime;
    u32 i32uDuration;
    bool bRun;
    bool bExpired;
} kbUT_Timer;

typedef struct kbUT_StrArgHeap
{
    u8 i8uState;                         //!< State of block; free or occupied
    u8 i8uOwner;                         //!< Debug Marker. Who has allocated the block ?
    u16 i16uLen;                         //!< Length of block
    struct kbUT_StrArgHeap *ptPrev;         //!< Previous descriptor structure
    struct kbUT_StrArgHeap *ptNext;         //!< Previous descriptor structure
} kbUT_TArgHeap;

//+=============================================================================================
//|		Makros / macros
//+=============================================================================================

#define kbUT_TimerReTrigger(TIMER, DURATION)	kbUT_TimerStart (TIMER, DURATION)

// macro to check if something is within a range ...
#define CHECK_LAPPING(OUTER_LOW, OUTER_HIGH, INNER)\
((INNER <= OUTER_HIGH) && (INNER >= OUTER_LOW))

// macro to check if a range is completely within another range ...
#define CHECK_INSIDE(OUTER_LOW,OUTER_HIGH,INNER_LOW,INNER_HIGH)\
((INNER_HIGH <= OUTER_HIGH) && (INNER_LOW >= OUTER_LOW ))

// macro for a comlete range check for two ranges ...
#define CHECK_OVERLAPPING(OUTER_LOW,OUTER_HIGH,INNER_LOW,INNER_HIGH) \
CHECK_INSIDE( OUTER_LOW, OUTER_HIGH, INNER_LOW, INNER_HIGH) || \
CHECK_LAPPING( OUTER_LOW, OUTER_HIGH, INNER_LOW ) || \
CHECK_LAPPING( OUTER_LOW, OUTER_HIGH, INNER_HIGH ) || \
CHECK_INSIDE( INNER_LOW, INNER_HIGH, OUTER_LOW, OUTER_HIGH) || \
CHECK_LAPPING( INNER_LOW, INNER_HIGH, OUTER_LOW ) || \
CHECK_LAPPING( INNER_LOW, INNER_HIGH, OUTER_HIGH )


//+=============================================================================================
//|		Prototypen / prototypes
//+=============================================================================================
#ifdef __cplusplus
extern "C" {
#endif


extern void				kbUT_TimerInit (kbUT_Timer *ptTimer_p);
extern void				kbUT_TimerStart (kbUT_Timer *ptTimer_p, u32 i32uDuration_p);
extern bool			kbUT_TimerRunning (kbUT_Timer *ptTimer_p);
extern bool			kbUT_TimerExpired (kbUT_Timer *ptTimer_p);
extern u32			kbUT_TimeElapsed (kbUT_Timer *ptTimer_p);
extern bool			kbUT_TimerInUse (kbUT_Timer *ptTimer_p);
extern u32			kbUT_getCurrentMs (void);
extern void				kbUT_crc32 (u8 *pi8uData_p, u16 i16uCnt_p, u32 *pi32uCrc_p);
extern void				kbUT_crc8XX(u8 *pi8uData_p, u16 i16uCnt_p, u8 i8uPolynom_p, u8 *pi8uCrc_p);
extern bool			kbUT_uitoa(u32 p_value, u8* p_string, u8 p_radix);
extern unsigned long	kbUT_atoi(const char *s, int *success);
extern char *			kbUT_itoa(u32 val, s16 radix, u16 len);


#ifdef  __cplusplus
}
#endif

//#else
//#error "KBUTILITIES_H_INC already defined !"
#endif // KBUTILITIES_H_INC


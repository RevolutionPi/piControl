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

#ifndef KBUTILITIES_H_INC
#define KBUTILITIES_H_INC

//+=============================================================================================
//|		Typen / types
//+=============================================================================================

typedef struct kbUT_StrTimer
{
    INT32U i32uStartTime;
    INT32U i32uDuration;
    TBOOL bRun;
    TBOOL bExpired;
} kbUT_Timer;

typedef struct kbUT_StrArgHeap
{
    INT8U i8uState;                         //!< State of block; free or occupied
    INT8U i8uOwner;                         //!< Debug Marker. Who has allocated the block ?
    INT16U i16uLen;                         //!< Length of block
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
extern void				kbUT_TimerStart (kbUT_Timer *ptTimer_p, INT32U i32uDuration_p);
extern TBOOL			kbUT_TimerRunning (kbUT_Timer *ptTimer_p);
extern TBOOL			kbUT_TimerExpired (kbUT_Timer *ptTimer_p);
extern INT32U			kbUT_TimeElapsed (kbUT_Timer *ptTimer_p);
extern TBOOL			kbUT_TimerInUse (kbUT_Timer *ptTimer_p);
extern INT32U			kbUT_getCurrentMs (void);
extern void				kbUT_crc32 (INT8U *pi8uData_p, INT16U i16uCnt_p, INT32U *pi32uCrc_p);
extern void				kbUT_crc8XX(INT8U *pi8uData_p, INT16U i16uCnt_p, INT8U i8uPolynom_p, INT8U *pi8uCrc_p);
extern TBOOL			kbUT_uitoa(INT32U p_value, INT8U* p_string, INT8U p_radix);
extern unsigned long	kbUT_atoi(const char *s, int *success);
extern char *			kbUT_itoa(INT32U val, INT16S radix, INT16U len);


#ifdef  __cplusplus
}
#endif

//#else
//#error "KBUTILITIES_H_INC already defined !"
#endif // KBUTILITIES_H_INC


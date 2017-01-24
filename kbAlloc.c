//+=============================================================================================
//|
//!    \file kbAlloc.c
//!    Dynamic memory allocation in uC environment.
//|
//+---------------------------------------------------------------------------------------------
//|
//|    File-ID:    $Id: kbAlloc.c 10496 2016-06-17 11:37:26Z mduckeck $
//|    Location:   $URL: http://srv-kunbus03.de.pilz.local/feldbus/software/trunk/platform/utilities/sw/kbAlloc.c $
//|    Copyright:  KUNBUS GmbH
//|
//+=============================================================================================

#include <common_define.h>
#include <project.h>
#include <common_debug.h>

#if CMN_CHECK_LEVEL == 3
#include <stdio.h>
#endif

#ifdef kbUT_MALLOC_DEBUG
#error replace kbUT_MALLOC_DEBUG by (CMN_CHECK_LEVEL == 3)
#endif

#include <kbAlloc.h>
#ifdef __KUNBUSPI_KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif
#include <bsp/globalIntCtrl/bspGlobalIntCtrl.h>


//*************************************************************************************************
#define kbUT_ARGM_ST_INVALID							1
#define kbUT_ARGM_ST_FREE                               2
#define kbUT_ARGM_ST_OCCUPIED                           3


#define kbUT_ARGM_OWN_SYSTEM                            0   //!< Default owner of a memory block

//*************************************************************************************************

typedef struct kbUT_StrHeap
{
    INT8U i8uState;                         //!< State of block; free or occupied
    INT8U i8uOwner;                         //!< Debug Marker. Who has allocated the block ?
    INT32U i32uLen;                         //!< Length of block
    struct kbUT_StrHeap *ptPrev;            //!< Previous descriptor structure
    struct kbUT_StrHeap *ptNext;            //!< Previous descriptor structure
#if CMN_CHECK_LEVEL >= 2
    INT32U counter;							//!< counts number of malloc-calls in first block and number of NULL-returned in last block
    struct kbUT_StrHeap *ptEnd;				//!< points to the last block, but in the last block it points to the first block 
    const char* file;						//!< filename where malloc was called
    INT32U line;								//!< line number where malloc was called
    INT32U magic;							//!< some number to detect overwrites
#endif
} kbUT_THeap;

//*************************************************************************************************

static volatile INT32U i32uSyncCnt_s = 0;


//*************************************************************************************************
//| Function: kbUT_initHeap
//|
//! brief. Initialize the linked list of blocks in the heap.
//!
//!
//! ingroup.
//-------------------------------------------------------------------------------------------------
void *kbUT_initHeap (
    INT8U *pHeap,						//!< [in] pointer to heap memory
    INT32U i32uLen_p					//!< [in] length of the heap memory area
    )
                                        //! \return handle for memory partition; = NULL: error
{
    kbUT_THeap *ptStart_l;
    kbUT_THeap *ptEnd_l;

    ptStart_l = (kbUT_THeap *)pHeap;
    ptStart_l = (kbUT_THeap *)(((INT32U)ptStart_l+3) & 0xfffffffc); // Do 4 Byte Allignment

    ptEnd_l = (kbUT_THeap *)(pHeap +  i32uLen_p - sizeof (kbUT_THeap));
    ptEnd_l = (kbUT_THeap *)((INT32U)ptEnd_l & 0xfffffffc); // Do 4 Byte Allignment
    
    ptStart_l->ptPrev = (kbUT_THeap *)0;
    ptStart_l->ptNext = ptEnd_l;
    ptStart_l->i8uOwner = kbUT_ARGM_OWN_SYSTEM;
    ptStart_l->i32uLen = (INT32U)(((INT8U *)ptStart_l->ptNext - (INT8U *)ptStart_l) - sizeof (kbUT_THeap));
    ptStart_l->i8uState = kbUT_ARGM_ST_FREE;
#if CMN_CHECK_LEVEL >= 2
    ptStart_l->counter = 0;	// count number of allocated blocks    
    ptStart_l->ptEnd = ptEnd_l;
    ptStart_l->line = 0xffffffff;
    ptStart_l->file = "none";
    ptStart_l->magic = 0xdeadbeef;
#endif

    ptEnd_l->ptPrev = ptStart_l;
    ptEnd_l->ptNext = (kbUT_THeap *)0;
    ptEnd_l->i8uOwner = kbUT_ARGM_OWN_SYSTEM;
    ptEnd_l->i32uLen = i32uLen_p;
    ptEnd_l->i8uState = kbUT_ARGM_ST_OCCUPIED;
#if CMN_CHECK_LEVEL >= 2
    ptEnd_l->counter = 0; // count number of 'out of memory'
    ptEnd_l->ptEnd = ptStart_l;
    ptEnd_l->line = i32uLen_p;
    ptEnd_l->file = "none";
    ptEnd_l->magic = 0xdeadbeef;
#endif

    return (void *)ptStart_l;
}    

//*************************************************************************************************
//| Function: kbUT_malloc
//|
//! brief. Allocate memory from the heap.
//!
//! the function is thread safe, that means it can be use in an interrupt routine. 
//! The function works in a preemptive multitasking system only correct, if at least the last thread 
//! that enters the function is not interrupted. Otherwise it could come to an dead lock.
//!
//! Due to the lack of a heap handle in kbUT_free, there is only a global synchronisation of all heaps
//! (via the static variable i32uSyncCnt_s). This could be improved by assigning each heap an own 
//! synchronization variable. 
//!
//!
//! ingroup.
//-------------------------------------------------------------------------------------------------
#if CMN_CHECK_LEVEL >= 2
void *kbUT_malloc_debug (
    void *pHandle_p,					//!< [in] handle of memory partition
    INT8U i8uOwner_p,                   //!< [in] for Debug purposes. Identifies the requestor
    INT32U i32uLen_p,					//!< [in] requested length of the memeory area
    const char* file,					//!< [in] filename where malloc was called
    const int line)						//!< [in] line number where malloc was called
#else
void *kbUT_malloc (
    void *pHandle_p,					//!< [in] handle of memory partition
    INT8U i8uOwner_p,                   //!< [in] for Debug purposes. Identifies the requestor
    INT32U i32uLen_p)					//!< [in] requested length of the memeory area
#endif
                                        //! \return Pointer to memory; = NULL: out of mem
{
    #define kbUT_MIN_FREE_BLOCK_LEN   (10 + sizeof (kbUT_THeap))       

    kbUT_THeap *ptStart_l;
    kbUT_THeap *ptAct_l;
    kbUT_THeap *ptMarker_l;
    INT32U i32uLen_l;
    void *pvRv_l;
    INT32U i32uSyncCnt_l;
    
    i32uLen_l = (i32uLen_p + 3) & -4;   // round off to 4 Byte allignment
    pvRv_l = (void *)0;
    ptStart_l = (kbUT_THeap *)pHandle_p;
    

    do
    {   // try it as long as there are access overlaps in time 
    
        GLOBAL_INTERRUPT_DISABLE ();        // lock section

        i32uSyncCnt_l = ++i32uSyncCnt_s;
        
        GLOBAL_INTERRUPT_ENABLE ();         // unlock section               


                                             // check if chain is still integer
        for (ptAct_l = ptStart_l; ptAct_l && (i32uSyncCnt_l == i32uSyncCnt_s); ptAct_l = ptAct_l->ptNext)
        {
            if (   (ptAct_l->i8uState == kbUT_ARGM_ST_FREE)
                && (ptAct_l->i32uLen >= i32uLen_l)
               )
            {   // suitable block found
            
                GLOBAL_INTERRUPT_DISABLE ();        // lock section
                
                // check if this block is meanwhile not fetched by an interrupt routine
                if (i32uSyncCnt_l == i32uSyncCnt_s)
                {   // suitable block found
                    if ((ptAct_l->i32uLen - i32uLen_l) >= kbUT_MIN_FREE_BLOCK_LEN)
                    {   // it is worth to split the block
                        ptMarker_l = (kbUT_THeap *)((INT8U *)ptAct_l + i32uLen_l + sizeof (kbUT_THeap));
                        
                        ptMarker_l->ptNext = ptAct_l->ptNext;
                        ptAct_l->ptNext->ptPrev = ptMarker_l;
                        ptMarker_l->i32uLen = (INT32U)(((INT8U *)ptMarker_l->ptNext - (INT8U *)ptMarker_l) - sizeof (kbUT_THeap));
                        ptMarker_l->i8uOwner = kbUT_ARGM_OWN_SYSTEM;
                        ptMarker_l->i8uState = kbUT_ARGM_ST_FREE;
#if CMN_CHECK_LEVEL >= 2
                        ptMarker_l->ptEnd = ptStart_l->ptEnd;
                        ptMarker_l->file = "none";
                        ptMarker_l->line = 0xffffffff;
                        ptMarker_l->magic = 0xdeadbeef;
#endif
                       
                        ptAct_l->ptNext = ptMarker_l;
#if CMN_CHECK_LEVEL >= 2
                        ptAct_l->file = file;
                        ptAct_l->line = line;
#endif

                        ptMarker_l->ptPrev = ptAct_l;
                        ptAct_l->i32uLen = (INT32U)(((INT8U *)ptAct_l->ptNext - (INT8U *)ptAct_l) - sizeof (kbUT_THeap));
                        // Falls ptMarker der vorletzte Block ist, schreibe die Größe in den letzten Block
                        // dann steht im letzten Block die kleinste Größe des noch freien Speichers, bzw. 0
                        if (ptMarker_l->ptNext && ptMarker_l->ptNext->ptNext == 0 && ptMarker_l->i32uLen < ptMarker_l->ptNext->i32uLen)
                            ptMarker_l->ptNext->i32uLen = ptMarker_l->i32uLen;
                    }
                    
#if CMN_CHECK_LEVEL >= 2
                    ptStart_l->counter++;
#endif
                    ptAct_l->i8uOwner = i8uOwner_p;
                    ptAct_l->i8uState = kbUT_ARGM_ST_OCCUPIED;
                    pvRv_l = ptAct_l + 1;
                    if (ptAct_l->ptNext->ptNext == 0)	// last block was allocated, no memory left
                        ptAct_l->ptNext->i32uLen = 0;
                }
                
                GLOBAL_INTERRUPT_ENABLE (); // unlock section               
                break;
            }    
        }
    }   while (pvRv_l == NULL && i32uSyncCnt_l != i32uSyncCnt_s);  // do until we either found a block, or we are one cycle not was disturbed


#if CMN_CHECK_LEVEL >= 2

    GLOBAL_INTERRUPT_DISABLE();        // lock section

    {
        kbUT_THeap *pt;
        int cnt, err, used, free = (int)ptStart_l->ptEnd - (int)ptStart_l;
        if (pvRv_l == 0) // && ptAct_l)
        {	// out of memory
            ptStart_l->ptEnd->counter++;
        }
        for (pt = ptStart_l, cnt=0, err=0, used=0; pt != 0; pt = pt->ptNext, cnt++)
        {
            if (pt->magic != 0xdeadbeef)
                err++;
            if (pt->i8uState == kbUT_ARGM_ST_OCCUPIED && pt->ptNext)
            {
                used++;
                free -= pt->i32uLen;
            }
            if (pt->ptNext && pt->ptNext->ptNext == 0 && pt->ptNext->line > pt->i32uLen)
            {
                pt->ptNext->line = pt->i32uLen;
            }
        }
#if CMN_CHECK_LEVEL == 3
        printf("kbUT_malloc: %2d/%2d/%2d buffers, %2d errors  new  %p  free %5d\n", 
            cnt-1, used, ptStart_l->counter, err, pvRv_l, free);
        if (pvRv_l == 0)
            printf("  called from line %4d file %s\n", line, file);
#else
        while (err);	// stop on error
#endif
    }

    GLOBAL_INTERRUPT_ENABLE(); // unlock section               
#endif
    return (pvRv_l);

}
    
//*************************************************************************************************
//| Function: kbUT_calloc
//|
//! brief. allocates memory from the heap and sets it to zero.
//!
//! the function is thread safe, that means it can be use in an interrupt routine
//!
//!
//!
//! ingroup.
//-------------------------------------------------------------------------------------------------
#if CMN_CHECK_LEVEL >= 2
void *kbUT_calloc_debug (
    void *pHandle_p,					//!< [in] handle of memory partition
    INT8U i8uOwner_p,                   //!< [in] for Debug purposes. Identifies the requestor
    INT32U i32uLen_p,                   //!< [in] requested length of the memeory area
    const char* file,					//!< [in] filename where malloc was called
    const int line)						//!< [in] line number where malloc was called
#else
void *kbUT_calloc (
    void *pHandle_p,					//!< [in] handle of memory partition
    INT8U i8uOwner_p,                   //!< [in] for Debug purposes. Identifies the requestor
    INT32U i32uLen_p)                   //!< [in] requested length of the memeory area
#endif
                                        //! \return Pointer to memory; = NULL: out of mem
{
#if CMN_CHECK_LEVEL >= 2
    void *pvRv_l = kbUT_malloc_debug (pHandle_p, i8uOwner_p, i32uLen_p, file, line);
#else
    void *pvRv_l = kbUT_malloc (pHandle_p, i8uOwner_p, i32uLen_p);
#endif
    
    if (pvRv_l)
    {
        memset (pvRv_l, 0, i32uLen_p);
    }
    
    return (pvRv_l);
}

//*************************************************************************************************
//| Function: kbUT_free
//|
//! brief. frees a previously allocated memory block.
//!
//! the function is thread safe, that means it can be use in an interrupt routine
//!
//!
//!
//! ingroup.
//-------------------------------------------------------------------------------------------------
void kbUT_free (
    void *pvMem_p						//!< [in] pointer to memory block
    )
{
    kbUT_THeap *ptAct_l;
    kbUT_THeap *ptPrev_l;
    kbUT_THeap *ptNext_l;
    
#if CMN_CHECK_LEVEL >= 2    // ----------  for Debug purposes only
    kbUT_THeap *pt, *ptStart_l, *ptEnd_l;
#endif

    ptAct_l = ((kbUT_THeap *)pvMem_p) - 1;

#if CMN_CHECK_LEVEL >= 2    // ----------  for Debug purposes only
    GLOBAL_INTERRUPT_DISABLE();        // lock section

    ptEnd_l = ptAct_l->ptEnd;
    ptStart_l = ptEnd_l->ptEnd;

    if (   (ptAct_l < ptStart_l)
        || (ptAct_l > ptEnd_l)
        || (ptAct_l->i8uState != kbUT_ARGM_ST_OCCUPIED)
        || (ptAct_l->magic != 0xdeadbeef)
       )
    {
#if CMN_CHECK_LEVEL == 3
        printf("kbUT_free: something goes really wrong! act %p start %p end %p state %d\n",
            ptAct_l, ptStart_l, ptEnd_l, ptAct_l->i8uState);
        return;
#else
        while (1);
#endif
    }
    else
    {
        int cnt, err, used, found, free = (int)ptStart_l->ptEnd - (int)ptStart_l;

        for (pt = ptStart_l, cnt=0, err=0, used=0, found=0; pt != 0; pt = pt->ptNext, cnt++)
        {
            if (pt->magic != 0xdeadbeef)
            {
                err++;
            }
            if (pt->i8uState == kbUT_ARGM_ST_OCCUPIED && pt->ptNext)
            {
                used++;
                free -= pt->i32uLen;
                if (pt == ptAct_l)
                    found++;
            }
        }
        if (err > 0 || found != 1)
        {
#if CMN_CHECK_LEVEL == 3
            printf("Problem with linked list: err %d  found %d\n", err, found);
            kbUT_dump(ptStart_l, stdout);
            return;
#else
            while (1);	// stop on error
#endif
        }
        else
        {
#if CMN_CHECK_LEVEL == 3
            printf("kbUT_free:   %2d/%2d/%2d buffers, %2d errors  free %p  free %5d\n",
                cnt - 1, used, ptStart_l->counter, err, pvMem_p, free);
#endif
        }
    }   

    GLOBAL_INTERRUPT_ENABLE(); // unlock section               
    //-----------
#endif

    GLOBAL_INTERRUPT_DISABLE ();        // lock section

    i32uSyncCnt_s++;        // indicate a manipulation of the system

#if CMN_CHECK_LEVEL >= 2
    ptStart_l->counter--;
#endif
     
    // look if there is a previous free block
    ptPrev_l = ptAct_l->ptPrev; 
    if (   (ptPrev_l == NULL)
        || (ptPrev_l->i8uState != kbUT_ARGM_ST_FREE)
       )
    {
        ptPrev_l = ptAct_l;    
    }    
    
    // look if next block is free    
    ptNext_l = ptAct_l->ptNext;
    if (ptNext_l->i8uState == kbUT_ARGM_ST_FREE)
    {
        ptNext_l = ptNext_l->ptNext;
    }
    
    if (ptPrev_l->ptNext != ptNext_l)
    {   // combine all free blocks to one
    
        ptPrev_l->ptNext = ptNext_l;
        ptNext_l->ptPrev = ptPrev_l;
        
        ptPrev_l->i32uLen = (INT32U)(((INT8U *)ptPrev_l->ptNext - (INT8U *)ptPrev_l) - sizeof (kbUT_THeap));
    }
    
    ptPrev_l->i8uOwner = kbUT_ARGM_OWN_SYSTEM;
    ptPrev_l->i8uState = kbUT_ARGM_ST_FREE;
    GLOBAL_INTERRUPT_ENABLE (); // unlock section               
    
}

#if CMN_CHECK_LEVEL >= 3
//*************************************************************************************************
//| Function: kbUT_dump
//|
//! brief. print the blocks in the heap
//!
//! ingroup.
//-------------------------------------------------------------------------------------------------
extern void kbUT_dump(void *pHandle_p, FILE *fp)
{
    kbUT_THeap *ptStart_l = (kbUT_THeap *)pHandle_p;
    kbUT_THeap *ptEnd_l = ptStart_l->ptEnd;
    kbUT_THeap *pt;
    int max = ((int)ptEnd_l - (int)ptStart_l) - (int)ptEnd_l->line;

    for (pt = ptStart_l; pt && pt->ptNext; pt = pt->ptNext)
    {
        fprintf(fp, "kbUT_Malloc: %s  len %4d  addr %p  magic %x  line %3d file %s\n", 
            (pt->i8uState == kbUT_ARGM_ST_FREE) ? "free " : (pt->i8uState == kbUT_ARGM_ST_OCCUPIED) ? "used " : "ERROR",
            pt->i32uLen, (pt+1), pt->magic, pt->line, pt->file);
    }
    fprintf(fp, "%d blocks, %d times out of memory, %d max. used\n", ptStart_l->counter, ptEnd_l->counter, max);
}
#endif


//*************************************************************************************************
//| Function: kbUT_minFree
//|
//! brief. return the minimal number of bytes that was left in the heap in one block.
//!
//!
//! ingroup.
//-------------------------------------------------------------------------------------------------
INT32U kbUT_minFree(void *pHandle_p)
{
    INT32U len = 0;
    kbUT_THeap *pt_l = (kbUT_THeap *)pHandle_p;
    while (pt_l->ptNext)
    {
        pt_l = pt_l->ptNext;
        len++;
    }
    return 1000000*len + pt_l->i32uLen;
}

//*************************************************************************************************

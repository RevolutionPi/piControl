//+=============================================================================================
//|
//!		\file BspSetJmp.h
//!
//!		BSP long jump definitions.
//|
//+---------------------------------------------------------------------------------------------
//|
//|		Files required:	(none)
//|
//+=============================================================================================
#ifndef BSPSETJMP_H_INC
#define BSPSETJMP_H_INC

#if defined (_MSC_VER) || defined(__NIOS_GENERIC__) || defined(__SF2_GENERIC__) || defined (__KUNBUSPI__)
  #include <setjmp.h>
  
  typedef jmp_buf BSP_TJumpBuf;

  #define bspSetJmp(x)  setjmp (x)
  #define bspLongJmp(x,y)  longjmp ((x), (y))
  
#else
  typedef INT32U BSP_TJumpBuf[12];

  #ifdef __cplusplus
  extern "C" { 
  #endif 
  
  extern INT32S bspSetJmp (BSP_TJumpBuf tJmpBuf_p);
  extern void bspLongJmp(BSP_TJumpBuf tJmpBuf_p, INT32S i32sValue_p);

  #ifdef __cplusplus
  }    
  #endif 

#endif




#ifdef __cplusplus
extern "C" { 
#endif 

#ifdef __cplusplus
}    
#endif 


#endif  // BSPSETJMP_H_INC


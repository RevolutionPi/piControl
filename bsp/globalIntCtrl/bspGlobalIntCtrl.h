//+=============================================================================================
//|
//|    functions for diable and enable of global interrupts
//|
//+=============================================================================================

#ifndef BSPGLOBALINTCTRL_H_INC
#define BSPGLOBALINTCTRL_H_INC

#if defined (STM32F3XX)
  #include "stm32f30x.h"

  #define	GLOBAL_INTERRUPT_ENABLE()		__enable_irq()
  #define	GLOBAL_INTERRUPT_DISABLE()		__disable_irq()

#elif defined (STM32F40_41xxx) || defined (STM32F427_437xx) || defined (STM32F429_439xx) || defined (STM32F401xx)
  #include <syslib\inc\stm32f4xx.h>

  #define	GLOBAL_INTERRUPT_ENABLE()		__enable_irq()
  #define	GLOBAL_INTERRUPT_DISABLE()		__disable_irq()

#elif defined (_MSC_VER)
  #include <bsp\globalIntCtrl\bspGlobalIntCtrl_win32.h>

  #define	GLOBAL_INTERRUPT_ENABLE()		bspGlobalIntEnable()
  #define	GLOBAL_INTERRUPT_DISABLE()		bspGlobalIntDisable()

#elif defined (__NIOS_GENERIC__)

  #include <sys\alt_irq.h>

  #define	GLOBAL_INTERRUPT_ENABLE()	{\
                                            alt_irq_context context;\
                                            NIOS2_READ_STATUS (context);\
                                            NIOS2_WRITE_STATUS (context | NIOS2_STATUS_PIE_MSK);\
                                        }
  #define	GLOBAL_INTERRUPT_DISABLE()	{\
                                            alt_irq_context context;\
                                            NIOS2_READ_STATUS (context);\
                                            NIOS2_WRITE_STATUS (context & ~NIOS2_STATUS_PIE_MSK);\
                                        }

#elif defined (__KUNBUSPI__)
  #include <bsp/globalIntCtrl/bspGlobalIntCtrl_linuxRT.h>  

  #define	GLOBAL_INTERRUPT_ENABLE()		bspGlobalIntEnable()
  #define	GLOBAL_INTERRUPT_DISABLE()		bspGlobalIntDisable()

#elif defined (__KUNBUSPI_KERNEL__)
  #define	GLOBAL_INTERRUPT_ENABLE()
  #define	GLOBAL_INTERRUPT_DISABLE()
#elif defined (__SF2_GENERIC__)
  #include <m2sxxx.h>

  #define	GLOBAL_INTERRUPT_ENABLE()		__enable_irq()
  #define	GLOBAL_INTERRUPT_DISABLE()		__disable_irq()
#else
  #define	GLOBAL_INTERRUPT_ENABLE()		__enable_irq()
  #define	GLOBAL_INTERRUPT_DISABLE()		__disable_irq()
#endif



#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
}
#endif

#endif  // BSPGLOBALINTCTRL_H_INC



//+=============================================================================================
//|
//|    Topic
//|
//+---------------------------------------------------------------------------------------------
//|
//|    File-ID:    $Id: $
//|    Location:   $URL: $
//|    Copyright:  KUNBUS GmbH
//|
//+=============================================================================================

#ifndef MGATEDECODE_H_INC
#define MGATEDECODE_H_INC

#ifdef __cplusplus
extern "C" {
#endif

#define MGATE_MODUL_PC    0
#define MGATE_MODUL_LEFT  1
#define MGATE_MODUL_RIGHT 2

extern void BSP_initMgateDecode (void);
#ifdef __STM32GENERIC__
extern INT8U BSP_getMgateDecode (void);
#else
#define BSP_getMgateDecode() MGATE_MODUL_LEFT
#endif // __STM32GENERIC__

#ifdef __cplusplus
}
#endif


#endif  // MGATEDECODE_H_INC

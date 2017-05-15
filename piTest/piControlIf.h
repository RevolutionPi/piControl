/*!
 *
 * Project: Pi Control
 * (C)    : KUNBUS GmbH, Wachhausstr. 5a, D-76227 Karlsruhe
 *
 *
 * \file piControlIf.c
 *
 * \brief PI Control Interface
 *
 *
 */

#ifndef PICONTROLIF_H_
#define PICONTROLIF_H_

/******************************************************************************/
/********************************  Includes  **********************************/
/******************************************************************************/

#include <stdint.h>
#include <piControl.h>


/******************************************************************************/
/*********************************  Types  ************************************/
/******************************************************************************/

extern int PiControlHandle_g;


/******************************************************************************/
/*******************************  Prototypes  *********************************/
/******************************************************************************/
#ifdef __cplusplus
extern "C" { 
#endif 

int piControlReset(void);
int piControlRead(uint32_t Offset, uint32_t Length, uint8_t *pData);
int piControlWrite(uint32_t Offset, uint32_t Length, uint8_t *pData);
int piControlGetDeviceInfo(SDeviceInfo *pDev);
int piControlGetDeviceInfoList(SDeviceInfo *pDev);
int piControlGetBitValue(SPIValue *pSpiValue);
int piControlSetBitValue(SPIValue *pSpiValue);
int piControlGetVariableInfo(SPIVariable *pSpiVariable);
int piControlFindVariable(const char *name);
int piControlResetCounter(int address, int bitfield);
int piControlWaitForEvent(void);
int piControlUpdateFirmware(void);
#ifdef KUNBUS_TEST
int piControlIntMsg(int msg, unsigned char *data, int size);
#endif
    
void piControlClose(void);
    
#ifdef __cplusplus
}
#endif 

#endif /* PICONTROLIF_H_ */

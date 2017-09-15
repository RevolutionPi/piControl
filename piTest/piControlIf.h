/*!
 *
 * Project: Pi Control
 *
 * MIT License
 *
 * Copyright (C) 2017 : KUNBUS GmbH, Heerweg 15C, 73370 Denkendorf, Germany
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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
int piControlUpdateFirmware(uint32_t addr_p);
#ifdef KUNBUS_TEST
int piControlIntMsg(int msg, unsigned char *data, int size);
int piControlSetSerial(int addr, int serial);
#endif

void piControlClose(void);

#ifdef __cplusplus
}
#endif

#endif /* PICONTROLIF_H_ */

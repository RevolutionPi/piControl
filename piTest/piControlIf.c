/*!
 *
 * Project: Pi Control
 * Demo source code for usage of piControl driver
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

/******************************************************************************/
/********************************  Includes  **********************************/
/******************************************************************************/

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <stdio.h>
#include <string.h>

#include "piControlIf.h"

#include "piControl.h"
#ifdef KUNBUS_TEST
#include <IoProtocol.h>
#endif

/******************************************************************************/
/******************************  Global Vars  *********************************/
/******************************************************************************/

int PiControlHandle_g = -1;

/******************************************************************************/
/*******************************  Functions  **********************************/
/******************************************************************************/


/***********************************************************************************/
/*!
 * @brief Open Pi Control Interface
 *
 * Initialize the Pi Control Interface
 *
 ************************************************************************************/
void piControlOpen(void)
{
    /* open handle if needed */
    if (PiControlHandle_g < 0)
    {
	PiControlHandle_g = open(PICONTROL_DEVICE, O_RDWR);
    }
}


/***********************************************************************************/
/*!
 * @brief Close Pi Control Interface
 *
 * Clsoe the Pi Control Interface
 *
 ************************************************************************************/
void piControlClose(void)
{
    /* open handle if needed */
    if (PiControlHandle_g > 0)
    {
	close(PiControlHandle_g);
	PiControlHandle_g = -1;
    }
}


/***********************************************************************************/
/*!
 * @brief Reset Pi Control Interface
 *
 * Initialize the Pi Control Interface
 *
 ************************************************************************************/
int piControlReset(void)
{
    piControlOpen();

    if (PiControlHandle_g < 0)
	return -ENODEV;

    if (ioctl(PiControlHandle_g, KB_RESET, NULL) < 0)
	return errno;

    return 0;
}


/***********************************************************************************/
/*!
 * @brief Wait for Reset of Pi Control Interface
 *
 * Wait for Reset of Pi Control Interface
 *
 ************************************************************************************/
int piControlWaitForEvent(void)
{
    int event;

    piControlOpen();

    if (PiControlHandle_g < 0)
	return -ENODEV;

    if (ioctl(PiControlHandle_g, KB_WAIT_FOR_EVENT, &event) < 0)
	return errno;

    return event;
}


/***********************************************************************************/
/*!
 * @brief Get Processdata
 *
 * Gets Processdata from a specific position
 *
 * @param[in]   Offset
 * @param[in]   Length
 * @param[out]  pData
 *
 * @return Number of Bytes read or error if negative
 *
 ************************************************************************************/
int piControlRead(uint32_t Offset, uint32_t Length, uint8_t *pData)
{
    int BytesRead = 0;

    piControlOpen();

    if (PiControlHandle_g < 0)
	return -ENODEV;

    /* seek */
    if (lseek(PiControlHandle_g, Offset, SEEK_SET) < 0)
    {
	return errno;
    }

    /* read */
    BytesRead = read(PiControlHandle_g, pData, Length);
    if (BytesRead < 0)
    {
	return errno;
    }

    return BytesRead;
}


/***********************************************************************************/
/*!
 * @brief Set Processdata
 *
 * Writes Processdata at a specific position
 *
 * @param[in]   Offset
 * @param[in]   Length
 * @param[out]  pData
 *
 * @return Number of Bytes written or error if negative
 *
 ************************************************************************************/
int piControlWrite(uint32_t Offset, uint32_t Length, uint8_t *pData)
{
    int BytesWritten = 0;

    piControlOpen();

    if (PiControlHandle_g < 0)
	return -ENODEV;

    /* seek */
    if (lseek(PiControlHandle_g, Offset, SEEK_SET) < 0)
    {
	return errno;
    }

    /* Write */
    BytesWritten = write(PiControlHandle_g, pData, Length);
    if (BytesWritten < 0)
    {
	return errno;
    }

    return BytesWritten;
}


/***********************************************************************************/
/*!
 * @brief Get Device Info
 *
 * Get Description of connected devices.
 *
 * @param[in/out]   Pointer to one element of type SDeviceInfo.
 *
 * @return 0 on success
 *
 ************************************************************************************/
int piControlGetDeviceInfo(SDeviceInfo *pDev)
{
    piControlOpen();

    if (PiControlHandle_g < 0)
	return -ENODEV;

    if (ioctl(PiControlHandle_g, KB_GET_DEVICE_INFO, pDev) < 0)
	return errno;

    return 0;
}


/***********************************************************************************/
/*!
 * @brief Get Device Info List
 *
 * Get Description of connected devices.
 *
 * @param[in/out]   Pointer to an array of 20 entries of type SDeviceInfo.
 *
 * @return Number of detected devices
 *
 ************************************************************************************/
int piControlGetDeviceInfoList(SDeviceInfo *pDev)
{
    piControlOpen();

    if (PiControlHandle_g < 0)
	return -ENODEV;

    int cnt = ioctl(PiControlHandle_g, KB_GET_DEVICE_INFO_LIST, pDev);
    if (cnt < 0)
	return errno;

    return cnt;
}

/***********************************************************************************/
/*!
 * @brief Get Bit Value
 *
 * Get the value of one bit in the process image.
 *
 * @param[in/out]   Pointer to SPIValue.
 *
 * @return 0 or error if negative
 *
 ************************************************************************************/
int piControlGetBitValue(SPIValue *pSpiValue)
{
    piControlOpen();

    if (PiControlHandle_g < 0)
	return -ENODEV;

    pSpiValue->i16uAddress += pSpiValue->i8uBit / 8;
    pSpiValue->i8uBit %= 8;
    
    if (ioctl(PiControlHandle_g, KB_GET_VALUE, pSpiValue) < 0)
	return errno;

    return 0;
}

/***********************************************************************************/
/*!
 * @brief Set Bit Value
 *
 * Set the value of one bit in the process image.
 *
 * @param[in/out]   Pointer to SPIValue.
 *
 * @return 0 or error if negative
 *
 ************************************************************************************/
int piControlSetBitValue(SPIValue *pSpiValue)
{
    piControlOpen();

    if (PiControlHandle_g < 0)
	return -ENODEV;

    pSpiValue->i16uAddress += pSpiValue->i8uBit / 8;
    pSpiValue->i8uBit %= 8;
    
    if (ioctl(PiControlHandle_g, KB_SET_VALUE, pSpiValue) < 0)
	return errno;

    return 0;
}

/***********************************************************************************/
/*!
 * @brief Get Variable Info
 *
 * Get the info for a variable.
 *
 * @param[in/out]   Pointer to SPIVariable.
 *
 * @return 0 or error if negative
 *
 ************************************************************************************/
int piControlGetVariableInfo(SPIVariable *pSpiVariable)
{
    piControlOpen();

    if (PiControlHandle_g < 0)
	return -ENODEV;

    if (ioctl(PiControlHandle_g, KB_FIND_VARIABLE, pSpiVariable) < 0)
	return errno;

    return 0;
}

/***********************************************************************************/
/*!
 * @brief Get Variable offset by name
 *
 * Get the offset of a variable in the process image. This does NOT work for variable of type bool.
 *
 * @param[in]   pointer to string with name of variable
 *
 * @return      >= 0    offset
		< 0     in case of error
 *
 ************************************************************************************/
int piControlFindVariable(const char *name)
{
    int ret;
    SPIVariable var;

    piControlOpen();

    if (PiControlHandle_g < 0)
	return -ENODEV;

    strncpy(var.strVarName, name, sizeof(var.strVarName));
    var.strVarName[sizeof(var.strVarName) - 1] = 0;

    ret = ioctl(PiControlHandle_g, KB_FIND_VARIABLE, &var);
    if (ret < 0)
    {
	//printf("could not find variable '%s' in configuration.\n", var.strVarName);
    }
    else
    {
	//printf("Variable '%s' is at offset %d and %d bits long\n", var.strVarName, var.i16uAddress, var.i16uLength);
	ret = var.i16uAddress;
    }
    return ret;
}


int piControlResetCounter(int address, int bitfield)
{
    SDIOResetCounter tel;
    int ret;

    piControlOpen();

    if (PiControlHandle_g < 0)
	return -ENODEV;

    tel.i8uAddress = address;
    tel.i16uBitfield = bitfield;

    ret = ioctl(PiControlHandle_g, KB_DIO_RESET_COUNTER, &tel);
    if (ret < 0)
    {
	perror("Counter reset not possible");
    }
    return ret;
}


int piControlUpdateFirmware(uint32_t addr_p)
{
    int ret;
    char cMsg[255];

    piControlOpen();

    if (PiControlHandle_g < 0)
	return -ENODEV;

    if (addr_p == 0)
    ret = ioctl(PiControlHandle_g, KB_UPDATE_DEVICE_FIRMWARE, NULL);
    else
        ret = ioctl(PiControlHandle_g, KB_UPDATE_DEVICE_FIRMWARE, &addr_p);

    if (ret < 0)
    {
	perror("Firmware update failed");
    }
    if (ioctl(PiControlHandle_g, KB_GET_LAST_MESSAGE, cMsg) == 0 && cMsg[0])
	printf("%s\n", cMsg);
    return ret;
}



#ifdef KUNBUS_TEST
int piControlIntMsg(int msg, unsigned char *data, int size)
{
    int ret;
    SIOGeneric telegram;

    piControlOpen();

    if (PiControlHandle_g < 0)
	return -ENODEV;

    telegram.uHeader.sHeaderTyp1.bitAddress = 32;
    telegram.uHeader.sHeaderTyp1.bitIoHeaderType = 0;
    telegram.uHeader.sHeaderTyp1.bitReqResp = 0;
    telegram.uHeader.sHeaderTyp1.bitLength = size;
    telegram.uHeader.sHeaderTyp1.bitCommand = msg;
    if (data != NULL)
	memcpy(telegram.ai8uData, data, size);

    ret = ioctl(PiControlHandle_g, KB_INTERN_IO_MSG, &telegram);
    if (ret < 0)
    {
	printf("error %d in ioctl\n", ret);
    }
    else
    {
	int i;
	printf("got response with %d bytes\n", telegram.uHeader.sHeaderTyp1.bitLength);
	for (i = 0; i < telegram.uHeader.sHeaderTyp1.bitLength; i++)
	{
	    printf("%02x ", telegram.ai8uData[i]);
	}
	printf("\n");
    }
    return ret;
}

int piControlSetSerial(int addr, int serial)
{
    int ret;
    INT32U data[2];
    data[0] = addr;
    data[1] = serial;
    
    piControlOpen();

    if (PiControlHandle_g < 0)
        return -ENODEV;

    ret = ioctl(PiControlHandle_g, KB_INTERN_SET_SERIAL_NUM, data);
    return ret;
}

#endif




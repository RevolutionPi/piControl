/*!
 *
 * Project: Pi Control
 * Demo source code for usage of piControl driver
 *
 *   Copyright (C) 2016 : KUNBUS GmbH, Heerweg 15C, 73370 Denkendorf, Germany
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
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

#include "piControlIf.h"

#include "piControl.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <stdio.h>
#include <string.h>


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
        return -errno;

    // do some ioctls
    if (ioctl(PiControlHandle_g, KB_RESET, NULL) < 0)
        return -errno;

    return 0;
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
        return -1;

    /* seek */
    if (lseek(PiControlHandle_g, Offset, SEEK_SET) < 0)
    {
        return -2;
    }

    /* read */
    BytesRead = read(PiControlHandle_g, pData, Length);
    if (BytesRead != Length)
    {
        return -3;
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
        return -1;

    /* seek */
    if (lseek(PiControlHandle_g, Offset, SEEK_SET) < 0)
    {
        return -2;
    }

    /* Write */
    BytesWritten = write(PiControlHandle_g, pData, Length);
    if (BytesWritten != Length)
    {
        return -3;
    }

    return BytesWritten;
}


/***********************************************************************************/
/*!
 * @brief Get Device Info
 *
 * Get Description of connected devices.
 *
 * @param[in/out]   Pointer to an array of 20 entries of type SDeviceInfo.
 *
 * @return Number of detected devices
 *
 ************************************************************************************/
int piControlGetDeviceInfo(SDeviceInfo *pDev)
{
    piControlOpen();

    if (PiControlHandle_g < 0)
        return -errno;

    return ioctl(PiControlHandle_g, KB_GET_DEVICE_INFO, pDev);
}

int piControlGetDeviceInfoList(SDeviceInfo *pDev)
{
    piControlOpen();

    if (PiControlHandle_g < 0)
        return -errno;

    return ioctl(PiControlHandle_g, KB_GET_DEVICE_INFO_LIST, pDev);
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
        return -1;

    return ioctl(PiControlHandle_g, KB_GET_VALUE, pSpiValue);
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
        return -1;

    return ioctl(PiControlHandle_g, KB_SET_VALUE, pSpiValue);
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
        return -errno;

    return ioctl(PiControlHandle_g, KB_FIND_VARIABLE, pSpiVariable);
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
        return -errno;

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

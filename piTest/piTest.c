/*!
 *
 * Project: piTest
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
 * \file piTest.c
 *
 * \brief PI Control Test Program
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <termios.h>
#include <getopt.h>
#include <time.h>
#include <sys/types.h>

#include "piControlIf.h"
#include "piControl.h"


/***********************************************************************************/
/*!
 * @brief Get message text for read error
 *
 * Get the message text for an error on read from control process.
 *
 * @param[in]   Error number.
 *
 * @return Pointer to the error message
 *
 ************************************************************************************/
char *getReadError(int error)
{
    static char *ReadError[] =
    {
        "Cannot connect to control process",
        "Offset seek error",
        "Cannot read from control process",
        "Unknown error"
    };
    switch (error)
    {
    case -1:
        return ReadError[0];
        break;
    case -2:
        return ReadError[1];
        break;
    case -3:
        return ReadError[2];
        break;
    default:
        return ReadError[3];
        break;
    }
}

/***********************************************************************************/
/*!
 * @brief Get message text for write error
 *
 * Get the message text for an error on write to control process.
 *
 * @param[in]   Error number.
 *
 * @return Pointer to the error message
 *
 ************************************************************************************/
char *getWriteError(int error)
{
    static char *WriteError[] =
    {
        "Cannot connect to control process",
        "Offset seek error",
        "Cannot write to control process",
        "Unknown error"
    };
    switch (error)
    {
    case -1:
        return WriteError[0];
        break;
    case -2:
        return WriteError[1];
        break;
    case -3:
        return WriteError[2];
        break;
    default:
        return WriteError[3];
        break;
    }
}

/***********************************************************************************/
/*!
 * @brief Get module type as string
 *
 *
 * @param[in]   module type number
 *
 * @return      Pointer to the string with the name
 *
 ************************************************************************************/
char *getModuleName(uint16_t moduletype)
{
    switch (moduletype)
    {
    case 95:
        return "RevPi Core";
        break;
    case 96:
        return "RevPi DIO";
        break;
    case 97:
        return "RevPi DI";
        break;
    case 98:
        return "RevPi DO";
        break;

    case 100:
        return "Gateway DMX";
        break;
    case 71:
        return "Gateway CANopen";
        break;
    case 73:
        return "Gateway DeviceNet";
        break;
    case 74:
        return "Gateway EtherCAT";
        break;
    case 75:
        return "Gateway EtherNet/IP";
        break;
    case 93:
        return "Gateway ModbusTCP";
        break;
    case 76:
        return "Gateway Powerlink";
        break;
    case 77:
        return "Gateway Profibus";
        break;
    case 79:
        return "Gateway Profinet IRT";
        break;
    case 81:
        return "Gateway SercosIII";
        break;

    default :
        return "unknown moduletype";
        break;
    }
}

/***********************************************************************************/
/*!
 * @brief Show device list
 *
 * Show all devices connected to control process and print their info data
 *
 ************************************************************************************/
void showDeviceList(void)
{
    int devcount;
    int dev;
    SDeviceInfo asDevList[255];

    // Get device info
    devcount = piControlGetDeviceInfoList(asDevList);
    if (devcount < 0)
    {
        printf("Cannot retrieve device list: %s\n", strerror(devcount));
        return;
    }

    printf("Found %d devices:\n\n", devcount);

    for (dev = 0; dev < devcount; dev++)
    {
        // Show device number, address and module type
        printf("Address: %d module type: %d (0x%x) %s\n", asDevList[dev].i8uAddress,
            asDevList[dev].i16uModuleType, asDevList[dev].i16uModuleType, getModuleName(asDevList[dev].i16uModuleType & PICONTROL_USER_MODULE_MASK));

        if (asDevList[dev].i8uActive)
        {
            printf("Module is present\n");
        }
        else
        {
            if (asDevList[dev].i16uModuleType & PICONTROL_USER_MODULE_TYPE)
            {
                printf("Module is NOT present, data is NOT available!!!\n");
            }
            else
            {
                printf("Module is present, but NOT CONFIGURED!!!\n");
            }
        }

        // Show offset and length of input section in process image
        printf("     input offset: %d length: %d\n", asDevList[dev].i16uInputOffset, asDevList[dev].i16uInputLength);

        // Show offset and length of output section in process image
        printf("    output offset: %d length: %d\n", asDevList[dev].i16uOutputOffset, asDevList[dev].i16uOutputLength);
        printf("\n");
    }
}

/***********************************************************************************/
/*!
 * @brief Read data
 *
 * Read <length> bytes at a specific offset.
 *
 * @param[in]   Offset
 * @param[in]   Length
 *
 ************************************************************************************/
void readData(uint16_t offset, uint16_t length)
{
    int rc;
    uint8_t *pValues;
    int val;

    // Get memory for the values
    pValues = malloc(length);
    if (pValues == NULL)
    {
        printf("Not enough memory\n");
        return;
    }

    while (1)
    {
        rc = piControlRead(offset, length, pValues);
        if (rc < 0)
        {
            printf("read error %s\n", getReadError(rc));
        }
        else
        {
            for (val = 0; val < length; val++)
            {
                printf("%02x ", pValues[val]);
                if ((val % 16) == 15)
                    printf("\n");
            }
            printf("\n");
        }
        sleep(1);
    }
}

/***********************************************************************************/
/*!
 * @brief Read variable value
 *
 * Read the value of a variable from process image
 *
 * @param[in]   Variable name
 *
 ************************************************************************************/
void readVariableValue(char *pszVariableName)
{
    int rc;
    SPIVariable sPiVariable;
    SPIValue sPIValue;
    uint8_t i8uValue;
    uint16_t i16uValue;
    uint32_t i32uValue;

    strncpy(sPiVariable.strVarName, pszVariableName, sizeof(sPiVariable.strVarName));
    rc = piControlGetVariableInfo(&sPiVariable);
    if (rc < 0)
    {
        printf("Cannot find variable '%s'\n", pszVariableName);
        return;
    }
    if (sPiVariable.i16uLength == 1)
    {
        sPIValue.i16uAddress = sPiVariable.i16uAddress;
        sPIValue.i8uBit = sPiVariable.i8uBit;
        while (1)
        {
            rc = piControlGetBitValue(&sPIValue);
            if (rc < 0)
                printf("Get bit error\n");
            else
                printf("Get bit value: %d\n", sPIValue.i8uValue);
            sleep(1);
        }
    }
    else if (sPiVariable.i16uLength == 8)
    {
        while (1)
        {
            rc = piControlRead(sPiVariable.i16uAddress, 1, (uint8_t*)&i8uValue);
            if (rc < 0)
                printf("Read error\n");
            else
                printf("Value of %s: %02x hex (=%d dez)\n", pszVariableName, i8uValue, i8uValue);
            sleep(1);
        }
    }
    else if (sPiVariable.i16uLength == 16)
    {
        while (1)
        {
            rc = piControlRead(sPiVariable.i16uAddress, 2, (uint8_t*)&i16uValue);
            if (rc < 0)
                printf("Read error\n");
            else
                printf("Value of %s: %04x hex (=%d dez)\n", pszVariableName, i16uValue, i16uValue);
            sleep(1);
        }
    }
    else if (sPiVariable.i16uLength == 32)
    {
        while (1)
        {
            rc = piControlRead(sPiVariable.i16uAddress, 4, (uint8_t*)&i32uValue);
            if (rc < 0)
                printf("Read error\n");
            else
                printf("Value of %s: %08x hex (=%d dez)\n", pszVariableName, i32uValue, i32uValue);
            sleep(1);
        }
    }
    else
        printf("Could not read variable %s. Internal Error\n", pszVariableName);
}

/***********************************************************************************/
/*!
 * @brief Write data to process image
 *
 * Write <length> bytes to a specific offset of process image.
 *
 * @param[in]   Offset
 * @param[in]   Length
 * @param[in]   Value to write
 *
 ************************************************************************************/
void writeData(int offset, int length, unsigned long i32uValue)
{
    int rc;

    if (length != 1 && length != 2 && length != 4)
    {
        printf("Length must be one of 1|2|4\n");
        return;
    }
    rc = piControlWrite(offset, length, (uint8_t *)&i32uValue);
    if (rc < 0)
    {
        printf("write error %s\n", getWriteError(rc));
    }
    else
    {
        printf("Write value %lx hex (=%ld dez) to offset %d.\n", i32uValue, i32uValue, offset);
    }
}

/***********************************************************************************/
/*!
 * @brief Write variable value
 *
 * Write a value to a variable.
 *
 * @param[in]   Variable name
 * @param[in]   Value to write
 *
 ************************************************************************************/
void writeVariableValue(char *pszVariableName, uint32_t i32uValue)
{
    int rc;
    SPIVariable sPiVariable;
    SPIValue sPIValue;
    uint8_t i8uValue;
    uint16_t i16uValue;

    strncpy(sPiVariable.strVarName, pszVariableName, sizeof(sPiVariable.strVarName));
    rc = piControlGetVariableInfo(&sPiVariable);
    if (rc < 0)
    {
        printf("Cannot find variable '%s'\n", pszVariableName);
        return;
    }

    if (sPiVariable.i16uLength == 1)
    {
        sPIValue.i16uAddress = sPiVariable.i16uAddress;
        sPIValue.i8uBit = sPiVariable.i8uBit;
        sPIValue.i8uValue = i32uValue;
        rc = piControlSetBitValue(&sPIValue);
        if (rc < 0)
            printf("Set bit error %s\n", getWriteError(rc));
        else
            printf("Set bit %d on byte at offset %d. Value %d\n", sPIValue.i8uBit, sPIValue.i16uAddress, sPIValue.i8uValue);
    }
    else if (sPiVariable.i16uLength == 8)
    {
        i8uValue = i32uValue;
        rc = piControlWrite(sPiVariable.i16uAddress, 1, (uint8_t *)&i8uValue);
        if (rc < 0)
            printf("Write error %s\n", getWriteError(rc));
        else
            printf("Write value %d dez (=%02x hex) to offset %d.\n", i8uValue, i8uValue, sPiVariable.i16uAddress);
    }
    else if (sPiVariable.i16uLength == 16)
    {
        i16uValue = i32uValue;
        rc = piControlWrite(sPiVariable.i16uAddress, 2, (uint8_t *)&i16uValue);
        if (rc < 0)
            printf("Write error %s\n", getWriteError(rc));
        else
            printf("Write value %d dez (=%04x hex) to offset %d.\n", i16uValue, i16uValue, sPiVariable.i16uAddress);
    }
    else if (sPiVariable.i16uLength == 32)
    {
        rc = piControlWrite(sPiVariable.i16uAddress, 4, (uint8_t *)&i32uValue);
        if (rc < 0)
            printf("Write error %s\n", getWriteError(rc));
        else
            printf("Write value %d dez (=%08x hex) to offset %d.\n", i32uValue, i32uValue, sPiVariable.i16uAddress);
    }
}

/***********************************************************************************/
/*!
 * @brief Set a bit
 *
 * Write a bit at a specific offset to a device.
 *
 * @param[in]   Offset
 * @param[in]   Bit number (0 - 7)
 * @param[in]   Value to write (0/1)
 *
 ************************************************************************************/
void setBit(int offset, int bit, int value)
{
    char c;
    int rc;
    SPIValue sPIValue;

    // Check bit
    if (bit < 0 || bit > 7)
    {
        printf("Wrong bit number. Try 0 - 7\n");
        return;
    }

    // Check value
    if (value != 0 && value != 1)
    {
        printf("Wrong value. Try 0/1\n");
        return;
    }

    sPIValue.i16uAddress = offset;
    sPIValue.i8uBit = bit;
    sPIValue.i8uValue = value;
    // Set bit
    rc = piControlSetBitValue(&sPIValue);
    if (rc < 0)
    {
        printf("Set bit error %s\n", getWriteError(rc));
    }
    else
    {
        printf("Set bit %d on byte at offset %d. Value %d\n", bit, offset, value);
    }
}

/***********************************************************************************/
/*!
 * @brief Get a bit
 *
 * Read a bit at a specific offset from a device.
 *
 * @param[in]   Offset
 * @param[in]   Bit number (0 - 7)
 *
 ************************************************************************************/
void getBit(int offset, int bit)
{
    int rc;
    int value;
    SPIValue sPIValue;

    // Check bit
    if (bit < 0 || bit > 7)
    {
        printf("Wrong bit number. Try 0 - 7\n");
        return;
    }

    sPIValue.i16uAddress = offset;
    sPIValue.i8uBit = bit;
    // Get bit
    rc = piControlGetBitValue(&sPIValue);
    if (rc < 0)
    {
        printf("Get bit error\n");
    }
    else
    {
        printf("Get bit %d at offset %d. Value %d\n", bit, offset, sPIValue.i8uValue);
    }
}

/***********************************************************************************/
/*!
 * @brief Show infos for a specific variable name from process image
 *
 * @param[in]   Variable name
 *
 ************************************************************************************/
void showVariableInfo(char *pszVariableName)
{
    int rc;
    SPIVariable sPiVariable;

    strncpy(sPiVariable.strVarName, pszVariableName, sizeof(sPiVariable.strVarName));
    rc = piControlGetVariableInfo(&sPiVariable);
    if (rc < 0)
    {
        printf("Cannot read variable info\n");
    }
    else
    {
        printf("variable name: %s\n", sPiVariable.strVarName);
        printf("       offset: %d\n", sPiVariable.i16uAddress);
        printf("       length: %d\n", sPiVariable.i16uLength);
        printf("          bit: %d\n", sPiVariable.i8uBit);
    }
}

/***********************************************************************************/
/*!
 * @brief Shows help for this program
 *
 * @param[in]   Program name
 *
 ************************************************************************************/
void printHelp(char *programname)
{
    printf("Usage: %s [OPTION]\n", programname);
    printf("- Shows infos from RevPiCore control process\n");
    printf("- Reads values of RevPiCore process image\n");
    printf("- Writes values to RevPiCore process image\n");
    printf("\n");
    printf("Options:\n");
    printf("                 -d: Get device list.\n");
    printf("\n");
    printf("  -v <variablename>: Shows infos for a variable.\n");
    printf("\n");
    printf("  -r <variablename>: Reads value of a variable.\n");
    printf("                     E.g.: -w Input_001:\n");
    printf("                     Read value from variable 'Input_001'.\n");
    printf("\n");
    printf("           -r <o,l>: Reads <l> bytes at offset <o>.\n");
    printf("                     E.g.: -r 1188,16:\n");
    printf("                     Read 16 bytes at offset 1188.\n");
    printf("\n");
    printf("-w <variablename,v>: Writes value <v> to variable.\n");
    printf("                     E.g.: -w Output_001,23:\n");
    printf("                     Write value 23 dez (=17 hex) to variable 'Output_001'.\n");
    printf("\n");
    printf("         -w <o,l,v>: Writes <l> bytes with value <v> (as hex) to offset <o>.\n");
    printf("                     length should be one of 1|2|4.\n");
    printf("                     E.g.: -w 0,4,31224205:\n");
    printf("                     Write value 31224205 hex (=824328709 dez) to offset 0.\n");
    printf("\n");
    printf("           -g <o,b>: Gets bit number <b> (0-7) from byte at offset <o>.\n");
    printf("                     E.g.: -b 0,5:\n");
    printf("                     Get bit 5 from byte at offset 0.\n");
    printf("\n");
    printf("       -s <o,b,0/1>: Sets 0/1 to bit <b> (0-7) of byte at offset <o>.\n");
    printf("                     E.g.: -b 0,5,1:\n");
    printf("                     Set bit 5 to 1 of byte at offset 0.\n");
    printf("\n");
    printf("                 -x: Reset control process.\n");
}

/***********************************************************************************/
/*!
 * @brief main program
 *
 * @param[in]   Program name and arguments
 *
 ************************************************************************************/
int main(int argc, char *argv[])
{
    int c;
    int rc;
    int offset;
    int length;
    int bit;
    unsigned long value;
    char szVariableName[256];
    char *pszTok, *progname;

    progname = strrchr(argv[0], '/');
    if (!progname)
    {
        progname = argv[0];
    }

    if (!strcmp(progname, "piControlReset"))
    {
        rc = piControlReset();
        if (rc)
        {
            printf("%s\n", strerror(rc));
        }
        return rc;
    }

    if (argc == 1)
    {
        printHelp(progname);
        return 0;
    }

    // Scan argument
    while ((c = getopt(argc, argv, "dv:r:w:s:g:xh")) != -1)
    {
        switch (c)
        {
        case 'd':
            showDeviceList();
            break;

        case 'v':
            if (strlen(optarg) > 0)
            {
                showVariableInfo(optarg);
            }
            else
            {
                printf("No variable name\n");
            }
            break;

        case 'r':
            rc = sscanf(optarg, "%d,%d", &offset, &length);
            if (rc == 2)
            {
                readData(offset, length);
                return 0;
            }
            rc = sscanf(optarg, "%s", szVariableName);
            if(rc == 1)
            {
                readVariableValue(szVariableName);
                return 0;
            }
            printf("Wrong arguments for read function\n");
            printf("1.) Try '-r variablename'\n");
            printf("2.) Try '-r offset,length' (without spaces)\n");
            break;

        case 'w':
            rc = sscanf(optarg, "%d,%d,%ld", &offset, &length, &value);
            if (rc == 3)
            {
                writeData(offset, length, value);
                return 0;
            }
            pszTok = strtok(optarg, ",");
            if (pszTok != NULL)
            {
                strncpy(szVariableName, pszTok, sizeof(szVariableName));
                pszTok = strtok(NULL, ",");
                if (pszTok != NULL)
                {
                    value = strtol(pszTok, NULL, 10);
                    writeVariableValue(szVariableName, value);
                    return 0;
                }
            }
            printf("Wrong arguments for write function\n");
            printf("1.) Try '-w variablename,value' (without spaces)\n");
            printf("2.) Try '-w offset,length,value' (without spaces)\n");
            break;

        case 's':
            rc = sscanf(optarg, "%d,%d,%ld", &offset, &bit, &value);
            if (rc != 3)
            {
                printf("Wrong arguments for set bit function\n");
                printf("Try '-s offset,bit,value' (without spaces)\n");
                return 0;
            }
            setBit(offset, bit, value);
            break;

        case 'g':
            rc = sscanf(optarg, "%d,%d", &offset, &bit);
            if (rc != 2)
            {
                printf("Wrong arguments for get bit function\n");
                printf("Try '-g offset,bit' (without spaces)\n");
                return 0;
            }
            getBit(offset, bit);
            break;

        case 'x':
            piControlReset();
            break;

        case 'h':
        default:
            printHelp(progname);
            break;
        }
    }
    return 0;
}

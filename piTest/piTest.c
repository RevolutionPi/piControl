/*!
 *
 * Project: piTest
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
#include <stdbool.h>

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
	static char *ReadError[] = {
		"Cannot connect to control process",
		"Offset seek error",
		"Cannot read from control process",
		"Unknown error"
	};
	switch (error) {
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
	static char *WriteError[] = {
		"Cannot connect to control process",
		"Offset seek error",
		"Cannot write to control process",
		"Unknown error"
	};
	switch (error) {
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
	switch (moduletype) {
	case 95:
		return "RevPi Core";
	case 96:
		return "RevPi DIO";
	case 97:
		return "RevPi DI";
	case 98:
		return "RevPi DO";
	case 103:
		return "RevPi AIO";
	case 104:
		return "RevPi Compact";
	case 105:
		return "RevPi Connect";
	case 109:
		return "RevPi CON CAN";
	case 110:
		return "RevPi CON M-Bus";
	case 111:
		return "RevPi CON BT";

	case PICONTROL_SW_MODBUS_TCP_SLAVE:
		return "ModbusTCP Slave Adapter";
	case PICONTROL_SW_MODBUS_RTU_SLAVE:
		return "ModbusRTU Slave Adapter";
	case PICONTROL_SW_MODBUS_TCP_MASTER:
		return "ModbusTCP Master Adapter";
	case PICONTROL_SW_MODBUS_RTU_MASTER:
		return "ModbusRTU Master Adapter";
	case PICONTROL_SW_PROFINET_CONTROLLER:
		return "Profinet Controller Adapter";
	case PICONTROL_SW_PROFINET_DEVICE:
		return "Profinet Device Adapter";
	case PICONTROL_SW_REVPI_SEVEN:
		return "RevPi7 Adapter";
	case PICONTROL_SW_REVPI_CLOUD:
		return "RevPi Cloud Adapter";

	case 71:
		return "Gateway CANopen";
	case 72:
		return "Gateway CC-Link";
	case 73:
		return "Gateway DeviceNet";
	case 74:
		return "Gateway EtherCAT";
	case 75:
		return "Gateway EtherNet/IP";
	case 76:
		return "Gateway Powerlink";
	case 77:
		return "Gateway Profibus";
	case 78:
		return "Gateway Profinet RT";
	case 79:
		return "Gateway Profinet IRT";
	case 80:
		return "Gateway CANopen Master";
	case 81:
		return "Gateway SercosIII";
	case 82:
		return "Gateway Serial";
	case 85:
		return "Gateway EtherCAT Master";
	case 92:
		return "Gateway ModbusRTU";
	case 93:
		return "Gateway ModbusTCP";
	case 100:
		return "Gateway DMX";

	default:
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
	SDeviceInfo asDevList[REV_PI_DEV_CNT_MAX];

	// Get device info
	devcount = piControlGetDeviceInfoList(asDevList);
	if (devcount < 0) {
		printf("Cannot retrieve device list: %s\n", strerror(-devcount));
		return;
	}

	printf("Found %d devices:\n\n", devcount);

	for (dev = 0; dev < devcount; dev++) {
		// Show device number, address and module type
		printf("Address: %d module type: %d (0x%x) %s V%d.%d\n", asDevList[dev].i8uAddress,
		       asDevList[dev].i16uModuleType, asDevList[dev].i16uModuleType,
		       getModuleName(asDevList[dev].i16uModuleType & PICONTROL_NOT_CONNECTED_MASK),
		       asDevList[dev].i16uSW_Major, asDevList[dev].i16uSW_Minor);

		if (asDevList[dev].i8uActive) {
			printf("Module is present\n");
		} else {
			if (asDevList[dev].i16uModuleType & PICONTROL_NOT_CONNECTED) {
				printf("Module is NOT present, data is NOT available!!!\n");
			} else {
				printf("Module is present, but NOT CONFIGURED!!!\n");
			}
		}

		// Show offset and length of input section in process image
		printf("     input offset: %d length: %d\n", asDevList[dev].i16uInputOffset,
		       asDevList[dev].i16uInputLength);

		// Show offset and length of output section in process image
		printf("    output offset: %d length: %d\n", asDevList[dev].i16uOutputOffset,
		       asDevList[dev].i16uOutputLength);
		printf("\n");
	}

	piShowLastMessage();
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
void readData(uint16_t offset, uint16_t length, bool cyclic, char format, bool quiet)
{
	int rc;
	uint8_t *pValues;
	int val;
	int line_len = 10;	// for decimal
	if (format == 'h')
		line_len = 16;
	else if (format == 'b')
		line_len = 4;

	// Get memory for the values
	pValues = malloc(length);
	if (pValues == NULL) {
		printf("Not enough memory\n");
		return;
	}

	do {
		rc = piControlRead(offset, length, pValues);
		if (rc < 0) {
			if (!quiet) {
				printf("read error %s\n", getReadError(rc));
			}
		} else {
			for (val = 0; val < length; val++) {
				if (format == 'h') {
					printf("%02x ", pValues[val]);
				} else if (format == 'b') {
					printf("%c%c%c%c%c%c%c%c ",
						pValues[val] & 0x80 ? '1' : '0',
						pValues[val] & 0x40 ? '1' : '0',
						pValues[val] & 0x20 ? '1' : '0',
						pValues[val] & 0x10 ? '1' : '0',
						pValues[val] & 0x08 ? '1' : '0',
						pValues[val] & 0x04 ? '1' : '0',
						pValues[val] & 0x02 ? '1' : '0',
						pValues[val] & 0x01 ? '1' : '0');
				} else if (format == 's') {
					uint16_t ui;
					int16_t *psi;
					ui = pValues[val] + (pValues[val + 1] << 8);
					psi = (int16_t *) & ui;
					printf("%6d ", *psi);
					val++;
				} else {
					printf("%3d ", pValues[val]);
				}
				if ((val % line_len) == (line_len - 1))
					printf("\n");
			}
			if ((val % line_len) != 0)
				printf("\n");
		}
		if (cyclic)
			sleep(1);
	} while (cyclic);
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
void readVariableValue(char *pszVariableName, bool cyclic, char format, bool quiet)
{
	int rc;
	SPIVariable sPiVariable;
	SPIValue sPIValue;
	uint8_t i8uValue;
	uint16_t i16uValue;
	uint32_t i32uValue;

	strncpy(sPiVariable.strVarName, pszVariableName, sizeof(sPiVariable.strVarName));
	rc = piControlGetVariableInfo(&sPiVariable);
	if (rc < 0) {
		printf("Cannot find variable '%s'\n", pszVariableName);
		return;
	}
	if (sPiVariable.i16uLength == 1) {
		sPIValue.i16uAddress = sPiVariable.i16uAddress;
		sPIValue.i8uBit = sPiVariable.i8uBit;

		do {
			rc = piControlGetBitValue(&sPIValue);
			if (rc < 0)
				printf("Get bit error\n");
			else {
				if (!quiet)
					printf("Bit value: %d\n", sPIValue.i8uValue);
				else
					printf("%d\n", sPIValue.i8uValue);
			}
			if (cyclic)
				sleep(1);
		} while (cyclic);
	} else if (sPiVariable.i16uLength == 8) {
		do {
			rc = piControlRead(sPiVariable.i16uAddress, 1, (uint8_t *) & i8uValue);
			if (rc < 0)
				printf("Read error\n");
			else {
				if (format == 'h') {
					if (!quiet)
						printf("1 Byte-Value of %s: %02x hex (=%d dez)\n", pszVariableName,
						       i8uValue, i8uValue);
					else
						printf("%x\n", i8uValue);
				} else if (format == 'b') {
					if (!quiet)
						printf("1 Byte-Value of %s: ", pszVariableName);

					printf("%c%c%c%c%c%c%c%c\n",
						i8uValue & 0x80 ? '1' : '0',
						i8uValue & 0x40 ? '1' : '0',
						i8uValue & 0x20 ? '1' : '0',
						i8uValue & 0x10 ? '1' : '0',
						i8uValue & 0x08 ? '1' : '0',
						i8uValue & 0x04 ? '1' : '0',
						i8uValue & 0x02 ? '1' : '0',
						i8uValue & 0x01 ? '1' : '0');
				} else {
					if (!quiet)
						printf("1 Byte-Value of %s: %d dez (=%02x hex)\n", pszVariableName,
						       i8uValue, i8uValue);
					else
						printf("%d\n", i8uValue);
				}
			}
			if (cyclic)
				sleep(1);
		} while (cyclic);
	} else if (sPiVariable.i16uLength == 16) {
		do {
			rc = piControlRead(sPiVariable.i16uAddress, 2, (uint8_t *) & i16uValue);
			if (rc < 0)
				printf("Read error\n");
			else {
				if (format == 'h') {
					if (!quiet)
						printf("2 Byte-Value of %s: %04x hex (=%d dez)\n", pszVariableName,
						       i16uValue, i16uValue);
					else
						printf("%x\n", i16uValue);
				} else if (format == 'b') {
					if (!quiet)
						printf("2 Byte-Value of %s: ", pszVariableName);

					printf("%c%c%c%c%c%c%c%c ",
					       i16uValue & 0x8000 ? '1' : '0',
					       i16uValue & 0x4000 ? '1' : '0',
					       i16uValue & 0x2000 ? '1' : '0',
					       i16uValue & 0x1000 ? '1' : '0',
					       i16uValue & 0x0800 ? '1' : '0',
					       i16uValue & 0x0400 ? '1' : '0',
					       i16uValue & 0x0200 ? '1' : '0',
					       i16uValue & 0x0100 ? '1' : '0');

					printf("%c%c%c%c%c%c%c%c\n",
					       i16uValue & 0x0080 ? '1' : '0',
					       i16uValue & 0x0040 ? '1' : '0',
					       i16uValue & 0x0020 ? '1' : '0',
					       i16uValue & 0x0010 ? '1' : '0',
					       i16uValue & 0x0008 ? '1' : '0',
					       i16uValue & 0x0004 ? '1' : '0',
					       i16uValue & 0x0002 ? '1' : '0',
					       i16uValue & 0x0001 ? '1' : '0');
				} else {
					if (!quiet)
						printf("2 Byte-Value of %s: %d dez (=%04x hex)\n", pszVariableName,
						       i16uValue, i16uValue);
					else
						printf("%d\n", i16uValue);
				}
			}
			if (cyclic)
				sleep(1);
		} while (cyclic);
	} else if (sPiVariable.i16uLength == 32) {
		do {
			rc = piControlRead(sPiVariable.i16uAddress, 4, (uint8_t *) & i32uValue);
			if (rc < 0)
				printf("Read error\n");
			else {
				if (format == 'h') {
					if (!quiet)
						printf("4 Byte-Value of %s: %08x hex (=%d dez)\n", pszVariableName,
						       i32uValue, i32uValue);
					else
						printf("%x\n", i32uValue);
				} else if (format == 'b') {
					if (!quiet)
						printf("4 Byte-Value of %s: ", pszVariableName);

					printf("%c%c%c%c%c%c%c%c ",
						i32uValue & 0x80000000 ? '1' : '0',
						i32uValue & 0x40000000 ? '1' : '0',
						i32uValue & 0x20000000 ? '1' : '0',
						i32uValue & 0x10000000 ? '1' : '0',
						i32uValue & 0x08000000 ? '1' : '0',
						i32uValue & 0x04000000 ? '1' : '0',
						i32uValue & 0x02000000 ? '1' : '0',
						i32uValue & 0x01000000 ? '1' : '0');

					printf("%c%c%c%c%c%c%c%c ",
						i32uValue & 0x00800000 ? '1' : '0',
						i32uValue & 0x00400000 ? '1' : '0',
						i32uValue & 0x00200000 ? '1' : '0',
						i32uValue & 0x00100000 ? '1' : '0',
						i32uValue & 0x00080000 ? '1' : '0',
						i32uValue & 0x00040000 ? '1' : '0',
						i32uValue & 0x00020000 ? '1' : '0',
						i32uValue & 0x00010000 ? '1' : '0');

					printf("%c%c%c%c%c%c%c%c ",
					       i32uValue & 0x00008000 ? '1' : '0',
					       i32uValue & 0x00004000 ? '1' : '0',
					       i32uValue & 0x00002000 ? '1' : '0',
					       i32uValue & 0x00001000 ? '1' : '0',
					       i32uValue & 0x00000800 ? '1' : '0',
					       i32uValue & 0x00000400 ? '1' : '0',
					       i32uValue & 0x00000200 ? '1' : '0',
						i32uValue & 0x00000100 ? '1' : '0');

					printf("%c%c%c%c%c%c%c%c\n",
					       i32uValue & 0x00000080 ? '1' : '0',
					       i32uValue & 0x00000040 ? '1' : '0',
					       i32uValue & 0x00000020 ? '1' : '0',
					       i32uValue & 0x00000010 ? '1' : '0',
					       i32uValue & 0x00000008 ? '1' : '0',
					       i32uValue & 0x00000004 ? '1' : '0',
					       i32uValue & 0x00000002 ? '1' : '0',
						i32uValue & 0x00000001 ? '1' : '0');
				} else {
					if (!quiet)
						printf("4 Byte-Value of %s: %d dez (=%08x hex)\n", pszVariableName,
						       i32uValue, i32uValue);
					else
						printf("%d\n", i32uValue);
				}
			}
			if (cyclic)
				sleep(1);
		} while (cyclic);
	} else
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

	if (length != 1 && length != 2 && length != 4) {
		printf("Length must be one of 1|2|4\n");
		return;
	}
	rc = piControlWrite(offset, length, (uint8_t *) & i32uValue);
	if (rc < 0) {
		printf("write error %s\n", getWriteError(rc));
	} else {
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
	if (rc < 0) {
		printf("Cannot find variable '%s'\n", pszVariableName);
		return;
	}

	if (sPiVariable.i16uLength == 1) {
		sPIValue.i16uAddress = sPiVariable.i16uAddress;
		sPIValue.i8uBit = sPiVariable.i8uBit;
		sPIValue.i8uValue = i32uValue;
		rc = piControlSetBitValue(&sPIValue);
		if (rc < 0)
			printf("Set bit error %s\n", getWriteError(rc));
		else
			printf("Set bit %d on byte at offset %d. Value %d\n", sPIValue.i8uBit, sPIValue.i16uAddress,
			       sPIValue.i8uValue);
	} else if (sPiVariable.i16uLength == 8) {
		i8uValue = i32uValue;
		rc = piControlWrite(sPiVariable.i16uAddress, 1, (uint8_t *) & i8uValue);
		if (rc < 0)
			printf("Write error %s\n", getWriteError(rc));
		else
			printf("Write value %d dez (=%02x hex) to offset %d.\n", i8uValue, i8uValue,
			       sPiVariable.i16uAddress);
	} else if (sPiVariable.i16uLength == 16) {
		i16uValue = i32uValue;
		rc = piControlWrite(sPiVariable.i16uAddress, 2, (uint8_t *) & i16uValue);
		if (rc < 0)
			printf("Write error %s\n", getWriteError(rc));
		else
			printf("Write value %d dez (=%04x hex) to offset %d.\n", i16uValue, i16uValue,
			       sPiVariable.i16uAddress);
	} else if (sPiVariable.i16uLength == 32) {
		rc = piControlWrite(sPiVariable.i16uAddress, 4, (uint8_t *) & i32uValue);
		if (rc < 0)
			printf("Write error %s\n", getWriteError(rc));
		else
			printf("Write value %d dez (=%08x hex) to offset %d.\n", i32uValue, i32uValue,
			       sPiVariable.i16uAddress);
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
	int rc;
	SPIValue sPIValue;

	// Check bit
	if (bit < 0 || bit > 7) {
		printf("Wrong bit number. Try 0 - 7\n");
		return;
	}
	// Check value
	if (value != 0 && value != 1) {
		printf("Wrong value. Try 0/1\n");
		return;
	}

	sPIValue.i16uAddress = offset;
	sPIValue.i8uBit = bit;
	sPIValue.i8uValue = value;
	// Set bit
	rc = piControlSetBitValue(&sPIValue);
	if (rc < 0) {
		printf("Set bit error %s\n", getWriteError(rc));
	} else {
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
void getBit(int offset, int bit, bool quiet)
{
	int rc;
	SPIValue sPIValue;

	// Check bit
	if (bit < 0 || bit > 7) {
		printf("Wrong bit number. Try 0 - 7\n");
		return;
	}

	sPIValue.i16uAddress = offset;
	sPIValue.i8uBit = bit;
	// Get bit
	rc = piControlGetBitValue(&sPIValue);
	if (rc < 0) {
		printf("Get bit error\n");
	} else if (quiet) {
		printf("%d\n", sPIValue.i8uValue);
	} else {
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
	if (rc < 0) {
		printf("Cannot read variable info\n");
	} else {
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
	printf("      -v <var_name>: Shows infos for a variable.\n");
	printf("\n");
	printf("                 -1: execute the following read only once.\n");
	printf("\n");
	printf("                 -q: execute the following read quietly, print only the value.\n");
	printf("\n");
	printf("-r <var_name>[,<f>]: Reads value of a variable.\n");
	printf("                     <f> defines the format: h for hex, d for decimal (default), b for binary\n");
	printf("                     E.g.: -r Input_001,h\n");
	printf("                     Read value from variable 'Input_001'.\n");
	printf("                     Shows values cyclically every second.\n");
	printf("                     Break with Ctrl-C.\n");
	printf("\n");
	printf("   -r <o>,<l>[,<f>]: Reads <l> bytes at offset <o>.\n");
	printf("                     <f> defines the format: h for hex, d for decimal (default), b for binary\n");
	printf("                     E.g.: -r 1188,16\n");
	printf("                     Read 16 bytes at offset 1188.\n");
	printf("                     Shows values cyclically every second.\n");
	printf("                     Break with Ctrl-C.\n");
	printf("\n");
	printf("  -w <var_name>,<v>: Writes value <v> to variable.\n");
	printf("                     E.g.: -w Output_001,23:\n");
	printf("                     Write value 23 dez (=17 hex) to variable 'Output_001'.\n");
	printf("\n");
	printf("     -w <o>,<l>,<v>: Writes <l> bytes with value <v> (as hex) to offset <o>.\n");
	printf("                     length should be one of 1|2|4.\n");
	printf("                     E.g.: -w 0,4,31224205:\n");
	printf("                     Write value 31224205 hex (=824328709 dez) to offset 0.\n");
	printf("\n");
	printf("         -g <o>,<b>: Gets bit number <b> (0-7) from byte at offset <o>.\n");
	printf("                     E.g.: -b 0,5:\n");
	printf("                     Get bit 5 from byte at offset 0.\n");
	printf("\n");
	printf("   -s <o>,<b>,<0|1>: Sets 0|1 to bit <b> (0-7) of byte at offset <o>.\n");
	printf("                     E.g.: -b 0,5,1:\n");
	printf("                     Set bit 5 to 1 of byte at offset 0.\n");
	printf("\n");
	printf("     -R <addr>,<bs>: Reset counters/encoders in a digital input module like DIO or DI.\n");
	printf("                     <addr> is the address of module as displayed with option -d.\n");
	printf("                     <bs> is a bitset. If the counter on input pin n must be reset,\n");
	printf("                     the n-th bit must be set to 1.\n");
	printf("                     E.g.: -R 32,0x0014:\n");
	printf("                     Reset the counters on input pin I_3 and I_5.\n");
	printf("\n");
	printf("                 -S: Stop/Restart I/O update.\n");
	printf("\n");
	printf("                 -x: Reset piControl process.\n");
	printf("\n");
	printf("                 -l: Wait for reset of piControl process.\n");
	printf("\n");
	printf("                 -f: Update firmware. (see tutorials on website for more info) \n");
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
	int address;
	unsigned int val;
	char format;
	int bit;
	bool cyclic = true;	// default is cyclic output
	bool quiet = false;	// default is verbose output
	unsigned long value;
	char szVariableName[256];
	char *pszTok, *progname;

	progname = strrchr(argv[0], '/');
	if (!progname) {
		progname = argv[0];
	} else {
		progname++;
	}

	if (!strcmp(progname, "piControlReset")) {
		rc = piControlReset();
		if (rc) {
			printf("Cannot reset: %s\n", strerror(-rc));
		}
		return rc;
	}

	if (argc == 1) {
		printHelp(progname);
		return 0;
	}
	// Scan argument
	while ((c = getopt(argc, argv, "dv:1qr:w:s:R:g:xlfS")) != -1) {
		switch (c) {
		case 'd':
			showDeviceList();
			break;

		case 'v':
			if (strlen(optarg) > 0) {
				showVariableInfo(optarg);
			} else {
				printf("No variable name\n");
			}
			break;

		case '1':	// execute the command only once, not cyclic
			cyclic = false;
			break;

		case 'q':	// execute the command quietly
			quiet = true;
			break;

		case 'r':
			format = 'd';
			rc = sscanf(optarg, "%d,%d,%c", &offset, &length, &format);
			if (rc == 3) {
				readData(offset, length, cyclic, format, quiet);
				return 0;
			}
			rc = sscanf(optarg, "%d,%d", &offset, &length);
			if (rc == 2) {
				readData(offset, length, cyclic, format, quiet);
				return 0;
			}
			rc = sscanf(optarg, "%s", szVariableName);
			if (rc == 1) {
				pszTok = strtok(szVariableName, ",");
				if (pszTok != NULL) {
					pszTok = strtok(NULL, ",");
					if (pszTok != NULL) {
						format = *pszTok;
					}
				}
				readVariableValue(szVariableName, cyclic, format, quiet);
				return 0;
			}
			printf("Wrong arguments for read function\n");
			printf("1.) Try '-r variablename'\n");
			printf("2.) Try '-r offset,length' (without spaces)\n");
			break;

		case 'w':
			rc = sscanf(optarg, "%d,%d,%lu", &offset, &length, &value);
			if (rc == 3) {
				writeData(offset, length, value);
				return 0;
			}
			pszTok = strtok(optarg, ",");
			if (pszTok != NULL) {
				strncpy(szVariableName, pszTok, sizeof(szVariableName));
				pszTok = strtok(NULL, ",");
				if (pszTok != NULL) {
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
			rc = sscanf(optarg, "%d,%d,%lu", &offset, &bit, &value);
			if (rc != 3) {
				printf("Wrong arguments for set bit function\n");
				printf("Try '-s offset,bit,value' (without spaces)\n");
				return 0;
			}
			setBit(offset, bit, value);
			break;

		case 'R':	// reset counter
			rc = sscanf(optarg, "%d,0x%x", &address, &val);
			if (rc != 2) {
				rc = sscanf(optarg, "%d,%u", &address, &val);
				if (rc != 2) {
					printf("Wrong arguments for counter reset function\n");
					printf("Try '-R address,value' (without spaces)\n");
					return 0;
				}
			}
			piControlResetCounter(address, val);
			break;

		case 'g':
			rc = sscanf(optarg, "%d,%d", &offset, &bit);
			if (rc != 2) {
				printf("Wrong arguments for get bit function\n");
				printf("Try '-g offset,bit' (without spaces)\n");
				return 0;
			}
			getBit(offset, bit, quiet);
			break;

		case 'x':
			rc = piControlReset();
			if (rc) {
				printf("Cannot reset: %s\n", strerror(-rc));
				return rc;
			}
			break;

		case 'l':
			rc = piControlWaitForEvent();
			if (rc < 0) {
				printf("WaitForEvent returned: %d (%s)\n", rc, strerror(-rc));
				return rc;
			} else if (rc == 1) {
				printf("WaitForEvent returned: Reset\n");
				return rc;
			} else {
				printf("WaitForEvent returned: %d\n", rc);
				return rc;
			}
			break;

		case 'f':
			rc = piControlUpdateFirmware(0);
			if (rc) {
				printf("piControlUpdateFirmware returned: %d (%s)\n", rc, strerror(-rc));
				return rc;
			}
			break;

		case 'S':
			rc = piControlStopIO(2);	// toggle mode of I/Os
			if (rc < 0) {
				printf("error in setting I/O update mode: %d\n", rc);
			} else if (rc == 0) {
				printf("I/Os and process image are updated\n");
			} else {
				printf("update of I/Os and process image is stopped\n");
			}
			break;

		case 'h':
		default:
			printHelp(progname);
			break;
		}
	}
	return 0;
}

#pragma once

#include <common_define.h>
#include <IoProtocol.h>

#define AIO_OFFSET_InputValue_1			 0	//INT
#define AIO_OFFSET_InputValue_2			 2	//INT
#define AIO_OFFSET_InputValue_3			 4	//INT
#define AIO_OFFSET_InputValue_4			 6	//INT
#define AIO_OFFSET_InputStatus_1		 8	//BYTE
#define AIO_OFFSET_InputStatus_2		 9	//BYTE
#define AIO_OFFSET_InputStatus_3		10	//BYTE
#define AIO_OFFSET_InputStatus_4		11	//BYTE
#define AIO_OFFSET_RTDValue_1			12	//INT
#define AIO_OFFSET_RTDValue_2			14	//INT
#define AIO_OFFSET_RTDStatus_1			16	//BYTE
#define AIO_OFFSET_RTDStatus_2			17	//BYTE
#define AIO_OFFSET_Output_Status_1		18	//BYTE
#define AIO_OFFSET_Output_Status_2		19	//BYTE
#define AIO_OFFSET_OutputValue_1		20	//INT
#define AIO_OFFSET_OutputValue_2		22	//INT
#define AIO_OFFSET_Input1Range			24	// ##ATTR_COMMENT##
#define AIO_OFFSET_Input1Factor			25	// ##ATTR_COMMENT##
#define AIO_OFFSET_Input1Divisor		27	// ##ATTR_COMMENT##
#define AIO_OFFSET_Input1Offset			29	// ##ATTR_COMMENT##
#define AIO_OFFSET_Input2Range			31	// ##ATTR_COMMENT##
#define AIO_OFFSET_Input2Factor			32	// ##ATTR_COMMENT##
#define AIO_OFFSET_Input2Divisor		34	// ##ATTR_COMMENT##
#define AIO_OFFSET_Input2Offset			36	// ##ATTR_COMMENT##
#define AIO_OFFSET_Input3Range			38	// ##ATTR_COMMENT##
#define AIO_OFFSET_Input3Factor			39	// ##ATTR_COMMENT##
#define AIO_OFFSET_Input3Divisor		41	// ##ATTR_COMMENT##
#define AIO_OFFSET_Input3Offset			43	// ##ATTR_COMMENT##
#define AIO_OFFSET_Input4Range			45	// ##ATTR_COMMENT##
#define AIO_OFFSET_Input4Factor			46	// ##ATTR_COMMENT##
#define AIO_OFFSET_Input4Divisor		48	// ##ATTR_COMMENT##
#define AIO_OFFSET_Input4Offset			50	// ##ATTR_COMMENT##
#define AIO_OFFSET_InputSampleRate		52	// ##ATTR_COMMENT##
#define AIO_OFFSET_RTD1Type			53	// ##ATTR_COMMENT##
#define AIO_OFFSET_RTD1Method			54	// ##ATTR_COMMENT##
#define AIO_OFFSET_RTD1Factor			55	// ##ATTR_COMMENT##
#define AIO_OFFSET_RTD1Divisor			57	// ##ATTR_COMMENT##
#define AIO_OFFSET_RTD1Offset			59	// ##ATTR_COMMENT##
#define AIO_OFFSET_RTD2Type			61	// ##ATTR_COMMENT##
#define AIO_OFFSET_RTD2Method			62	// ##ATTR_COMMENT##
#define AIO_OFFSET_RTD2Factor			63	// ##ATTR_COMMENT##
#define AIO_OFFSET_RTD2Divisor			65	// ##ATTR_COMMENT##
#define AIO_OFFSET_RTD2Offset			67	// ##ATTR_COMMENT##
#define AIO_OFFSET_Output1Range			69	// ##ATTR_COMMENT##
#define AIO_OFFSET_Output1EnableSlew		70	//Enable Slew Rate Control ##ATTR_COMMENT##
#define AIO_OFFSET_Output1SlewStepSize		71	//Slew rate step size ##ATTR_COMMENT##
#define AIO_OFFSET_Output1SlewUpdateFreq	72	//Slew Rate Update Clock ##ATTR_COMMENT##
#define AIO_OFFSET_Output1Factor		73	// ##ATTR_COMMENT##
#define AIO_OFFSET_Output1Divisor		75	// ##ATTR_COMMENT##
#define AIO_OFFSET_Output1Offset		77	// ##ATTR_COMMENT##
#define AIO_OFFSET_Output2Range			79	// ##ATTR_COMMENT##
#define AIO_OFFSET_Output2EnableSlew		80	//Enable Slew Rate Control ##ATTR_COMMENT##
#define AIO_OFFSET_Output2SlewStepSize		81	//Slew rate step size ##ATTR_COMMENT##
#define AIO_OFFSET_Output2SlewUpdateFreq	82	//Slew Rate Update Clock ##ATTR_COMMENT##
#define AIO_OFFSET_Output2Factor		83	// ##ATTR_COMMENT##
#define AIO_OFFSET_Output2Divisor		85	// ##ATTR_COMMENT##
#define AIO_OFFSET_Output2Offset		87	// ##ATTR_COMMENT##


void piAIOComm_InitStart(void);

INT32U piAIOComm_Config(uint8_t i8uAddress, uint16_t i16uNumEntries, SEntryInfo * pEnt);

INT32U piAIOComm_Init(INT8U i8uDevice_p);

INT32U piAIOComm_sendCyclicTelegram(INT8U i8uDevice_p);

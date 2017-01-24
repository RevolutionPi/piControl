#pragma once

#include <common_define.h>
#include <IoProtocol.h>

void piDIOComm_InitStart(void);

INT32U piDIOComm_Config(uint8_t i8uAddress, uint16_t i16uNumEntries, SEntryInfo *pEnt);

INT32U piDIOComm_Init(INT8U i8uDevice_p);

INT32U piDIOComm_sendCyclicTelegram(INT8U i8uDevice_p);

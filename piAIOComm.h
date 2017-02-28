#pragma once

#include <common_define.h>
#include <IoProtocol.h>

void piAIOComm_InitStart(void);

INT32U piAIOComm_Config(uint8_t i8uAddress, uint16_t i16uNumEntries, SEntryInfo *pEnt);

INT32U piAIOComm_Init(INT8U i8uDevice_p);

INT32U piAIOComm_sendCyclicTelegram(INT8U i8uDevice_p);

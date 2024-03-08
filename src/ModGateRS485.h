/*=======================================================================================
 *
 *	       KK    KK   UU    UU   NN    NN   BBBBBB    UU    UU    SSSSSS
 *	       KK   KK    UU    UU   NNN   NN   BB   BB   UU    UU   SS
 *	       KK  KK     UU    UU   NNNN  NN   BB   BB   UU    UU   SS
 *	+----- KKKKK      UU    UU   NN NN NN   BBBBB     UU    UU    SSSSS
 *	|      KK  KK     UU    UU   NN  NNNN   BB   BB   UU    UU        SS
 *	|      KK   KK    UU    UU   NN   NNN   BB   BB   UU    UU        SS
 *	|      KK    KKK   UUUUUU    NN    NN   BBBBBB     UUUUUU    SSSSSS     GmbH
 *	|
 *	|            [#]  I N D U S T R I A L   C O M M U N I C A T I O N
 *	|             |
 *	+-------------+
 *
 *---------------------------------------------------------------------------------------
 *
 * (C) KUNBUS GmbH, Heerweg 15C, 73770 Denkendorf, Germany
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License V2 as published by
 * the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *  For licencing details see COPYING
 *
 *=======================================================================================
 */

#ifndef MODGATERS485_H_INC
#define MODGATERS485_H_INC

#include "piControl.h"

#define MAX_FWU_DATA_SIZE      250
#define MODGATE_RS485_BROADCAST_ADDR            0xff

typedef enum
{
    eCmdPing                  = 0x0001,
    eCmdSetFwUpdateMode       = 0x0002,
    eCmdGetFwInfo             = 0x0003,
    eCmdReadFwFlash           = 0x0004,
    eCmdWriteFwFlash          = 0x0005,
    eCmdEraseFwFlash          = 0x0006,
    eCmdWriteSerialNumber     = 0x0007,
    eCmdResetModule           = 0x0008,
    eCmdWriteMacAddr          = 0x0009,
    eCmdGetDeviceInfo         = 0x000a,
    eCmdFactoryReset          = 0x000b,
    eCmdSetLEDGreen           = 0x000c,
    eCmdSetLEDRed             = 0x000d,
    eCmdRAPIMessage           = 0x000e,
    eCmdGetErrorLog           = 0x000f,
    eCmdScriptMessage         = 0x0010,
    eCmdAgentCode             = 0x0011,     //!< Agent testing only, not in productive environment
    eCmdserFlashErase         = 0x0012,     // löschen des externen Flash
    eCmdserFlashWrite         = 0x0013,     // schreiben auf externen Flash
    // Commands from PiBridge master to slave
    eCmdPiIoSetAddress        = 0x0014,     // PiBridge master sends the module address
    eCmdPiIoSetTermination    = 0x0015,     // The slave should set the RS485 termination resistor
    eCmdPiIoConfigure         = 0x0016,     // The configuration data for the slave
    eCmdPiIoStartDataExchange = 0x0017,     // Slave have to start dataexchange
} ERs485Command;

#endif // MODGATERS485_H_INC

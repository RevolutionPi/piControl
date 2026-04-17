/* SPDX-License-Identifier: GPL-2.0-only
 * SPDX-FileCopyrightText: 2017-2023 KUNBUS GmbH
 */

#ifndef FWUFLASHFILEMAIN_H_INC
#define FWUFLASHFILEMAIN_H_INC

#include "common_define.h"

typedef struct StrFileHeadData {
    u16 usType;
    u16 usHwRev;
    u16 usFwuRev;
    u32 ulFlashStart;
    u32 ulFlashEnd;
    u32 ulFwuEntry;
    u32 ulFlashCrc;
} __attribute__((__packed__)) TFileHeadData; // sizeof = 22

typedef struct StrFPGAHeadData {
    u32 ulFPGALen;
    u32 ulFPGACrc;
} __attribute__((__packed__)) TFPGAHeadData; // sizeof = 8

typedef struct StrFileHead {
    u8 acSync[2];
    u32 ulLength;
    TFileHeadData dat;
    TFPGAHeadData fpga;
} __attribute__((__packed__)) TFileHead; // sizeof = 36

#endif // FWUFLASHFILEMAIN_H_INC

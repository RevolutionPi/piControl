/* SPDX-License-Identifier: GPL-2.0-only
 * SPDX-FileCopyrightText: 2017-2024 KUNBUS GmbH
 */

#ifndef _REVPI_COMPACT_H
#define _REVPI_COMPACT_H

#include <linux/types.h>
#include <linux/platform_device.h>

#include "common_define.h"
#include "piControl.h"

#define RevPi_Compact_OFFSET_CoreTemperatur		 0	//BYTE
#define RevPi_Compact_OFFSET_CoreFrequency		 1	//BYTE
#define RevPi_Compact_OFFSET_DIn			 2	//BYTE
#define RevPi_Compact_OFFSET_AIn1			 3	//INT
#define RevPi_Compact_OFFSET_AIn2			 5	//INT
#define RevPi_Compact_OFFSET_AIn3			 7	//INT
#define RevPi_Compact_OFFSET_AIn4			 9	//INT
#define RevPi_Compact_OFFSET_AIn5			11	//INT
#define RevPi_Compact_OFFSET_AIn6			13	//INT
#define RevPi_Compact_OFFSET_AIn7			15	//INT
#define RevPi_Compact_OFFSET_AIn8			17	//INT
#define RevPi_Compact_OFFSET_DIn_Status			19	//BYTE
#define RevPi_Compact_OFFSET_DOut_Status		20	//BYTE
#define RevPi_Compact_OFFSET_AIn_Status			21	//BYTE
#define RevPi_Compact_OFFSET_AOut_Status		22	//BYTE
#define RevPi_Compact_OFFSET_RevPiLED			23	//BYTE
#define RevPi_Compact_OFFSET_DOut			24	//BYTE
#define RevPi_Compact_OFFSET_AOut1			25	//BYTE
#define RevPi_Compact_OFFSET_AOut2			27	//BYTE
#define RevPi_Compact_OFFSET_DInDebounce		29	//BYTE
#define RevPi_Compact_OFFSET_AInMode1			30	//INT
#define RevPi_Compact_OFFSET_AInMode2			31	//INT
#define RevPi_Compact_OFFSET_AInMode3			32	//INT
#define RevPi_Compact_OFFSET_AInMode4			33	//INT
#define RevPi_Compact_OFFSET_AInMode5			34	//INT
#define RevPi_Compact_OFFSET_AInMode6			35	//INT
#define RevPi_Compact_OFFSET_AInMode7			36	//INT
#define RevPi_Compact_OFFSET_AInMode8			37	//INT

typedef struct _SRevPiCompactConfig {
	unsigned int offset;
	u16 din_debounce; /* usec */
	u8 ain[8];
#define AIN_ENABLED 0  /* bit number */
#define AIN_RTD     1
#define AIN_PT1K    2
} SRevPiCompactConfig;

typedef struct _SRevPiCompactImage {
	struct {
		u8 i8uCPUTemperature;
		u8 i8uCPUFrequency;
		u8 din;
		s16 ain[8];
		u8 din_status; /* identical layout as SDioModuleStatus */
		u8 dout_status;
		u8 ain_status;
#define AIN_TX_ERR  7 /* bit number */
		u8 aout_status;
#define AOUT_TX_ERR 7
	} __attribute__ ((__packed__)) drv;	// 23 bytes
	struct {
		u8 led;
		u8 dout;
		u16 aout[2];
	} __attribute__ ((__packed__)) usr;	// 6 bytes
} __attribute__ ((__packed__)) SRevPiCompactImage;

struct revpi_compact_stats {
	u64 lost_cycles;
	seqlock_t lock;
};

INT32U revpi_compact_config(uint8_t i8uAddress, uint16_t i16uNumEntries, SEntryInfo * pEnt);
int revpi_compact_reset(void);
int revpi_compact_probe(struct platform_device *pdev);
void revpi_compact_remove(struct platform_device *pdev);
void revpi_compact_adjust_config(void);
#endif /*_REVPI_COMPACT_H */

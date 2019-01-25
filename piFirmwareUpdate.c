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

#include <project.h>
#include <common_define.h>

#include <linux/module.h>	// included for all kernel modules
#include <linux/kernel.h>	// included for KERN_INFO
#include <linux/slab.h>		// included for KERN_INFO
#include <linux/fs.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <asm/segment.h>

#include "compat.h"
#include "revpi_common.h"
#include "revpi_core.h"
#include "piFirmwareUpdate.h"
#include "fwuFlashFileMain.h"


// ret < 0: error
// ret == 0: no update needed
// ret > 0: updated
int FWU_update(tpiControlInst *priv, SDevice *pDev_p)
{
	struct file *input;
	char *filename;
	char *data = 0;
	loff_t length;
	int ret = -EINVAL;
	TFileHead header;
	T_KUNBUS_APPL_DESCR *pApplDesc;
	int read;

	filename = kmalloc(PATH_MAX, GFP_KERNEL);

	sprintf(filename, FIRMWARE_PATH "/fw_%05d_%03d.fwu", pDev_p->sId.i16uModulType, pDev_p->sId.i16uHW_Revision);

	input = open_filename(filename, O_RDONLY);

	if (!input) {
		kfree(filename);
		return -ENOENT;
	}

	read = kernel_read(input, (char *)&header, sizeof(header), &input->f_pos);
	if (read <= 0) {
		pr_err("kernel_read returned %d: %x, %ld\n", read, (int)input, (long int)input->f_pos);
		ret = -EINVAL;
		goto laError;
	}

	if (header.dat.usType != pDev_p->sId.i16uModulType) {
		if (header.dat.usType == KUNBUS_FW_DESCR_TYP_PI_DIO_14
		    && (pDev_p->sId.i16uModulType == KUNBUS_FW_DESCR_TYP_PI_DO_16
			|| pDev_p->sId.i16uModulType == KUNBUS_FW_DESCR_TYP_PI_DI_16))
		{
			// this exception is allowed
			// DIO, DI and DO use the same software
		}
		else
		{
			pr_err("Module type does not match: %d != %d\nFirmware file %s is corrupt!\n", header.dat.usType, pDev_p->sId.i16uModulType, filename);
			ret = -ENODEV;
			goto laError;
		}
	}
	if (header.dat.usHwRev != pDev_p->sId.i16uHW_Revision) {
		pr_err("HW revision does not match: %d != %d\nFirmware file %s is corrupt!\n", header.dat.usHwRev, pDev_p->sId.i16uHW_Revision, filename);
		ret = -ENODEV;
		goto laError;
	}

	length = vfs_llseek(input, 0, SEEK_END);
	pr_info("firmware file length: %ld\n", (long int)length);

	length -= header.ulLength + 6; // without header
	data = kmalloc(length, GFP_KERNEL);
	if (data == NULL) {
		pr_err("out of memory\n");
		goto laError;
	}

	// seek to the begin of the application descriptor, if we are not already there
	vfs_llseek(input, header.ulLength + 6, SEEK_SET);

	// read the whole file
	read = kernel_read(input, data, length, &input->f_pos);
	if (read < length) {
		pr_err("kernel_read returned %d: %x, %ld\n", read, (int)input, (long int)input->f_pos);
		goto laError;
	}

	pApplDesc = (T_KUNBUS_APPL_DESCR *)data;

	if ((pApplDesc->i8uSwMajor < pDev_p->sId.i16uSW_Major)
	    || (pApplDesc->i8uSwMajor == pDev_p->sId.i16uSW_Major && pApplDesc->i16uSwMinor <= pDev_p->sId.i16uSW_Minor)) {
		// firmware file is older or equal than firmware in module -> nothing to do
		printUserMsg(priv, "firmware is up to date: %d.%d >= %d.%d",
			pDev_p->sId.i16uSW_Major, pDev_p->sId.i16uSW_Minor,
			pApplDesc->i8uSwMajor, pApplDesc->i16uSwMinor);
		ret = 0;
		goto laError;
	}

	printUserMsg(priv, "update firmware: %d.%d --> %d.%d",
		pDev_p->sId.i16uSW_Major, pDev_p->sId.i16uSW_Minor,
		pApplDesc->i8uSwMajor, pApplDesc->i16uSwMinor);

	if (PiBridgeMaster_FWUModeEnter(pDev_p->i8uAddress, pDev_p->i8uScan) == 0) {
		msleep(500);

		ret = PiBridgeMaster_FWUflashErase();
		if (ret) {
			goto laError;
		}

		ret = PiBridgeMaster_FWUflashWrite(header.dat.ulFlashStart, data, length);
		if (ret) {
			goto laError;
		}

		PiBridgeMaster_FWUReset();
		printUserMsg(priv,"update firmware success");
		ret = 1;	// success
	}

laError:
	if (data)
		kfree(data);
	kfree(filename);
	close_filename(input);
	if(ret!=1)
		printUserMsg(priv, "update firmware fail");
	return ret;
}

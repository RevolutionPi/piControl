// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: 2017-2024 KUNBUS GmbH

#include <linux/firmware.h>
#include "fwuFlashFileMain.h"
#include "piFirmwareUpdate.h"
#include "RS485FwuCommand.h"
#include "revpi_core.h"

#define TFPGA_HEAD_DATA_OFFSET			6
#define	CHUNK_TRANSMISSION_ATTEMPTS		100
#define	FLASH_ERASE_ATTEMPTS			5

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

	if (pDev_p->sId.i16uModulType & PICONTROL_NOT_CONNECTED) {
		printUserMsg(priv, "Disconnected modules can't be updated");
		return -EOPNOTSUPP;
	}

	if (pDev_p->sId.i16uModulType >= PICONTROL_SW_OFFSET) {
		printUserMsg(priv, "Virtual modules don't have firmware to update");
		return -EOPNOTSUPP;
	}

	if (pDev_p->i8uAddress == 0) {
		printUserMsg(priv, "RevPi has no firmware to update");
		return -EOPNOTSUPP;
	}

	filename = kmalloc(PATH_MAX, GFP_KERNEL);

	sprintf(filename, FIRMWARE_PATH "/fw_%05d_%03d.fwu", pDev_p->sId.i16uModulType, pDev_p->sId.i16uHW_Revision);

	input = open_filename(filename, O_RDONLY);

	if (!input) {
		kfree(filename);
		return -ENOENT;
	}

	read = kernel_read(input, (char *)&header, sizeof(header), &input->f_pos);
	if (read <= 0) {
		pr_err("kernel_read returned %d: %s, %lld\n", read, filename, input->f_pos);
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
		pr_err("kernel_read returned %d: %s, %lld\n", read, filename, input->f_pos);
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
	if (ret < 0)
		printUserMsg(priv, "update firmware fail");
	return ret;
}

static bool firmware_is_newer(unsigned int major_a, unsigned int minor_a,
			      unsigned int major_b, unsigned int minor_b)
{
	if (major_a < major_b)
		return false;
	if (major_a > major_b)
		return true;
	if (minor_a > minor_b)
		return true;
	return false;
}

static int flash_chunk(unsigned int dev_addr, unsigned int chunk_addr,
		       unsigned char *chunk_data, unsigned int chunk_len,
		       unsigned int *retrans)

{
	unsigned int attempts = CHUNK_TRANSMISSION_ATTEMPTS;
	int ret;

	do {
		ret = fwuWrite(dev_addr, chunk_addr, chunk_data, chunk_len);
		if (ret) {
			/*
			 * A failure may result from a protocol communication
			 * error. Retry to submit the chunk a if attempts are
			 * left.
			 */
			usleep_range(1000, 2000);
			attempts--;
			pr_debug("Error transmitting firmware for flash addr 0x%08x, len %u: %i (left attempts: %u)\n",
				chunk_addr, chunk_len, ret, attempts);
		}
	} while (ret && attempts);

	*retrans = CHUNK_TRANSMISSION_ATTEMPTS - attempts;

	return ret;
}

int flash_firmware(unsigned int dev_addr, unsigned int flash_addr,
		   unsigned char *upload_data, unsigned int upload_len)
{
	unsigned int upload_offset = 0;
	unsigned int total_retrans = 0;
	unsigned int chunk_len;
	unsigned int retrans;
	int ret = 0;

	while (upload_len) {
		if (upload_len > MAX_FWU_DATA_SIZE)
			chunk_len = MAX_FWU_DATA_SIZE;
		else
			chunk_len = upload_len;

		ret = flash_chunk(dev_addr, flash_addr + upload_offset,
				  upload_data + upload_offset, chunk_len,
				  &retrans);
		total_retrans += retrans;
		if (ret)
			break;

		upload_offset += chunk_len;
		upload_len -= chunk_len;
	}

	if (total_retrans)
		pr_warn("%u retransmissions during firmware update required\n",
			total_retrans);

	return ret;
}

int erase_flash(unsigned int dev_addr)
{
	unsigned int attempts = FLASH_ERASE_ATTEMPTS;
	int ret;

	do {
		ret = fwuEraseFlash(dev_addr);
		if (ret) {
			/*
			 * A failure may result from a protocol communication
			 * error. Retry to erase flash if attempts are left.
			 */
			usleep_range(1000, 2000);
			attempts--;
			pr_debug("Error erasing flash: %i (left attempts: %u)\n",
				ret, attempts);
		}
	} while (ret && attempts);

	/* failure */
	if (ret)
		return ret;

	if (attempts != FLASH_ERASE_ATTEMPTS)
		pr_warn("%u attempts to erase flash required\n",
			FLASH_ERASE_ATTEMPTS - attempts);
	return 0;
}

int upload_firmware(SDevice *sdev, const struct firmware *fw, u32 mask,
		    unsigned int module_type, unsigned int hw_rev)
{
	T_KUNBUS_APPL_DESCR *desc;
	unsigned int flash_offset;
	unsigned int upload_len;
	unsigned int dev_addr;
	bool force_upload;
	TFileHead *hdr;
	bool update;
	int ret = 0;

	force_upload  = !!(mask & PICONTROL_FIRMWARE_FORCE_UPLOAD);

	hdr = (TFileHead *) &fw->data[0];
	if (hdr->dat.usType != module_type) {
		if (hdr->dat.usType != KUNBUS_FW_DESCR_TYP_PI_DIO_14)
			return -EIO;
		if ((module_type != KUNBUS_FW_DESCR_TYP_PI_DO_16) &&
		    (module_type != KUNBUS_FW_DESCR_TYP_PI_DI_16))
			return -EIO;
	}

	if (hdr->dat.usHwRev != hw_rev) {
		pr_err("HW revision %u in FW does not match HW revision %u of device\n",
			hdr->dat.usHwRev, hw_rev);
		return -EIO;
	}
	/* Flashing starts with an offset to the "fpga" element of the
	   TFileHead structure */
	flash_offset = hdr->ulLength + TFPGA_HEAD_DATA_OFFSET;

	if (fw->size <= flash_offset) {
		pr_err("firmware corrupted: invalid header length %u in firmware with size %zu\n",
			hdr->ulLength, fw->size);
		return -EIO;
	}
	desc = (T_KUNBUS_APPL_DESCR *) &fw->data[flash_offset];
	update = firmware_is_newer(desc->i8uSwMajor,
				   desc->i16uSwMinor,
				   sdev->sId.i16uSW_Major,
				   sdev->sId.i16uSW_Minor);
	if (update) {
		pr_info("Updating firmware from %u.%u to %u.%u\n",
			sdev->sId.i16uSW_Major, sdev->sId.i16uSW_Minor,
			desc->i8uSwMajor, desc->i16uSwMinor);
	} else if (force_upload) {
		pr_info("Forced replacement of firmware %u.%u to %u.%u\n",
			sdev->sId.i16uSW_Major, sdev->sId.i16uSW_Minor,
			desc->i8uSwMajor, desc->i16uSwMinor);
	} else {
		pr_info("Firmware %u.%u already up to date. Use forced upload to override with firmware %u.%u\n",
			sdev->sId.i16uSW_Major, sdev->sId.i16uSW_Minor,
			desc->i8uSwMajor, desc->i16uSwMinor);
		return 1;
	}

	upload_len = fw->size - flash_offset;
	dev_addr = sdev->i8uAddress;

	if (fwuEnterFwuMode(dev_addr) < 0) {
		pr_err("error entering firmware update mode\n");
		return -EIO;
	}

	/* Old mGates always use 2 as device address */
	if (!sdev->i8uScan)
		dev_addr = 2;
	else if (dev_addr < REV_PI_DEV_FIRST_RIGHT) {
		dev_addr = 1;
	} else {
		if (dev_addr == RevPiDevice_getAddrRight() - 1)
			dev_addr = 2;
		else
			dev_addr = 1;
	}

	msleep(500);

	if (erase_flash(dev_addr)) {
		pr_err("failed to erase flash\n");
		ret = -EIO;
		goto reset;
	}
	if (flash_firmware(dev_addr, hdr->dat.ulFlashStart,
			   (unsigned char *) desc, upload_len) < 0) {
		pr_err("Errors while flashing firmware\n");
		ret = -EIO;
		goto reset;
	}

	pr_info("Firmware upload successful.");
reset:
	if (fwuResetModule(dev_addr) < 0) {
		pr_err("failed to reset after firmware update\n");
		ret = -EIO;
	}

	return ret;
}

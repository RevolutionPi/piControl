/*
 * Copyright (C) 2020 KUNBUS GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/fcntl.h>
#include <linux/termios.h>
#include <linux/syscalls.h>
#include <asm/uaccess.h>
#include <asm/segment.h>
#include <linux/fs.h>
#include <linux/kthread.h>
#include <linux/gpio.h>
#include <linux/crc8.h>

#include <project.h>
#include <common_define.h>

#include "revpi_common.h"
#include "revpi_core.h"
#include "revpi_mio.h"

/* configurations of MIO modules */
static struct mio_config mio_list[REVPI_MIO_MAX];
/* the counter of the MIO module */
static int mio_cnt;
/* store the sent analog request.
   the field i8uChannels of struct SMioAnalogRequestData takes no function here,
   but it could be used for the debuging purpose */
static SMioAnalogRequestData mio_dio_request_last[REVPI_MIO_MAX];

static inline unsigned char revpi_crc8(void *buf, unsigned short len)
{
	return piIoComm_Crc8((INT8U *)buf, len);
}

static int revpi_mio_cycle_dio(SDevice *dev, SMioDigitalRequestData *req_data,
			       SMioDigitalResponseData *resp_data)
{
	SMioDigitalResponse resp;
	SMioDigitalRequest req;
	unsigned char crc_cal;
	int ret;

	revpi_io_build_header(&req.uHeader, dev->i8uAddress,
			      sizeof(SMioDigitalRequestData),
			      IOP_TYP1_CMD_DATA);
	/*copy: from process image:output to request*/
	rt_mutex_lock(&piDev_g.lockPI);
	memcpy(&req.sData, req_data, sizeof(SMioDigitalRequestData));
	rt_mutex_unlock(&piDev_g.lockPI);

	req.i8uCrc = revpi_crc8(&req, sizeof(req) - 1);

	ret = revpi_io_talk(&req, sizeof(req), &resp, sizeof(resp));
	if (ret) {
		pr_err_ratelimited("talk with mio for dio data error(addr:%d, "
				   "ret:%d)\n", dev->i8uAddress, ret);
		return -EIO;
	}

	crc_cal = revpi_crc8(&resp, sizeof(resp) - 1);
	if (resp.i8uCrc != crc_cal) {
		pr_err_ratelimited("crc for dio data err(got:%d, expect:%d)\n",
				   resp.i8uCrc, crc_cal);
		return -EBADMSG;
	}

	pr_info_once("headers of mio:dio data request: 0x%4x, response:0x%4x\n",
		     *(unsigned short *) &req.uHeader,
		     *(unsigned short *) &resp.uHeader);
	/*copy: from response to process image:input*/
	rt_mutex_lock(&piDev_g.lockPI);
	memcpy(resp_data, &resp.sData, sizeof(SMioDigitalResponseData));
	rt_mutex_unlock(&piDev_g.lockPI);

	return 0;
}

static int revpi_mio_cycle_aio(SDevice *dev, SMioAnalogRequestData *req_data,
			       size_t ch_cnt, SMioAnalogResponseData *resp_data)
{
	size_t compressed = (MIO_AIO_PORT_CNT - ch_cnt) * sizeof(INT16U);
	SMioAnalogResponse resp;
	SMioAnalogRequest req;
	unsigned char crc_cal;
	int ret;

	revpi_io_build_header(&req.uHeader, dev->i8uAddress,
			      sizeof(SMioAnalogRequestData) - compressed,
			      IOP_TYP1_CMD_DATA2);
	/*copy: from process image:output to request*/
	rt_mutex_lock(&piDev_g.lockPI);
	memcpy(&req.sData, req_data, sizeof(SMioAnalogRequestData) -
				     compressed);
	rt_mutex_unlock(&piDev_g.lockPI);

	req.i8uCrc = revpi_crc8(&req, sizeof(req) - 1 - compressed);
	/*crc is adjoining data */
	memcpy(&req.i8uCrc - compressed, &req.i8uCrc, 1);

	ret = revpi_io_talk(&req, sizeof(req) - compressed, &resp,
			    sizeof(resp));
	if (ret) {
		pr_err_ratelimited("talk with mio for aio data error(addr:%d, "
				   "ret:%d)\n", dev->i8uAddress, ret);
		return -EIO;
	}
	crc_cal = revpi_crc8(&resp, sizeof(resp) - 1);

	if (resp.i8uCrc != crc_cal) {
		pr_err_ratelimited("crc for aio data err(got:%d, expect:%d)\n",
				   resp.i8uCrc, crc_cal);
		return -EBADMSG;
	}

	pr_info_once("headers of mio:aio data request: 0x%4x, response:0x%4x\n",
		     *(unsigned short *) &req.uHeader,
		     *(unsigned short *) &resp.uHeader);
	/*copy: from response to process image*/
	rt_mutex_lock(&piDev_g.lockPI);
	memcpy(resp_data, &resp.sData, sizeof(SMioAnalogResponseData));
	rt_mutex_unlock(&piDev_g.lockPI);

	return 0;
}

/*
	compare to get the changed values of channels
	input parameters:
		a, b: data of two messages
		count: count of channels
		step: number of bytes for a channel
	return:	bit map of changed channels
*/
static unsigned long revpi_chnl_cmp(void *a, void *b, int count, int step)
{
	unsigned char *pa, *pb;
	unsigned long ret = 0;
	int i;

	pa = (unsigned char *) a;
	pb = (unsigned char *) b;

	for (i = 0; i < count; i++) {
		if (memcmp(pa + i * step, pb + i * step, step))
			set_bit(i, &ret);
	}
	return ret;
}

#define VALUE_MASK(v, h, l) ((v) & (GENMASK((h), (l))))
#define VALUE_MASK_L(v, h) VALUE_MASK((v), (h), 0)
#define VALUE_MASK_H(v, l) VALUE_MASK((v), (BITS_PER_LONG - 1), (l))

/*
	compress the channel, only data of changed channel will be taken
	input parameters:
		dst, dst: compress from src to dst
		bitmap: compress according to
		step: number of bytes for a channel
	return: the count of channels has been taken.
*/
static unsigned int revpi_chnl_compress(void *dst, void *src,
					unsigned long bitmap, int step)
{
	unsigned char *d, *s;
	unsigned int i = 0;

	s = (unsigned char *) src;
	d = (unsigned char *) dst;

	for (i = 0; i < 32 && VALUE_MASK_H(bitmap, i); i++) {
		if (test_bit(i, &bitmap)) {
			memcpy(d, s + i * step, step);
			d += step;
		}
	}
	return (d - (unsigned char *)dst) / step;
}

int revpi_mio_cycle(unsigned char devno)
{
	SMioAnalogRequestData io_req_ex;
	struct mio_img_out *img_out;
	SMioAnalogRequestData *last;
	struct mio_img_in *img_in;
	unsigned int ch_cnt = 0;
	SDevice *dev;
	int ret;

	dev = RevPiDevice_getDev(devno);
	last = &mio_dio_request_last[dev->i8uPriv];

	img_out = (struct mio_img_out *)(piDev_g.ai8uPI +
					 dev->i16uOutputOffset);
	img_in = (struct mio_img_in *)(piDev_g.ai8uPI + dev->i16uInputOffset);

	ret = revpi_mio_cycle_dio(dev, &img_out->dio, &img_in->dio);
	if (ret)
		return ret;

	/* for the AIO cycle */
	my_rt_mutex_lock(&piDev_g.lockPI);
	io_req_ex.i8uLogicLevel = img_out->aio.i8uLogicLevel;

	io_req_ex.i8uChannels = revpi_chnl_cmp(&last->i16uOutputVoltage,
						&img_out->aio.i16uOutputVoltage,
						MIO_AIO_PORT_CNT, 2);
	/* force to update from process image */
	io_req_ex.i8uChannels |= img_out->aio.i8uChannels;

	if (io_req_ex.i8uChannels) {
		memcpy(&last->i16uOutputVoltage,
			&img_out->aio.i16uOutputVoltage,
			sizeof(unsigned short) * MIO_AIO_PORT_CNT);

		ch_cnt = revpi_chnl_compress(&io_req_ex.i16uOutputVoltage,
						&img_out->aio.i16uOutputVoltage,
						io_req_ex.i8uChannels, 2);

		pr_info("copy bits,bitmap:0x%x,force:0x%x,len:%d,data:%*ph\n",
						io_req_ex.i8uChannels,
						img_out->aio.i8uChannels,
						ch_cnt,
						17,
						&io_req_ex);
	}
	rt_mutex_unlock(&piDev_g.lockPI);
	return revpi_mio_cycle_aio(dev, &io_req_ex, ch_cnt, &img_in->aio);
}

int revpi_mio_reset()
{
	mio_cnt = 0;
	return 0;
}

int revpi_mio_config(unsigned char addr, unsigned short e_cnt, SEntryInfo *ent)
{
	struct mio_config *conf;
	int arr_idx;
	int offset;
	int i;

	if (mio_cnt >= REVPI_MIO_MAX) {
		pr_err("max. of MIOs(%d) reached(%d)\n", REVPI_MIO_MAX,
		       mio_cnt);
		return -ERANGE;
	}

	conf = &mio_list[mio_cnt];
	memset(conf, 0, sizeof(struct mio_config));

	revpi_io_build_header(&conf->dio.uHeader, addr,
			      sizeof(SMioDIOConfigData), IOP_TYP1_CMD_CFG);
	revpi_io_build_header(&conf->aio_i.uHeader, addr,
			      sizeof(SMioAIOConfigData), IOP_TYP1_CMD_DATA4);
	revpi_io_build_header(&conf->aio_o.uHeader, addr,
			      sizeof(SMioAIOConfigData), IOP_TYP1_CMD_DATA4);

	/*0=input (InputThreshold)*/
	conf->aio_i.sData.i8uDirection = 0;
	/*1=output(Fixed Output)*/
	conf->aio_o.sData.i8uDirection = 1;

	pr_info("MIO configured(addr:%d, ent-cnt:%d, conf-no:%d, conf-base:%d, "
		"dio hdr:0x%x,aio_i hdr:0x%x, aio_o hdr:0x%x)\n",
		addr, e_cnt, mio_cnt, MIO_CONF_BASE,
		*(unsigned short*)&mio_list[mio_cnt].dio.uHeader,
		*(unsigned short*)&mio_list[mio_cnt].aio_i.uHeader,
		*(unsigned short*)&mio_list[mio_cnt].aio_o.uHeader);

	for (i = 0; i < e_cnt; i++) {
		offset = ent[i].i16uOffset;
		switch (offset) {
		case 0 ... MIO_CONF_BASE - 1:
			/*nothing to do for input and output */
			break;
		case MIO_CONF_EMOD:
			conf->dio.sData.i8uEncoderMode = ent[i].i32uDefault;
			break;
		case MIO_CONF_IOMOD ... MIO_CONF_PUL -1:
			arr_idx = (offset - MIO_CONF_IOMOD) / sizeof(INT8U);
			conf->dio.sData.i8uIoMode[arr_idx] = ent[i].i32uDefault;
			break;
		case MIO_CONF_PUL:
			conf->dio.sData.i8uPullup = ent[i].i32uDefault;
			break;
		case MIO_CONF_PMOD:
			conf->dio.sData.i8uPulseRetrigMode = ent[i].i32uDefault;
			break;
		case MIO_CONF_FPWM ... MIO_CONF_PLEN - 1:
			arr_idx = (offset - MIO_CONF_FPWM) / sizeof(INT16U);
			conf->dio.sData.i16uPwmFrequency[arr_idx]
							= ent[i].i32uDefault;
			break;
		case MIO_CONF_PLEN ... MIO_CONF_AIM - 1:
			arr_idx = (offset - MIO_CONF_PLEN) / sizeof(INT16U);
			conf->dio.sData.i16uPulseLength[arr_idx]
							= ent[i].i32uDefault;
			break;
		case MIO_CONF_AIM:
			conf->aio_i.sData.i8uIoMode
				|= ent[i].i32uDefault << ent[i].i8uBitPos;
			break;
		case MIO_CONF_THR ... MIO_CONF_WSIZE - 1:
			arr_idx = (offset - MIO_CONF_THR) / sizeof(INT16U);
			conf->aio_i.sData.i16uVolt[arr_idx] = ent[i].i32uDefault;
			break;
		case MIO_CONF_WSIZE:
			conf->aio_i.sData.i8uMvgAvgWindow = ent[i].i32uDefault;
			break;
		case MIO_CONF_AOM:
			conf->aio_o.sData.i8uIoMode
				|= ent[i].i32uDefault << ent[i].i8uBitPos;
			break;
		case MIO_CONF_OUTV ... MIO_CONF_END - 1:
			arr_idx = (offset - MIO_CONF_OUTV) / sizeof(INT16U);
			conf->aio_o.sData.i16uVolt[arr_idx] = ent[i].i32uDefault;
			break;
		default:
			pr_warn("unknown config entry(offset:%d)\n", offset);
			break;
		}
	}

	pr_info("dio  :%*ph\n", sizeof(conf->dio.sData), &conf->dio.sData);
	pr_info("aio-i:%*ph\n", sizeof(conf->aio_i.sData), &conf->aio_i.sData);
	pr_info("aio-o:%*ph\n", sizeof(conf->aio_o.sData), &conf->aio_o.sData);

	conf->dio.i8uCrc = revpi_crc8(&conf->dio, sizeof(conf->dio) - 1);
	conf->aio_i.i8uCrc = revpi_crc8(&conf->aio_i, sizeof(conf->aio_i) - 1);
	conf->aio_o.i8uCrc = revpi_crc8(&conf->aio_o, sizeof(conf->aio_o) - 1);

	mio_cnt++;

	return 0;
}

static inline void revpi_mio_addr_chk(struct mio_config *conf,
					unsigned char addr)
{
	/*the address of dio and aio (in and out) should be the same*/
	if ((conf->aio_i.uHeader.sHeaderTyp1.bitAddress != addr ) ||
	    (conf->aio_o.uHeader.sHeaderTyp1.bitAddress != addr )) {
		pr_warn("addr differ in mio conf(dio:%d, aio "
			"in:%d, aio out:%d)\n", addr,
			conf->aio_i.uHeader.sHeaderTyp1.bitAddress,
			conf->aio_o.uHeader.sHeaderTyp1.bitAddress);
	}
}

int revpi_mio_init(unsigned char devno)
{
	struct mio_config *conf = NULL;
	SMioConfigResponse resp;
	unsigned char crc;
	unsigned char addr;
	int ret;
	int i;

	addr = RevPiDevice_getDev(devno)->i8uAddress;

	pr_info("MIO Initializing...(devno:%d, addr:%d, conf-base:%d)\n",
						devno, addr, MIO_CONF_BASE);

	for (i = 0; i < mio_cnt; i++) {
		pr_info("search mio conf(index:%d, addr:%d)\n", i,
			mio_list[i].dio.uHeader.sHeaderTyp1.bitAddress);

		if (mio_list[i].dio.uHeader.sHeaderTyp1.bitAddress == addr) {
			conf = &mio_list[i];
			RevPiDevice_getDev(devno)->i8uPriv = (unsigned char) i;
			revpi_mio_addr_chk(conf, addr);
			break;
		}
	}

	if (!conf) {
		pr_err("fail to find the mio module(devno:%d)\n", devno);
		return -ENODATA;
	}
	/*dio*/
	memset(&resp, 0, sizeof(resp));
	ret = revpi_io_talk(&conf->dio, sizeof(conf->dio), &resp, sizeof(resp));
	if (ret) {
		pr_err("talk with mio for conf dio err(devno:%d, ret:%d)\n",
		       devno, ret);
	}
	pr_info("headers of mio:aio conf request: 0x%4x, response:0x%4x\n",
		*(unsigned short *) &conf->dio.uHeader,
		*(unsigned short *) &resp.uHeader);

	crc = revpi_crc8(&resp, sizeof(resp) - 1);
	if (resp.i8uCrc != crc)
		pr_err("crc for mio:dio conf err(got:%d, exp:%d)\n",
		       resp.i8uCrc, crc);
	/*aio out*/
	memset(&resp, 0, sizeof(resp));
	ret = revpi_io_talk(&conf->aio_i, sizeof(conf->aio_i), &resp,
			    sizeof(resp));
	if (ret)
		pr_err("talk with mio for conf aio_i err(devno:%d, ret:%d)\n",
		       devno, ret);

	pr_info("headers of mio:aio_i conf request: 0x%4x, response:0x%4x\n",
		*(unsigned short *) &conf->aio_i.uHeader,
		*(unsigned short *) &resp.uHeader);

	crc = revpi_crc8(&resp, sizeof(resp) - 1);
	if (resp.i8uCrc != crc)
		pr_err("crc for mio:aio_i conf err(got:%d, exp:%d)\n",
		       resp.i8uCrc, crc);
	/*aio out*/
	memset(&resp, 0, sizeof(resp));
	ret = revpi_io_talk(&conf->aio_o, sizeof(conf->aio_o), &resp,
			    sizeof(resp));
	if (ret)
		pr_err("talk with mio for conf aio_o err(devno:%d, ret:%d)\n",
		       devno, ret);
	pr_info("headers of mio:aio_o conf request: 0x%4x, response:0x%4x\n",
		*(unsigned short *) &conf->aio_o.uHeader,
		*(unsigned short *) &resp.uHeader);

	crc = revpi_crc8(&resp, sizeof(resp) - 1);
	if (resp.i8uCrc != crc)
		pr_err("crc for mio:aio_o conf err(got:%d, exp:%d)\n",
		       resp.i8uCrc, crc);

	pr_info("MIO Initializing finished(devno:%d, addr:%d)\n", devno, addr);

	return 0;
}

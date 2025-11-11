// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: 2020-2024 KUNBUS GmbH

#include <linux/pibridge_comm.h>

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
static SMioAnalogRequestData mio_aio_request_last[REVPI_MIO_MAX];

static int revpi_mio_cycle_dio(SDevice *dev, SMioDigitalRequestData *req_data,
			       SMioDigitalResponseData *resp_data)
{
	SMioDigitalResponseData resp;
	SMioDigitalRequestData req;
	int ret;

	/*copy: from process image:output to request*/
	rt_mutex_lock(&piDev_g.lockPI);
	memcpy(&req, req_data, sizeof(req));
	rt_mutex_unlock(&piDev_g.lockPI);

	ret = pibridge_req_io(dev->i8uAddress, IOP_TYP1_CMD_DATA,
			      &req, sizeof(req), &resp, sizeof(resp));
	if (ret != sizeof(resp)) {
		pr_debug("MIO addr %2d: dio communication failed (req:%zu,ret:%d)\n",
			dev->i8uAddress, sizeof(resp), ret);

		if (ret >= 0)
			ret = -EIO;

		return ret;
	}

	/*copy: from response to process image:input*/
	rt_mutex_lock(&piDev_g.lockPI);
	memcpy(resp_data, &resp, sizeof(*resp_data));
	rt_mutex_unlock(&piDev_g.lockPI);

	return 0;
}

static int revpi_mio_cycle_aio(SDevice *dev, SMioAnalogRequestData *req_data,
			       size_t ch_cnt, SMioAnalogResponseData *resp_data)
{
	size_t compressed = (MIO_AIO_PORT_CNT - ch_cnt) * sizeof(INT16U);
	SMioAnalogResponseData resp;
	int ret;

	ret = pibridge_req_io(dev->i8uAddress, IOP_TYP1_CMD_DATA2,
			      req_data, sizeof(*req_data) - compressed,
			      &resp, sizeof(resp));
	if (ret != sizeof(resp)) {
		pr_debug("MIO addr %2d: aio communication failed (req:%zd,ret:%d)\n",
			dev->i8uAddress, sizeof(resp), ret);

		if (ret >= 0)
			ret = -EIO;

		return ret;
	}

	/*copy: from response to process image*/
	rt_mutex_lock(&piDev_g.lockPI);
	memcpy(resp_data, &resp, sizeof(*resp_data));
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
			__set_bit(i, &ret);
	}
	return ret;
}

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
	unsigned int i, last = __fls(bitmap);

	s = (unsigned char *) src;
	d = (unsigned char *) dst;

	for (i = 0; i <= last; i++) {
		if (test_bit(i, &bitmap)) {
			memcpy(d, s + i * step, step);
			d += step;
		}
	}
	return (d - (unsigned char *)dst) / step;
}

int revpi_mio_cycle(unsigned char devno)
{
	SMioAnalogRequestData pending_values;
	SMioAnalogRequestData io_req_ex;
	struct mio_img_out *img_out;
	SMioAnalogRequestData *last;
	struct mio_img_in *img_in;
	unsigned int ch_cnt = 0;
	SDevice *dev;
	int ret;

	dev = RevPiDevice_getDev(devno);
	last = &mio_aio_request_last[dev->i8uPriv];

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
		/* preserve analog output values for later caching */
		memcpy(&pending_values.i16uOutputVoltage,
			&img_out->aio.i16uOutputVoltage,
			sizeof(unsigned short) * MIO_AIO_PORT_CNT);
		ch_cnt = revpi_chnl_compress(&io_req_ex.i16uOutputVoltage,
						&pending_values.i16uOutputVoltage,
						io_req_ex.i8uChannels, 2);
	}
	rt_mutex_unlock(&piDev_g.lockPI);
	ret = revpi_mio_cycle_aio(dev, &io_req_ex, ch_cnt, &img_in->aio);

	if (ret)
		return ret;

	/*
	 * Only cache analog output values if data was sent successfully
	 * and at least one channel was updated.
	 */
	if (io_req_ex.i8uChannels) {
		memcpy(&last->i16uOutputVoltage,
			&pending_values.i16uOutputVoltage,
			sizeof(unsigned short) * MIO_AIO_PORT_CNT);
		}

	return 0;
}

void revpi_mio_reset(void)
{
	mio_cnt = 0;
	memset(mio_aio_request_last, 0, sizeof(mio_aio_request_last));
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

	conf->addr = addr;

	/*0=input (InputThreshold)*/
	conf->aio_i.i8uDirection = 0;
	/*1=output(Fixed Output)*/
	conf->aio_o.i8uDirection = 1;

	pr_debug("MIO configured(addr:%d, ent-cnt:%d, conf-no:%d, conf-base:%zd)\n",
		addr, e_cnt, mio_cnt, MIO_CONF_BASE);

	for (i = 0; i < e_cnt; i++) {
		offset = ent[i].i16uOffset;
		switch (offset) {
		case 0 ... MIO_CONF_BASE - 1:
			/*nothing to do for input and output */
			break;
		case MIO_CONF_EMOD:
			conf->dio.i8uEncoderMode = ent[i].i32uDefault;
			break;
		case MIO_CONF_IOMOD ... MIO_CONF_PUL -1:
			arr_idx = (offset - MIO_CONF_IOMOD) / sizeof(INT8U);
			conf->dio.i8uIoMode[arr_idx] = ent[i].i32uDefault;
			break;
		case MIO_CONF_PUL:
			conf->dio.i8uPullup = ent[i].i32uDefault;
			break;
		case MIO_CONF_PMOD:
			conf->dio.i8uPulseRetrigMode = ent[i].i32uDefault;
			break;
		case MIO_CONF_FPWM ... MIO_CONF_PLEN - 1:
			arr_idx = (offset - MIO_CONF_FPWM) / sizeof(INT16U);
			conf->dio.i16uPwmFrequency[arr_idx]
							= ent[i].i32uDefault;
			break;
		case MIO_CONF_PLEN ... MIO_CONF_AIM - 1:
			arr_idx = (offset - MIO_CONF_PLEN) / sizeof(INT16U);
			conf->dio.i16uPulseLength[arr_idx]
							= ent[i].i32uDefault;
			break;
		case MIO_CONF_AIM:
			conf->aio_i.i8uIoMode
				|= ent[i].i32uDefault << ent[i].i8uBitPos;
			break;
		case MIO_CONF_THR ... MIO_CONF_WSIZE - 1:
			arr_idx = (offset - MIO_CONF_THR) / sizeof(INT16U);
			conf->aio_i.i16uVolt[arr_idx] = ent[i].i32uDefault;
			break;
		case MIO_CONF_WSIZE:
			conf->aio_i.i8uMvgAvgWindow = ent[i].i32uDefault;
			break;
		case MIO_CONF_AOM:
			conf->aio_o.i8uIoMode
				|= ent[i].i32uDefault << ent[i].i8uBitPos;
			break;
		case MIO_CONF_OUTV ... MIO_CONF_END - 1:
			arr_idx = (offset - MIO_CONF_OUTV) / sizeof(INT16U);
			conf->aio_o.i16uVolt[arr_idx] = ent[i].i32uDefault;
			break;
		default:
			pr_warn("unknown config entry(offset:%d)\n", offset);
			break;
		}
	}

	pr_debug("dio  :%*ph\n", (int) sizeof(conf->dio), &conf->dio);
	pr_debug("aio-i:%*ph\n", (int) sizeof(conf->aio_i), &conf->aio_i);
	pr_debug("aio-o:%*ph\n", (int) sizeof(conf->aio_o), &conf->aio_o);

	mio_cnt++;

	return 0;
}

int revpi_mio_init(unsigned char devno)
{
	struct mio_config *conf = NULL;
	unsigned char addr;
	int ret;
	int i;

	addr = RevPiDevice_getDev(devno)->i8uAddress;

	pr_debug("MIO Initializing...(devno:%d, addr:%d, conf-base:%zd)\n",
						devno, addr, MIO_CONF_BASE);

	for (i = 0; i < mio_cnt; i++) {
		pr_debug("search mio conf(index:%d, addr:%d)\n", i,
			mio_list[i].addr);

		if (mio_list[i].addr == addr) {
			conf = &mio_list[i];
			RevPiDevice_getDev(devno)->i8uPriv = (unsigned char) i;
			break;
		}
	}

	if (!conf) {
		pr_err("fail to find the mio module(devno:%d)\n", devno);
		return -ENODATA;
	}

	/*dio*/
	ret = pibridge_req_io(addr, IOP_TYP1_CMD_CFG,
			      &conf->dio, sizeof(conf->dio), NULL, 0);
	if (ret) {
		pr_err("talk with mio for conf dio err(devno:%d, ret:%d)\n",
		       devno, ret);
	}

	/*aio in*/
	ret = pibridge_req_io(addr, IOP_TYP1_CMD_DATA4,
			      &conf->aio_i, sizeof(conf->aio_i), NULL, 0);
	if (ret)
		pr_err("talk with mio for conf aio_i err(devno:%d, ret:%d)\n",
		       devno, ret);

	/*aio out*/
	ret = pibridge_req_io(addr, IOP_TYP1_CMD_DATA4,
			      &conf->aio_o, sizeof(conf->aio_o), NULL, 0);
	if (ret)
		pr_err("talk with mio for conf aio_o err(devno:%d, ret:%d)\n",
		       devno, ret);

	pr_debug("MIO Initializing finished(devno:%d, addr:%d)\n", devno, addr);

	return 0;
}

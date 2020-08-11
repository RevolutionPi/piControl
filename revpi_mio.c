#include <linux/module.h>	// included for all kernel modules
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

#include <project.h>
#include <common_define.h>

#include "revpi_common.h"
#include "revpi_core.h"
#include "revpi_mio.h"

/*configurations of MIO modules*/
static struct mio_config mio_conf_list[REVPI_MIO_MAX];

/*the counter of the MIO module*/
static int mio_conf_cnt;


int revpi_mio_cycle_dio(SDevice *dev, SMioDigitalRequestData *req_data,
											SMioDigitalResponseData *resp_data)
{
	int ret;
	SMioDigitalRequest req;
	SMioDigitalResponse resp;
	unsigned char crc_cal;

	revpi_io_build_header(&req.uHeader,
							dev->i8uAddress,
							sizeof(SMioDigitalRequestData),
							IOP_TYP1_CMD_DATA);

	/*copy: from process image:output to request*/
	rt_mutex_lock(&piDev_g.lockPI);
	memcpy(&req.sData, req_data, sizeof(SMioDigitalRequestData));
	rt_mutex_unlock(&piDev_g.lockPI);

	req.i8uCrc = revpi_crc8(&req, sizeof(req) - 1);

	ret = revpi_io_talk(&req, sizeof(req), &resp, sizeof(resp));
	if(ret) {
		pr_err_ratelimited("talk with mio for dio data error(addr:%d, ret:%d)\n",
														dev->i8uAddress, ret);
		return -EIO;
	}
	crc_cal = revpi_crc8(&resp, sizeof(resp) - 1);
	if (resp.i8uCrc != crc_cal) {
		pr_err_ratelimited("crc for dio data error(got:%d, expect:%d)\n",
														resp.i8uCrc, crc_cal);
		return -EBADMSG;
	}

	pr_info_once("headers of mio:dio data request: 0x%4x, response:0x%4x\n",
						*(unsigned short*)&req.uHeader,
						*(unsigned short*)&resp.uHeader);

	/*copy: from response to process image:input*/
	rt_mutex_lock(&piDev_g.lockPI);
	memcpy(resp_data, &resp.sData, sizeof(SMioDigitalResponseData));
	rt_mutex_unlock(&piDev_g.lockPI);

	return 0;
}

int revpi_mio_cycle_aio(SDevice *dev, SMioAnalogRequestData *req_data,
											SMioAnalogResponseData *resp_data)
{
	int ret;

	SMioAnalogRequest  req;
	SMioAnalogResponse resp;
	unsigned char crc_cal;

	revpi_io_build_header(&req.uHeader,
							dev->i8uAddress,
							sizeof(SMioAnalogRequestData),
							IOP_TYP1_CMD_DATA2);

	/*copy: from process image:output to request*/
	rt_mutex_lock(&piDev_g.lockPI);
	memcpy(&req.sData, req_data, sizeof(SMioAnalogRequestData));
	rt_mutex_unlock(&piDev_g.lockPI);

	req.i8uCrc = revpi_crc8(&req, sizeof(req) - 1);

	ret = revpi_io_talk(&req, sizeof(req), &resp, sizeof(resp));
	if(ret) {
		pr_err_ratelimited("talk with mio for aio data error(addr:%d, ret:%d)\n",
														dev->i8uAddress, ret);
		return -EIO;
	}

	crc_cal = revpi_crc8(&resp, sizeof(resp) - 1);
	if (resp.i8uCrc != crc_cal) {
		pr_err_ratelimited("crc for aio data error(got:%d, expect:%d)\n",
														resp.i8uCrc, crc_cal);
		return -EBADMSG;
	}

	pr_info_once("headers of mio:aio data request: 0x%4x, response:0x%4x\n",
						*(unsigned short*)&req.uHeader,
						*(unsigned short*)&resp.uHeader);
	/*copy: from response to process image*/
	rt_mutex_lock(&piDev_g.lockPI);
	memcpy(resp_data, &resp.sData, sizeof(SMioAnalogResponseData));
	rt_mutex_unlock(&piDev_g.lockPI);

	return 0;
}

int revpi_mio_cycle(unsigned char devno)
{
	int ret;
	SDevice *dev;
	mio_process_image_out *proc_img_out;
	mio_process_image_in *proc_img_in;

	dev = RevPiDevice_getDev(devno);

	proc_img_out = (mio_process_image_out *)(piDev_g.ai8uPI + dev->i16uOutputOffset);
	proc_img_in = (mio_process_image_in *)(piDev_g.ai8uPI + dev->i16uInputOffset);

	//TODO check how is the config exists in the process image

	ret = revpi_mio_cycle_dio(dev, &proc_img_out->dio, &proc_img_in->dio);
	if (ret) {
		return ret;
	}

	ret = revpi_mio_cycle_aio(dev, &proc_img_out->aio, &proc_img_in->aio);
	if (ret) {
		return ret;
	}

	return 0;
}

int revpi_mio_reset()
{
	mio_conf_cnt = 0;

	return 0;
}

int revpi_mio_config(unsigned char addr, unsigned short ent_cnt, SEntryInfo *ent)
{
	int i, offset;
	struct mio_config *conf;

	if (mio_conf_cnt >= REVPI_MIO_MAX) {
		pr_err("max. of MIOs(%d) reached(%d)\n", REVPI_MIO_MAX, mio_conf_cnt);
		return -ERANGE;
	}

	conf = &mio_conf_list[mio_conf_cnt];


	revpi_io_build_header(&conf->dio.uHeader,
							addr,
							sizeof(SMioDIOConfigData),
							IOP_TYP1_CMD_CFG);

	revpi_io_build_header(&conf->aio_i.uHeader,
							addr,
							sizeof(SMioAIOConfigData),
							IOP_TYP1_CMD_DATA4);
	conf->aio_i.sData.i8uDirection = 0;

	revpi_io_build_header(&conf->aio_o.uHeader,
							addr,
							sizeof(SMioAIOConfigData),
							IOP_TYP1_CMD_DATA4);
	conf->aio_o.sData.i8uDirection = 1;

	pr_info("MIO configured(addr:%d, ent-cnt:%d, conf-no:%d, conf-base:%d, \
dio hdr:0x%x,aio_i hdr:0x%x, aio_o hdr:0x%x)\n",
			addr, ent_cnt, mio_conf_cnt, MIO_CONF_BASE,
			*(unsigned short*)&mio_conf_list[mio_conf_cnt].dio.uHeader,
			*(unsigned short*)&mio_conf_list[mio_conf_cnt].aio_i.uHeader,
			*(unsigned short*)&mio_conf_list[mio_conf_cnt].aio_o.uHeader);

	for (i = 0; i < 0; i++) {
		offset = ent[i].i16uOffset;
		pr_info("to deal with entity with offset:%d\n", offset);
		switch (offset) {
		case 0 ... MIO_CONF_BASE - 1:
			/*nothing to do for input and output */
			break;

		case MIO_CONF_DIO_DIR:
			conf->dio.sData.i8uIoConfig = ent[i].i32uDefault;
			break;
		case MIO_CONF_DIO_IMOD:
			conf->dio.sData.i8uInputMode = ent[i].i32uDefault;
			break;
		case MIO_CONF_DIO_PUL:
			conf->dio.sData.i8uPullup = ent[i].i32uDefault;
			break;
		case MIO_CONF_DIO_OMOD:
			conf->dio.sData.i8uOutputMode = ent[i].i32uDefault;
			break;
		case MIO_CONF_DIO_FREQ ... MIO_CONF_DIO_PLEN - 1:
			conf->dio.sData.i16uPwmFrequency[offset - MIO_CONF_DIO_FREQ]
				= ent[i].i32uDefault;
			break;
		case MIO_CONF_DIO_PLEN ... MIO_CONF_DIO_TRIG - 1:
			conf->dio.sData.i16uPulseLength[offset - MIO_CONF_DIO_FREQ]
														  = ent[i].i32uDefault;
			break;
		case MIO_CONF_DIO_TRIG:
			conf->dio.sData.i8uPulseRetriggerMode = ent[i].i32uDefault;
			break;
		case MIO_CONF_AIO_IN ... MIO_CONF_AIO_OUT - 1:
			conf->aio_i.sData.i8uDirection = 0; /*0=input (InputThreshold)*/
			conf->aio_i.sData.i16uVoltage[offset - MIO_CONF_AIO_IN]
														  = ent[i].i32uDefault;
			break;
		case MIO_CONF_AIO_OUT ... MIO_CONF_END - 1:
			conf->aio_o.sData.i8uDirection = 1;/*1=output(Fixed Output)*/
			conf->aio_o.sData.i16uVoltage[offset - MIO_CONF_AIO_OUT]
													      = ent[i].i32uDefault;
			break;
		default:
			pr_warn("unknown config entry(offset:%d)\n", offset);
			break;
		}
	}
	conf->dio.i8uCrc = revpi_crc8(&conf->dio, sizeof(conf->dio) - 1);
	conf->aio_i.i8uCrc = revpi_crc8(&conf->aio_i, sizeof(conf->aio_i) - 1);
	conf->aio_o.i8uCrc = revpi_crc8(&conf->aio_o, sizeof(conf->aio_o) - 1);

	mio_conf_cnt++;

	return 0;
}

int revpi_mio_init(unsigned char devno)
{
	int i, ret;
	struct mio_config *conf = NULL;
	SMioConfigResponse resp;
	unsigned char crc;
	unsigned char addr;

	addr = RevPiDevice_getDev(devno)->i8uAddress;

	pr_info("MIO Initializing...(devno:%d, addr:%d)\n", devno, addr);

	for (i = 0; i < mio_conf_cnt; i++) {
		pr_info("search mio conf(index:%d, addr:%d)\n", i,
			mio_conf_list[i].dio.uHeader.sHeaderTyp1.bitAddress);

		if (mio_conf_list[i].dio.uHeader.sHeaderTyp1.bitAddress == addr) {

			conf = &mio_conf_list[i];

			/*the address of dio and aio (in and out) should be the same*/
			if ((conf->aio_i.uHeader.sHeaderTyp1.bitAddress != addr )
				|| (conf->aio_o.uHeader.sHeaderTyp1.bitAddress != addr )) {

				pr_warn("addr differ in mio conf(dio:%d, aio in:%d, aio out:%d)\n",
								addr,
								conf->aio_i.uHeader.sHeaderTyp1.bitAddress,
								conf->aio_o.uHeader.sHeaderTyp1.bitAddress);
			}
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
	if(ret) {
		pr_err("talk with mio for conf dio err(devno:%d, ret:%d)\n", devno, ret);
	}
	pr_info("headers of mio:aio conf request: 0x%4x, response:0x%4x\n",
										*(unsigned short*)&conf->dio.uHeader,
										*(unsigned short*)&resp.uHeader);

	crc = revpi_crc8(&resp, sizeof(resp) - 1);
	if (resp.i8uCrc != crc) {
		pr_err("crc for mio:dio conf err(got:%d, exp:%d)\n", resp.i8uCrc, crc);
	}

	/*aio out*/
	memset(&resp, 0, sizeof(resp));
	ret = revpi_io_talk(&conf->aio_i, sizeof(conf->aio_i), &resp, sizeof(resp));
	if(ret) {
		pr_err("talk with mio for conf aio_i err(devno:%d, ret:%d)\n", devno, ret);
	}
	pr_info("headers of mio:aio_i conf request: 0x%4x, response:0x%4x\n",
										*(unsigned short*)&conf->aio_i.uHeader,
										*(unsigned short*)&resp.uHeader);

	crc = revpi_crc8(&resp, sizeof(resp) - 1);
	if (resp.i8uCrc != crc) {
		pr_err("crc for mio:aio_i conf err(got:%d, exp:%d)\n",resp.i8uCrc, crc);
	}

	/*aio out*/
	memset(&resp, 0, sizeof(resp));
	ret = revpi_io_talk(&conf->aio_o, sizeof(conf->aio_o), &resp, sizeof(resp));
	if(ret) {
		pr_err("talk with mio for conf aio_o err(devno:%d, ret:%d)\n", devno, ret);
	}
	pr_info("headers of mio:aio_o conf request: 0x%4x, response:0x%4x\n",
						*(unsigned short*)&conf->aio_o.uHeader,
						*(unsigned short*)&resp.uHeader);

	crc = revpi_crc8(&resp, sizeof(resp) - 1);
	if (resp.i8uCrc != crc) {
		pr_err("crc for mio:aio_o conf err(got:%d, exp:%d)\n", resp.i8uCrc, crc);
	}

	pr_info("MIO Initializing finished(devno:%d, addr:%d)\n", devno, addr);
	return 0;
}

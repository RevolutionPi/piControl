/*
 * Copyright (C) 2020 KUNBUS GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */
#ifndef _REVPI_MIO_H_
#define _REVPI_MIO_H_

#include <common_define.h>
#include <IoProtocol.h>

/************************************************/

#define REVPI_MIO_MAX	10

#define MIO_CONF_BASE	sizeof(SMioDigitalRequestData) + \
			sizeof(SMioAnalogRequestData) + \
			sizeof(SMioDigitalResponseData) + \
			sizeof(SMioAnalogResponseData)

#define MIO_CONF_EMOD	MIO_CONF_BASE
#define MIO_CONF_IOMOD	(MIO_CONF_EMOD + 1)
#define MIO_CONF_PUL	(MIO_CONF_IOMOD + sizeof(INT8U) * MIO_DIO_PORT_CNT)
#define MIO_CONF_PMOD	(MIO_CONF_PUL + 1)
#define MIO_CONF_FPWM	(MIO_CONF_PMOD + 1)
#define MIO_CONF_PLEN	(MIO_CONF_FPWM + sizeof(INT16U) * MIO_PWM_TMR_CNT)
#define MIO_CONF_AIM	(MIO_CONF_PLEN + sizeof(INT16U) * MIO_DIO_PORT_CNT)
#define MIO_CONF_THR	(MIO_CONF_AIM + 1)
#define MIO_CONF_WSIZE	(MIO_CONF_THR + sizeof(INT16U) * MIO_AIO_PORT_CNT)
#define MIO_CONF_AOM	(MIO_CONF_WSIZE + 1)
#define MIO_CONF_OUTV	(MIO_CONF_AOM + 1)
#define MIO_CONF_END	(MIO_CONF_OUTV + sizeof(INT16U) * MIO_AIO_PORT_CNT)

/*
because of the limitation of length field in UIoProtocolHeader, which has 5 bits,
only maximum 31 bytes of data can be transmitted once, so the mio configurations
is transmitted in 3 times.

to make the struct mio_config can be directly used as the buffer for the
request  IOP_TYP1_CMD_CFG(digital) and IOP_TYP1_CMD_DATA4(analog), complete
structs for digital, analog input and analog output configurations are put here,
despite of the repeated headers
*/
struct mio_config {
	SMioDIOConfig dio;   /*digital configuration*/
	SMioAIOConfig aio_i; /*analog configuration for input*/
	SMioAIOConfig aio_o; /*analog configuration for output*/
};

struct mio_img_out {
	SMioDigitalRequestData dio;
	SMioAnalogRequestData aio;
};

struct mio_img_in {
	SMioDigitalResponseData dio;
	SMioAnalogResponseData aio;
};


int revpi_mio_init(unsigned char devno);
int revpi_mio_config(unsigned char addr, unsigned short ent_cnt, SEntryInfo *ent);
int revpi_mio_reset(void);
int revpi_mio_cycle(unsigned char devno);
#endif /* _REVPI_MIO_H_ */



#pragma once

#include <common_define.h>
#include <IoProtocol.h>

/************************************************/

#define REVPI_MIO_MAX 					10

#define MIO_CONF_BASE	sizeof(SMioDigitalRequestData) + \
							sizeof(SMioAnalogRequestData) + \
							sizeof(SMioDigitalResponseData) + \
							sizeof(SMioAnalogResponseData)

#define MIO_CONF_DIO_DIR	MIO_CONF_BASE
/*60*/

#define MIO_CONF_DIO_IMOD	(MIO_CONF_DIO_DIR + 1)
/*61*/

#define MIO_CONF_DIO_PUL	(MIO_CONF_DIO_IMOD + 1)
/*62*/

#define MIO_CONF_DIO_OMOD	(MIO_CONF_DIO_PUL + 1)
/*63*/

#define MIO_CONF_DIO_FREQ	(MIO_CONF_DIO_OMOD + 1)
/*64*/

#define MIO_CONF_DIO_PLEN (MIO_CONF_DIO_FREQ + sizeof(INT16U) * MIO_PWM_TMR_CNT)
/*70*/

#define MIO_CONF_DIO_TRIG (MIO_CONF_DIO_PLEN + sizeof(INT16U) * MIO_DIO_PORT_CNT)
/*78*/

//TODO: Code review, confirm with Frank B, the switch SMioAIOConfigData.i8uDirection not visible to pictory, is this ok?
#define MIO_CONF_AIO_IN	 (MIO_CONF_DIO_TRIG + 1)
/*79*/

#define MIO_CONF_AIO_OUT (MIO_CONF_AIO_IN + sizeof(INT16U) * MIO_AIO_PORT_CNT)
/*95*/

#define MIO_CONF_END	(MIO_CONF_AIO_OUT + sizeof(INT16U) * MIO_AIO_PORT_CNT)
/*111*/


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

typedef struct {
	SMioDigitalRequestData dio;
	SMioAnalogRequestData aio;
} mio_process_image_out;

typedef struct {
	SMioDigitalResponseData dio;
	SMioAnalogResponseData aio;
} mio_process_image_in;


int revpi_mio_init(unsigned char devno);
int revpi_mio_config(unsigned char addr, unsigned short ent_cnt, SEntryInfo *ent);
int revpi_mio_reset(void);
int revpi_mio_cycle(unsigned char devno);
int revpi_mio_calibrate(SAIOCalibrate *cali);

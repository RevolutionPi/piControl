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

#ifndef BSPCONFIG_H_INC
#define BSPCONFIG_H_INC

//#define ENDTEST_DIO

#define PRINT_MODGATE_COM_STATE
#if 0
#define pr_info_modgate(fmt, ...)	pr_info(fmt, ##__VA_ARGS__)
#else
#define pr_info_modgate(fmt, ...)
#endif

#if 0
#define DEBUG_CONFIG
#define pr_info_config(fmt, ...)	pr_info(fmt, ##__VA_ARGS__)
#else
#define pr_info_config(fmt, ...)
#endif

#if 0
#define DEBUG_MASTER_STATE
#define pr_info_master(fmt, ...)	pr_info(fmt, ##__VA_ARGS__)
#define pr_info_master2(fmt, ...)	pr_info(fmt, ##__VA_ARGS__)
#elif 1
#define DEBUG_MASTER_STATE
#define pr_info_master(fmt, ...)	pr_info(fmt, ##__VA_ARGS__)
#define pr_info_master2(fmt, ...)
#else
#define pr_info_master(fmt, ...)
#define pr_info_master2(fmt, ...)
#endif

#if 0
#define DEBUG_DEVICE_DIO
#define pr_info_dio(fmt, ...)	pr_info(fmt, ##__VA_ARGS__)
#else
#define pr_info_dio(fmt, ...)
#endif

#if 0
#define DEBUG_DEVICE_AIO
#define pr_info_aio(fmt, ...)	pr_info(fmt, ##__VA_ARGS__)
#else
#define pr_info_aio(fmt, ...)
#endif

#if 0
#define DEBUG_LINUX_DRV
#define pr_info_drv(fmt, ...)	pr_info(fmt, ##__VA_ARGS__)
#else
#define pr_info_drv(fmt, ...)
#endif

#if 0
#define pr_info_spi(fmt, ...)	pr_info(fmt, ##__VA_ARGS__)
#define pr_info_spi2(fmt, ...)	pr_info(fmt, ##__VA_ARGS__)
#elif 0
#define DEBUG_DEVICE_SPI
extern int __debug_show_msg;
#define pr_info_spi(fmt, ...)	pr_info(fmt, ##__VA_ARGS__)
#define pr_info_spi2(fmt, ...)	if (__debug_show_msg) pr_info(fmt, ##__VA_ARGS__)
#else
#define pr_info_spi(fmt, ...)
#define pr_info_spi2(fmt, ...)
#endif

//#define DEBUG_SERIALCOMM
#define DEBUG_GPIO
#define DEBUG_DEVICE
#define DEBUG_DEVICE_IO

#define SPI_MODULE	"spi_bcm2835"
#define SPI_BUS         0
#define RT_PRIO_UART    59
#define RT_PRIO_BRIDGE  58
#define RT_PRIO_GATE    60

#define INTERVAL_PI_GATE    ( 5*1000*1000)     //  5   ms    piGateCommunication

#ifdef DEBUG_SERIALCOMM
// use longer intervals to reduce the number of messages
#define INTERVAL_RS485      ( 1*1000*1000)     //  100 ms    piRs485
#define INTERVAL_IO_COMM    ( 5*1000*1000)     //  500 ms    piIoComm
#define INTERVAL_ADDITIONAL ( 500*1000)     //  500 ms    piIoComm
#else
#define INTERVAL_RS485      ( 1*1000*1000)     //  1   ms    piRs485
#define INTERVAL_IO_COMM    ( 5*1000*1000)     //  5   ms    piIoComm
#define INTERVAL_ADDITIONAL (    500*1000)     //  0.5 ms    piIoComm
#endif

#define KB_PD_LEN       512
#define KB_PI_LEN       4096

#define GPIO_RESET		40
#define GPIO_CS_KSZ0		35
#define GPIO_CS_KSZ1		36
#define GPIO_SNIFF1A		42
#define GPIO_SNIFF1B		43
#define GPIO_SNIFF2A		28
#define GPIO_SNIFF2B		29
#define KSZ8851_SPI_PORT	 0      // we use SPI port 0 for both sides


//#define VERBOSE
//#define DF_PRINTK(...)      printk(KBUILD_MODNAME ": " __VA_ARGS__)

#undef pr_fmt
#define pr_fmt(fmt)     KBUILD_MODNAME ": " fmt

//#define MEASURE_DURATION
#ifdef MEASURE_DURATION
#define DURSTART(x)	x = kbUT_getCurrentMs()
#define DURSTOP(x)	x = (kbUT_getCurrentMs() - x); \
			if (x < 0x80000000 && x##_max < x) \
			{ \
				x##_max = x; \
				pr_info("max " #x " %u\n", x##_max); \
			}
#endif

#endif // BSPCONFIG_H_INC



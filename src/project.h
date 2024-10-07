/* SPDX-License-Identifier: GPL-2.0-only
 * SPDX-FileCopyrightText: 2016-2024 KUNBUS GmbH
 */

#ifndef BSPCONFIG_H_INC
#define BSPCONFIG_H_INC

//#define ENDTEST_DIO

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
#define DEBUG_SERIALCOMM
#define pr_info_serial(fmt, ...)	pr_info(fmt, ##__VA_ARGS__)
#else
#define pr_info_serial(fmt, ...)	pr_info(fmt, ##__VA_ARGS__)
#endif

//#define DEBUG_GPIO
#define DEBUG_DEVICE
#define DEBUG_DEVICE_IO

/*
 * The priority has to be at least 54 (same as SPI thread), otherwise lots
 * of UART reception errors are observed. The reason for this effect is not
 * yet completely clear, presumably the UART has to switch as quickly as
 * possible from data transmission to data reception to avoid errors during
 * piBridge communication.
*/
#define RT_PRIO_BRIDGE  54

#ifdef DEBUG_SERIALCOMM
// use longer intervals to reduce the number of messages
#define INTERVAL_RS485      ( 1*1000*1000)     //  100 ms    piRs485
#define INTERVAL_IO_COMM    ( 5*1000*1000)     //  500 ms    piIoComm
#define INTERVAL_ADDITIONAL (    500*1000)     //  500 ms    piIoComm
#else
#define INTERVAL_RS485      ( 1*1000*1000)     //  1   ms    piRs485
#define INTERVAL_IO_COMM    ( 5*1000*1000)     //  5   ms    piIoComm
#define INTERVAL_ADDITIONAL (    500*1000)     //  0.5 ms    piIoComm
#endif

#define KB_PD_LEN       (u16)512
#define KB_PI_LEN       4096

//#define VERBOSE

#undef pr_fmt
#define pr_fmt(fmt)     KBUILD_MODNAME ": " fmt

#endif // BSPCONFIG_H_INC

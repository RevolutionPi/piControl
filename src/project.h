/* SPDX-License-Identifier: GPL-2.0-only
 * SPDX-FileCopyrightText: 2016-2024 KUNBUS GmbH
 */

#ifndef BSPCONFIG_H_INC
#define BSPCONFIG_H_INC

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

#define pr_info_serial(fmt, ...)	pr_info(fmt, ##__VA_ARGS__)

//#define DEBUG_GPIO
#define DEBUG_DEVICE
#define DEBUG_DEVICE_IO

#define KB_PD_LEN       (u16)512
#define KB_PI_LEN       4096

#undef pr_fmt
#define pr_fmt(fmt)     KBUILD_MODNAME ": " fmt

#endif // BSPCONFIG_H_INC

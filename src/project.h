/* SPDX-License-Identifier: GPL-2.0-only
 * SPDX-FileCopyrightText: 2016-2024 KUNBUS GmbH
 */

#ifndef BSPCONFIG_H_INC
#define BSPCONFIG_H_INC


#define DEBUG_MASTER_STATE
#define pr_info_master(fmt, ...)	pr_info(fmt, ##__VA_ARGS__)



#define pr_info_serial(fmt, ...)	pr_info(fmt, ##__VA_ARGS__)

#define KB_PD_LEN       (u16)512
#define KB_PI_LEN       4096

#undef pr_fmt
#define pr_fmt(fmt)     KBUILD_MODNAME ": " fmt

#endif // BSPCONFIG_H_INC

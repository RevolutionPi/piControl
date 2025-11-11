/* SPDX-License-Identifier: GPL-2.0-only
 * SPDX-FileCopyrightText: 2020-2023 KUNBUS GmbH
 */

#ifndef _REVPI_FLAT_H
#define _REVPI_FLAT_H

#include <linux/platform_device.h>

int revpi_flat_probe(struct platform_device *pdev);
void revpi_flat_remove(struct platform_device *pdev);
int revpi_flat_reset(void);

#endif /* _REVPI_FLAT_H */

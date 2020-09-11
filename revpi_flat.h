/*
 * Copyright (C) 2020 KUNBUS GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#ifndef _REVPI_FLAT
#define _REVPI_FLAT

#include <linux/types.h>
#include "piControl.h"
#include "common_define.h"
#include <linux/completion.h>
#include <linux/iio/consumer.h>

struct revpi_flat_config {
	unsigned int offset;
};

struct revpi_flat_image {
	struct {
	} __attribute__ ((__packed__)) drv;
	struct {
		u8 dout;
	} __attribute__ ((__packed__)) usr;
} __attribute__ ((__packed__));

struct revpi_flat {
	struct revpi_flat_image image;
	struct revpi_flat_config config;
	struct task_struct *dout_thread;
	struct device *din_dev;
	struct gpio_desc *dout_fault;
	struct gpio_desc *digout;

	struct gpio_descs *dout;
};

int revpi_flat_init(void);
void revpi_flat_fini(void);
#endif /* _REVPI_FLAT */

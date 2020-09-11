/*
 * Copyright (C) 2020 KUNBUS GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#include "revpi_flat.h"

#include <linux/device.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <uapi/linux/sched/types.h>

#include "revpi_common.h"
#include "project.h"
#include "piControlMain.h"

/* relais gpio num */
#define REVPI_FLAT_RELAIS_GPIO			28

#define REVPI_FLAT_DOUT_THREAD_PRIO		(MAX_USER_RT_PRIO / 2 + 8)

static const struct kthread_prio revpi_flat_kthread_prios[] = {
	/* softirq daemons handling hrtimers */
	{ .comm = "ktimersoftd/0",
	  .prio = MAX_USER_RT_PRIO/2 + 10
	},
	{ .comm = "ktimersoftd/1",
	  .prio = MAX_USER_RT_PRIO/2 + 10
	},
	{ .comm = "ktimersoftd/2",
	  .prio = MAX_USER_RT_PRIO/2 + 10
	},
	{ .comm = "ktimersoftd/3",
	  .prio = MAX_USER_RT_PRIO/2 + 10
	},
	{ }
};

static int revpi_flat_poll_dout(void *data)
{
	struct revpi_flat *flat = (struct revpi_flat *) data;
	struct revpi_flat_image *image = &flat->image;
	struct revpi_flat_image *usr_image;
	int dout_val = -1;

	usr_image = (struct revpi_flat_image *) (piDev_g.ai8uPI +
						 flat->config.offset);
	while (!kthread_should_stop()) {
		my_rt_mutex_lock(&piDev_g.lockPI);
		usr_image->drv = image->drv;

		if (usr_image->usr.dout != image->usr.dout)
			dout_val = usr_image->usr.dout;

		image->usr = usr_image->usr;
		rt_mutex_unlock(&piDev_g.lockPI);

		if (dout_val != -1) {
			gpiod_set_value_cansleep(flat->digout, !!dout_val);
			dout_val = -1;
		}

		usleep_range(100, 150);
	}

	return 0;
}


int revpi_flat_init(void)
{
	struct revpi_flat *flat;
	struct sched_param param = { };
	int ret;

	flat = devm_kzalloc(piDev_g.dev, sizeof(*flat), GFP_KERNEL);
	if (!flat)
		return -ENOMEM;

	piDev_g.machine = flat;

	flat->digout = gpio_to_desc(REVPI_FLAT_RELAIS_GPIO);
	if (!flat->digout) {
		dev_err(piDev_g.dev, "no gpio desc for digital output found\n");
		return -ENXIO;
	}

	ret = gpiod_direction_output(flat->digout, 0);
	if (ret) {
		dev_err(piDev_g.dev, "Failed to set direction for relais "
			"gpio %i\n", ret);
		return -ENXIO;
	}

	flat->dout_thread = kthread_create(&revpi_flat_poll_dout, flat,
					   "piControl dout");
	if (IS_ERR(flat->dout_thread)) {
		dev_err(piDev_g.dev, "cannot create dout thread\n");
		ret = PTR_ERR(flat->dout_thread);
		return ret;
	}

	param.sched_priority = REVPI_FLAT_DOUT_THREAD_PRIO;
	ret = sched_setscheduler(flat->dout_thread, SCHED_FIFO, &param);
	if (ret) {
		dev_err(piDev_g.dev, "cannot upgrade dout thread priority\n");
		goto err_stop_dout_thread;
	}

	ret = set_kthread_prios(revpi_flat_kthread_prios);
	if (ret)
		goto err_stop_dout_thread;

	wake_up_process(flat->dout_thread);

	return 0;

err_stop_dout_thread:
	kthread_stop(flat->dout_thread);

	return ret;
}

void revpi_flat_fini(void)
{
	struct revpi_flat *flat = (struct revpi_flat *) piDev_g.machine;

	kthread_stop(flat->dout_thread);
}

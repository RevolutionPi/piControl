/* SPDX-License-Identifier: GPL-2.0-only
 * SPDX-FileCopyrightText: 2024 KUNBUS GmbH
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM picontrol

#include <linux/tracepoint.h>

#if !defined(_PICONTROL_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _PICONTROL_TRACE_H


/*
 * picontrol_cycle_start
 *
 * Info: The beginning of a cycle of data exchange with all devices.
 * cycle: The current cycle.
 * Time: At the beginning of a cycle of data exchange with all devices.
 */
TRACE_EVENT(picontrol_cycle_start,
	TP_PROTO(u64 cycle),
	TP_ARGS(cycle),
	TP_STRUCT__entry(
		__field(u64, cycle)
	),
	TP_fast_assign(
		__entry->cycle = cycle;
	),
	TP_printk(
		"cycle=%llu ",
		__entry->cycle
	)
);

/*
 * picontrol_cycle_end
 *
 * Info: The end of a cycle of data exchange with all devices.
 * cycle: The current cycle.
 * duration: The duration of the current cycle.
 * Time: At the end of a cycle of data exchange with all devices.
 */
TRACE_EVENT(picontrol_cycle_end,
	TP_PROTO(u64 cycle, unsigned int duration),
	TP_ARGS(cycle, duration),
	TP_STRUCT__entry(
		__field(u64, cycle)
		__field(unsigned int, duration)
	),
	TP_fast_assign(
		__entry->cycle = cycle;
		__entry->duration = duration;
	),
	TP_printk(
		"cycle=%llu, duration=%u usecs",
		__entry->cycle,
		__entry->duration
	)
);

/*
 * picontrol_cycle_exceeded
 *
 * Info: The cycle equals a certain time span.
 * exceeded: The current cycle length.
 * min: The current (measured) min cycle length.
 * Time: At the end of a data cycle if the current cycle exceeds a certain time
 *       span.
 */
TRACE_EVENT(picontrol_cycle_exceeded,
	TP_PROTO(unsigned int exceeded, unsigned int min),
	TP_ARGS(exceeded, min),
	TP_STRUCT__entry(
		__field(unsigned int, exceeded)
		__field(unsigned int, min)
	),
	TP_fast_assign(
		__entry->exceeded = exceeded;
		__entry->min = min;
	),
	TP_printk("Cycle time exceeded: %u usecs (min: %u usecs)",
		__entry->exceeded,
		__entry->min
	)
);

/*
 * picontrol_cyclic_device_data_class
 *
 * Print the address of the device involved in data exchange.
 *
 * addr: the device address.
 */
DECLARE_EVENT_CLASS(picontrol_cyclic_device_data_class,
	TP_PROTO(unsigned int addr),
	TP_ARGS(addr),
	TP_STRUCT__entry(
		__field(unsigned int, addr)
	),
	TP_fast_assign(
		__entry->addr = addr;
	),
	TP_printk(
		"Data exchange dev %u",
		__entry->addr
	)
);

/*
 * picontrol_cyclic_device_data_start
 *
 * Info: The device starting to exchange data.
 * Time: Before a device starts to exchange data in this cycle.
 */
DEFINE_EVENT(picontrol_cyclic_device_data_class, picontrol_cyclic_device_data_start,
	TP_PROTO(unsigned int addr),
	TP_ARGS(addr)
);

/*
 * picontrol_cyclic_device_data_stop
 *
 * Info: The device stopping to exchange data
 * Time: After a device stopped to exchange data in this cycle.
 */
DEFINE_EVENT(picontrol_cyclic_device_data_class, picontrol_cyclic_device_data_stop,
	TP_PROTO(unsigned int addr),
	TP_ARGS(addr)
);

DECLARE_EVENT_CLASS(picontrol_sniffpin_value_class,
	TP_PROTO(unsigned int value),
	TP_ARGS(value),
	TP_STRUCT__entry(
		__field(unsigned int, value)
	),
	TP_fast_assign(
		__entry->value = value;
	),
	TP_printk("Sniff-Pin value: %u",
		__entry->value
	)
);

DEFINE_EVENT(picontrol_sniffpin_value_class, picontrol_sniffpin_1a_read,
	TP_PROTO(unsigned int value),
	TP_ARGS(value)
);

DEFINE_EVENT(picontrol_sniffpin_value_class, picontrol_sniffpin_2a_read,
	TP_PROTO(unsigned int value),
	TP_ARGS(value)
);

DEFINE_EVENT(picontrol_sniffpin_value_class, picontrol_sniffpin_1b_read,
	TP_PROTO(unsigned int value),
	TP_ARGS(value)
);

DEFINE_EVENT(picontrol_sniffpin_value_class, picontrol_sniffpin_2b_read,
	TP_PROTO(unsigned int value),
	TP_ARGS(value)
);

DEFINE_EVENT(picontrol_sniffpin_value_class, picontrol_sniffpin_1a_set,
	TP_PROTO(unsigned int value),
	TP_ARGS(value)
);

DEFINE_EVENT(picontrol_sniffpin_value_class, picontrol_sniffpin_2a_set,
	TP_PROTO(unsigned int value),
	TP_ARGS(value)
);

DEFINE_EVENT(picontrol_sniffpin_value_class, picontrol_sniffpin_1b_set,
	TP_PROTO(unsigned int value),
	TP_ARGS(value)
);

DEFINE_EVENT(picontrol_sniffpin_value_class, picontrol_sniffpin_2b_set,
	TP_PROTO(unsigned int value),
	TP_ARGS(value)
);

#endif /* _PICONTROL_TRACE_H */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE picontrol_trace
#include <trace/define_trace.h>

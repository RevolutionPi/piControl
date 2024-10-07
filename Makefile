# SPDX-License-Identifier: GPL-2.0-only
# SPDX-FileCopyrightText: 2016-2024 KUNBUS GmbH

obj-m   := piControl.o

#add other objects e.g. test.o
piControl-objs  = src/piControlMain.o
piControl-objs += src/piIOComm.o
piControl-objs += src/piDIOComm.o
piControl-objs += src/piAIOComm.o
piControl-objs += src/RevPiDevice.o
piControl-objs += src/json.o
piControl-objs += src/piConfig.o
piControl-objs += src/RS485FwuCommand.o
piControl-objs += src/piFirmwareUpdate.o
piControl-objs += src/PiBridgeMaster.o
piControl-objs += src/kbUtilities.o
piControl-objs += src/systick.o
piControl-objs += src/revpi_common.o
piControl-objs += src/revpi_compact.o
piControl-objs += src/revpi_core.o
piControl-objs += src/revpi_gate.o
piControl-objs += src/revpi_flat.o
piControl-objs += src/pt100.o
piControl-objs += src/revpi_mio.o
piControl-objs += src/revpi_ro.o

ccflags-y := -O2
ccflags-y += -I$(src)/src
ccflags-$(_ACPI_DEBUG) += -DACPI_DEBUG_OUTPUT

KBUILD_CFLAGS += -g

PWD   	:= $(shell pwd)

EXTRA_CFLAGS += -D__KUNBUSPI_KERNEL__ -I$(src)

all:
	$(MAKE) -C $(KDIR) M=$(PWD)  modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f $(piControl-objs)

# SPDX-License-Identifier: GPL-2.0-only
# SPDX-FileCopyrightText: 2016-2024 KUNBUS GmbH

obj-m := piControl.o

#add other objects e.g. test.o
piControl-y  = src/piControlMain.o
piControl-y += src/piIOComm.o
piControl-y += src/piDIOComm.o
piControl-y += src/piAIOComm.o
piControl-y += src/RevPiDevice.o
piControl-y += src/json.o
piControl-y += src/piConfig.o
piControl-y += src/RS485FwuCommand.o
piControl-y += src/piFirmwareUpdate.o
piControl-y += src/PiBridgeMaster.o
piControl-y += src/kbUtilities.o
piControl-y += src/systick.o
piControl-y += src/revpi_common.o
piControl-y += src/revpi_compact.o
piControl-y += src/revpi_core.o
piControl-y += src/revpi_gate.o
piControl-y += src/revpi_flat.o
piControl-y += src/pt100.o
piControl-y += src/revpi_mio.o
piControl-y += src/revpi_ro.o

ccflags-y := -O2
ccflags-y += -I$(src)/src
ccflags-y += -D__KUNBUSPI_KERNEL__ -I$(src)
ccflags-$(_ACPI_DEBUG) += -DACPI_DEBUG_OUTPUT

KBUILD_CFLAGS += -g

PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f $(piControl-y)

modules_install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install

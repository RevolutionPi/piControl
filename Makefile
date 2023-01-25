
obj-m   := piControl.o

#add other objects e.g. test.o
piControl-objs  = piControlMain.o
piControl-objs += piIOComm.o
piControl-objs += piDIOComm.o
piControl-objs += piAIOComm.o
piControl-objs += RevPiDevice.o
piControl-objs += json.o
piControl-objs += piConfig.o
piControl-objs += RS485FwuCommand.o
piControl-objs += piFirmwareUpdate.o
piControl-objs += PiBridgeMaster.o
piControl-objs += kbUtilities.o
piControl-objs += systick.o
piControl-objs += revpi_common.o
piControl-objs += revpi_compact.o
piControl-objs += revpi_core.o
piControl-objs += revpi_gate.o
piControl-objs += revpi_flat.o
piControl-objs += pt100.o
piControl-objs += revpi_mio.o

ccflags-y := -O2
ccflags-$(_ACPI_DEBUG) += -DACPI_DEBUG_OUTPUT

KBUILD_CFLAGS += -g

PWD   	:= $(shell pwd)

EXTRA_CFLAGS = -I$(src)/

EXTRA_CFLAGS += -D__KUNBUSPI_KERNEL__

.PHONY: compiletime.h

all: compiletime.h
	$(MAKE) -C $(KDIR) M=$(PWD)  modules

compiletime.h:
	echo "#define COMPILETIME \""`date`"\"" > compiletime.h

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f $(piControl-objs)


CC=/home/md/pi/tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64/bin/arm-linux-gnueabihf-gcc

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
piControl-objs += spi.o
piControl-objs += ksz8851.o
piControl-objs += ModGateComMain.o
piControl-objs += ModGateComError.o
piControl-objs += PiBridgeMaster.o
piControl-objs += kbUtilities.o
piControl-objs += kbAlloc.o
piControl-objs += systick.o
piControl-objs += revpi_compact.o

ccflags-y := -O2
ccflags-$(_ACPI_DEBUG) += -DACPI_DEBUG_OUTPUT

KDIR    := /home/md/pi/kernelpkg/kbuild7
KBUILD_CFLAGS += -g

PWD   	:= $(shell pwd)

EXTRA_CFLAGS = -I$(src)/

EXTRA_CFLAGS += -D__KUNBUSPI_KERNEL__


.PHONY: compiletime.h

all: compiletime.h
	$(MAKE) ARCH=arm -C $(KDIR) M=$(PWD) O=../kernelpkg/kbuild modules

compiletime.h:
	echo "#define COMPILETIME \""`date`"\"" > compiletime.h

clean:
	$(MAKE) ARCH=arm -C $(KDIR) M=$(PWD) clean
	rm -f $(piControl-objs)





# If KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.
ifneq ($(KERNELRELEASE),)
	obj-m += intspkr.o
	# https://tldp.org/LDP/lkmpg/2.6/html/x181.html
	intspkr-objs := ./src/intspkr.o ./src/spkr-io.o ./src/spkr-fifo.o ./src/spkr-fs.o
# Otherwise we were called directly from the command
# line; invoke the kernel build system.
else
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)
default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
endif
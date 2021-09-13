obj-y := syscalls.o
obj-m := elevator.o

PWD := $(shell pwd)
KDIR := /lib/modules/`uname -r`/build

default:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules

clean:
	rm -f *.o *.ko *.mod.* Module.* modules.*

watch_proc:
	while [ 1 ]; do \
		clear; clear; \
		cat /proc/elevator; \
		sleep 1; \
	done
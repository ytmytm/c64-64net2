
all: ioport

ioport:
	gcc -O1 -no-fpic -c driver.c -D_KERNEL_MODE=1
	gcc -o ioport driver.o -nostdlib /boot/develop/lib/x86/_KERNEL_
	mimeset -f ioport

install: ioport
	cp ioport ~/config/add-ons/kernel/drivers/bin/ioport \
		&& ln -fs ~/config/add-ons/kernel/drivers/bin/ioport \
		~/config/add-ons/kernel/drivers/dev/misc/ioport

uninstall:
	rm ~/config/add-ons/kernel/drivers/bin/ioport
	rm ~/config/add-ons/kernel/drivers/dev/misc/ioport

clean:
	-rm ioport driver.o


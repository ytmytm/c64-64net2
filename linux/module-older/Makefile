# Makefile for a basic kernel module

CC=egcs
MODCFLAGS = $(CFLAGS) -Wall -DCONFIG_KERNELD -DMODULE -D__KERNEL__ -DLINUX

fastpc.o:	fastpc.c
		$(CC) $(MODCFLAGS) -c fastpc.c

clean:
		rm -f *.o
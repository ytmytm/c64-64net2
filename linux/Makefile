# Makefile for a basic kernel module

CC=egcs
MODCFLAGS = -O2 -mpentiumpro -march=pentiumpro -Wall -DCONFIG_KERNELD -DMODULE -D__KERNEL__ -DLINUX

fastpc.o:	fastpc.c
		$(CC) $(MODCFLAGS) -c fastpc.c

clean:
		rm -f *.o
FSYS_OBJ=fs_error.o fs_media.o fs_readts.o fs_search.o fs_fileio.o fs_rawdir.o\
	fs_glob.o fs_parse.o fs_hostdep.o\
	fs_io_ufs.o fs_io_t64.o fs_io_lnx.o fs_io_net.o\
	fs_io_dxx.o fs_io_d64.o fs_io_d71.o fs_io_d81.o fs_io_dhd.o\
	dosemu.o dos_blockio.o misc_func.o debug.o version.o resman.o datestamp.o

COMM_OBJ=comm-lpt.o comm-work.o machdep.o fs_accel.o\
	client-comm.o clientdep.o

OBJECTS	= $(FSYS_OBJ) $(COMM_OBJ)

PROGS	= main_64net2.o main_rm.o main_list.o main_ls.o main_cat.o main_shell.c
XPROGS	= x/x64net.o

AR =ar
CC =gcc
XFLAGS =$(CFLAGS) -L/usr/X11R6/lib -lX11
LOPT=-L. -l64net2
COPT=$(CFLAGS) -Wall -pedantic
#-DUSE_LINUX_KERNEL_MODULE

all:	64net2 64rm 64ls 64list 64cat 64shell x64net

backup:	64net2.tar.gz
	tar zcvf 64net2.tgz *c *h Makefile devel*

#
#	Distribution build commands
#

freebsd-dist: 64net2
	tar zcvf /tmp/64net2fb.tgz 64net2 64netrc *.development cable.doc
msdos-dist:
	tar zcvf /tmp/64net2ms.tgz 64net2.exe 64netrc *.development cable.doc

lib64net2.a: $(OBJECTS)
	$(AR) r $@ $(OBJECTS)

#
# Directives to build actual binaries
#
# 64net2 - 64NET/2 Server
# 64rm   - Delete files in a 64net filesystem
# 64ls   - View a directory c64 style using the 64net filesystem
# 64list - Show the BASIC listing of a file using the 64net filesystem
# 64cat  - Show the contents of a file using the 64net filesystem
#

64net2:	lib64net2.a main_64net2.o
	$(CC) main_64net2.o -o 64net2 $(LOPT) 

64shell:lib64net2.a main_shell.o
	$(CC) main_shell.o -o 64shell $(LOPT)

64list:	lib64net2.a main_list.o
	$(CC) main_list.o -o 64list $(LOPT)

64rm:	lib64net2.a main_rm.o
	$(CC) main_rm.o -o 64rm $(LOPT)

64ls:	lib64net2.a main_ls.o
	$(CC) main_ls.o -o 64ls $(LOPT)

64cat:	lib64net2.a main_cat.o
	$(CC) main_cat.o -o 64cat $(LOPT)

x64net: x/15xx.xpm
	$(CC) $(XFLAGS) -o x64net x/x64net.c

#
# all modules
#

%.o:	%.c
	$(CC) -c -o $@ $(COPT) $<

#
# purity is needed
#

clean:
	rm *.o *.a

spotless:	clean
	rm 64net2 64shell 64ls 64rm 64list 64cat x64net

zap:	spotless


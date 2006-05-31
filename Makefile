FSYS_OBJ=fs_error.o fs_media.o fs_readts.o fs_search.o fs_fileio.o fs_rawdir.o\
	fs_glob.o fs_parse.o fs_hostdep.o\
	fs_io_ufs.o fs_io_t64.o fs_io_lnx.o fs_io_net.o\
	fs_io_dxx.o fs_io_d64.o fs_io_d71.o fs_io_d81.o fs_io_dhd.o\
	dosemu.o dos_blockio.o misc_func.o debug.o version.o resman.o\
	datestamp.o

COMM_OBJ=comm-rrnet.o client-comm.o
# COMM_OBJ=comm-lpt.o client-comm.o

OBJECTS	= $(FSYS_OBJ) $(COMM_OBJ)

PROGS	= main_64net2.o main_rm.o main_list.o main_ls.o main_cat.o main_shell.c
XPROGS	= x/x64net.o

AR =ar
CC =gcc
XFLAGS =$(CFLAGS) -L/usr/X11R6/lib -lX11 -I/usr/X11R6/include
# This is for Linux and BeOS
LOPT=-L. -l64net2
# This is for Windows
#LOPT=-L. -l64net2 -lwsock32
# This is for Solaris
#LOPT=-L. -l64net2 -lsocket -lnsl
# This is for FreeBSD
#LOPT=-L. -l64net2 -lcompat
COPT=-g -O $(CFLAGS) -Wall

.PHONY: all clean spotless depend dep

ifeq (.depend,$(wildcard .depend))
all : 	bin/64net2 bin/64rm bin/64ls bin/64list bin/64cat \
	bin/64shell bin/x64net bin/build_wedge
include .depend
else
all:	depend
	@$(MAKE) all
endif

backup: spotless
	tar zcvf ../64net2.tar.gz $(PWD)

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

bin/64net2:	lib64net2.a main_64net2.o
	$(CC) main_64net2.o -o bin/64net2 $(LOPT) 

bin/64shell:lib64net2.a main_shell.o
	$(CC) main_shell.o -o bin/64shell $(LOPT)

bin/64list:	lib64net2.a main_list.o
	$(CC) main_list.o -o bin/64list $(LOPT)

bin/64rm:	lib64net2.a main_rm.o
	$(CC) main_rm.o -o bin/64rm $(LOPT)

bin/64ls:	lib64net2.a main_ls.o
	$(CC) main_ls.o -o bin/64ls $(LOPT)

bin/64cat:	lib64net2.a main_cat.o
	$(CC) main_cat.o -o bin/64cat $(LOPT)

bin/x64net: x/15xx.xpm
	$(CC) $(XFLAGS) -o bin/x64net x/x64net.c

bin/build_wedge:	tools/build_wedge.o
	$(CC) tools/build_wedge.o -o bin/build_wedge $(LOPT)

#
# all modules
#

%.o:	%.c Makefile
	$(CC) -c -o $@ $(COPT) $<

#
# purity is needed
#

clean:
	-rm *.o *.a tools/*.o .depend bin/*

spotless: clean
	-rm bin/*

depend dep:	$(OBJECTS:.o=.c)
	@echo "Creating dependency information"
	$(CC) -MM -MG $^ > .depend

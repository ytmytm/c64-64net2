
#include "misc_func.h"
#include "comm-lpt.h"

#ifdef __FreeBSD__
/* chip-bash includes */
#include <machine/sysarch.h>
#include <machine/cpufunc.h>
#endif

#ifdef LINUX
#include <sys/io.h>
#endif

#ifdef AMIGA
#include "amiga/my_exec_types.h"
#include <exec/semaphores.h>
#include <clib/exec_protos.h>
#include <clib/misc_protos.h>
#include <hardware/cia.h>
#include <resources/misc.h>
extern volatile struct CIA ciaa;
extern volatile struct CIA ciab;
#endif

#ifdef WINDOWS
#include <windows.h>
typedef void (CALLBACK * outb_t)(int,int);
typedef unsigned char (CALLBACK * inb_t)(int);
HINSTANCE dll_handle;
WSADATA wsa_data;
outb_t outb;
inb_t inb;
#endif

/* Faxe port access XXX */
#ifdef SOLARIS
int outb(int p,int v) { return 0; }
int inb(int p) { return 255; }
#endif /* SOLARIS */

#ifdef BEOS
#define DRV_READ_IO_8 'r'
#define DRV_WRITE_IO_8 'w'
#define DRV_READ_IO_16 'r16'
#define DRV_WRITE_IO_16 'w16'
int parport_fd;
typedef struct IO_Tuple {
        unsigned long Port;
        unsigned char  Data;
        unsigned short Data16;
} IO_Tuple;

int inb (int port) {
  IO_Tuple temp;

  temp.Port = port;
  ioctl(parport_fd, DRV_READ_IO_8, &temp, 0);
  return (temp.Data);
}

void outb(int port, int value) {
  IO_Tuple temp;

  temp.Port = port;
  temp.Data = value;
  ioctl(parport_fd, DRV_WRITE_IO_8, &temp, 0);
}
#endif /* BEOS */

/* Low-level functions definitions for each architecture */

#ifdef AMIGA
#define PAROUT ciaa.ciaddrb = 0xFF
#define PARIN ciaa.ciaddrb = 0x00
#define POUTHIGH ciab.ciapra = ((ciab.ciapra & 0xFD) | 0x02)
#define POUTLOW ciab.ciapra = ciab.ciapra & 0xFD
#define POUTRHIGH POUTHIGH
#define POUTWHIGH POUTHIGH
#define POUTRLOW  POUTLOW
#define POUTWLOW  POUTLOW
#define BUSY (!(ciab.ciapra & 0x01)) /* Inverted 'cause BUSY is active low on PC(!) */
#define PARW(x) ciaa.ciaprb = (x)
#define PARR ciaa.ciaprb
#define SETINITDDR ciab.ciaddra = (ciab.ciaddra & 0xF8) | 0x02
#else
#define PAROUT
#define PARIN
#define POUTHIGH
#define POUTLOW
#define PARR inb(portout)
#define BUSY (inb(portin) & 0x80)
#define ACK (inb(portin) & 0x40)
#ifdef LINUX
#define POUTRHIGH outb(0xa0, portout+2)
#define POUTWHIGH outb(0x40, portout+2)
#define POUTRLOW  outb(0xa1, portout+2)
#define POUTWLOW  outb(0x41, portout+2)
#define PARW(x) outb(x, portout)
#else
#define POUTRHIGH outb(portout+2, 0xa0)
#define POUTWHIGH outb(portout+2, 0x40)
#define POUTRLOW  outb(portout+2, 0xa1)
#define POUTWLOW  outb(portout+2, 0x41)
#define PARW(x) outb(portout, x)
#endif /* LINUX */
#endif /* AMIGA */

void
init_hw (void)
{
#ifdef __FreeBSD__
/* This is old privilege scheme, maybe it works - test it */
/*
 FILE *f;

 if ((f = fopen ("/dev/io", "rw")) == NULL) {
    fatal_error ("Cannot get chip-bash privilege.\
                 \nPlease setgid to kmem (or similar)\n");
    exit (1);
  }
*/
/* This is new one */
 int a;

 debug_msg("Asking for chip-bash privileges... ");
 a = i386_set_ioperm(portout,3,1);
 debug_msg("i386_set_ioperm returned = %i\n", a);
 if (a != 0) {
    fatal_error ("Cannot get io space privilege. Please setsgid to kmem (or similar)\n");
    exit (1);
  }

 fflush(stdout);
#endif /* FreeBSD */

#ifdef BEOS
  debug_msg("Opening ioport device to access parallel ports.\n");
  parport_fd = open("/dev/misc/ioport", O_RDWR | O_NONBLOCK);
  if (parport_fd < 0) {
	fatal_error ("Cannot open /dev/misc/ioport, install ioport driver first.\n");
	exit(1);
  }
  fflush(stdout);
#endif /* BEOS */

#ifdef LINUX
 int a;

 /* Enable port access to the range [portout] to [portout+3] */
 debug_msg("Asking for chip-bash privileges... ");
 a = ioperm(portout,3,3);
 debug_msg("ioperm returned = %i\n", a);
 if (a != 0) {
    fatal_error ("Cannot get io space privilege. Please setsgid to root (or similar)\n");
    exit (1);
  }

 fflush(stdout);
#endif /* LINUX */

#ifdef WINDOWS
  if (no_net != 1) {
    printf("Initializing wsock32.dll\n");
    WSAStartup(0x0101, &wsa_data);
  }
  dll_handle = LoadLibrary("inpout32");
  if (dll_handle!=NULL) {
    debug_msg("Opened inpout32.dll\n");
    outb = (outb_t)GetProcAddress(dll_handle,"Out32");
    inb = (inb_t)GetProcAddress(dll_handle,"Inp32");
     if (!outb || !inb)
	fatal_error("Failed to get functions adresses from Inpout32.dll");
  } else {
    fatal_error("Could not open inpout32.dll");
  }
#endif /* WINDOWS */

#ifdef AMIGA
  const char *user;
  char myname[] = "64NET/2 - Parallel";

  if (steal_parallel_port == 0)
  {
    debug_msg ("Opening misc resource\n");
    if ((MiscBase = OpenResource ("misc.resource")) == NULL)
	fatal_error ("Can't open misc.resource\n");

    debug_msg ("Allocating parallel port\n");
    user = AllocMiscResource (MR_PARALLELPORT, myname);
    if (user != NULL && user != myname)
      fatal_error ("Printer data lines already in use by %s.\nUnable to lock parallel port\n", user);
  }
  debug_msg ("Setting DDRs\n");
  PAROUT;			/* Set data lines for output initially */
  /* Set following control line DDR stuff
     Bit 2 - Sel - Input
     Bit 1 - Pout - Out
     Bit 0 - Busy - Input
   */
  SETINITDDR;
#endif /* AMIGA */
}

void
starthw(void)
{
  /* Parallel=in, Pout=high */
  /* ddr = in, strobe = high */
  PARIN;
  POUTRHIGH;
}

int
syncchar (void)
{
  int value;
  long c,d;

  PARIN;
sync_loop:
  POUTRHIGH;

  c = 0; d = 0;
  /* Set parallel port DDR = in, POUT=high */
#ifdef DEBUG2
  debug_msg ("syncchar - Waiting for 64 ACK (!BUSY)\n"); fflush (stdout);
#endif
  /* Wait for 64 ACK */
  while (!(BUSY))
  {
        c++;
	if (c > max_tries) {

	c=0;
	debug_msg ("z"); fflush (stdout);
	usleep (snooz_time);	/* dont hog cpu  */
	}
  }
  /* read value */
  value = PARR;
  /* Set POUT low, Parallel port DDR = in */
  POUTRLOW;
#ifdef DEBUG2
  printf ("syncchar - Waiting for 64 ACK (BUSY)\n");
  fflush (stdout);
#endif
  /* Wait for 64 ACK */
  while (BUSY)
  {
	c++;
	if (c > max_tries) {
	debug_msg ("x"); fflush (stdout);
	usleep (snooz_time_2);	/* dont hog cpu  */
	c=0; d++;
	if (d > synctolerance_2) goto sync_loop;
	}
  }
  /* second toggle */
  POUTRHIGH;
  POUTRLOW;

  return (value);
}

/* receive a character after a connection has been established */

int
charget (void)
{
  int value;

#ifdef DEBUG2
  debug_msg ("Trying to get char..."); fflush (stdout);
#endif
  /* Parallel port DDR = in , POUT = high */
  PARIN;
  POUTRHIGH;
#ifdef DEBUG2
  debug_msg ("getchar - Waiting for 64 ACK (!BUSY)\n"); fflush (stdout);
#endif
  /* Wait for 64 ACK */
  while (!(BUSY));
  /* read value */
  value = PARR;
  /* Toggle POUT low, Parallel port DDR = in */
  POUTRLOW;
#ifdef DEBUG2
  debug_msg ("getchar - Waiting for 64 ACK (BUSY)\n"); fflush (stdout);
#endif
  /* Wait for 64 ACK */
  while (BUSY);
  /* Second toggle */
  POUTRHIGH;
  POUTRLOW;
#ifdef DEBUG2
  debug_msg ("Got %02x\n", value); fflush (stdout);
#endif

  return (value);
}


/* send a single character back over the link */

int
sendchar (int byte)
{
#ifdef DEBUG2
  debug_msg ("Trying to send %02x...", byte); fflush (stdout);
  debug_msg ("sendchar - Waiting for 64 ACK (!BUSY)\n"); fflush (stdout);
#endif
  /* wait for 64 ready (93) */
  while (!(BUSY));
  /* POUT = high, Parallel port DDR = Out */
  POUTWHIGH;
  PAROUT;
  /* Write byte */
  PARW (byte);
  /* wait for 64 ready (97) */
  /* Bring POUT low */
  POUTWLOW;
#ifdef DEBUG2
  debug_msg ("sendchar - Waiting for 64 ACK (BUSY)\n"); fflush (stdout);
#endif
  while (BUSY);
  PARIN;
  POUTRHIGH;
  POUTRLOW;
  POUTRHIGH;
#ifdef DEBUG2
  debug_msg ("Sent\n");
#endif
  return (0);
}

//#define DEBUG2

int
fishsendblock (int size, uchar * block)
{
  int n = 0;

  PAROUT;
  POUTWHIGH;	/* set port DDR out, don't toggle strobe, this is for non-AMIGA */

  while (n < size)
  {
    /* wait for 64 ACK */
    while (!(BUSY));
    /* send char & toggle strobe */
    PARW (block[n]);
    POUTWLOW;
    POUTWHIGH;			/* raise here before busy loop to reduce port accesses */
    n++;
#ifdef DEBUG2
    printf("$%02x ",n);
#endif
    /* wait for 64 NACK */
    while (BUSY);
    PARW (block[n]);
    POUTWLOW;
    POUTWHIGH;
    n++;
#ifdef DEBUG2
    printf("$%02x ",n);
#endif
  }
  /* close off */
  while (!(BUSY));
  /* send ack */
  /* Parallel port DDR = in, POUT = low */
  PARIN;
  POUTRLOW;
  POUTRHIGH;

  /* FIXME - how to handle it??? it must be here and it must have \n at the end (flushing) */
  printf("delay\n");
  return (0);

}

int
fishgetblock (int size, uchar * block)
{
  int n = 0;

  PARIN;
  POUTRHIGH;	/* set port DDR in, don't toggle strobe, this is for non-AMIGA */
  usleep(1);

  while (n < size)
  {
    /* wait for 64 NACK */
    while (!(BUSY));
    /* get char & toggle POUT */
    block[n] = PARR;
    n++;
#ifdef DEBUG2
    printf("$%02x ",n);
#endif
    POUTRLOW;
    POUTRHIGH;			/* raise here before busy loop to reduce port accesses */
    /* wait for ACK */
    while (BUSY);
    /* get char & toggle POUT */
    block[n] = PARR;
    n++;
#ifdef DEBUG2
    printf("$%02x ",n);
#endif
    POUTRLOW;
    POUTRHIGH;
  }

  /* close off */
  while (!(BUSY));
  /* send ack */
  /* Parallel port DDR = in, POUT = low */
  PARIN;
  POUTRLOW;
  POUTRHIGH;

  usleep(5);
  return (0);

}

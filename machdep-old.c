
#include "misc_func.h"
#include "comm-lpt.h"
#include "debug.h"

#ifdef BSD
#include "bsd/cpufunc.h"
#endif /* BSD */

#ifdef LINUX
#include <asm/io.h>
#include <sys/io.h>
#endif /* LINUX */

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
#define PARR inb(outport)
#define BUSY (inb(inport) & 0x80)
#ifdef LINUX
#define POUTRHIGH outb(0xa0, outport+2)
#define POUTWHIGH outb(0x40, outport+2)
#define POUTRLOW  outb(0xa1, outport+2)
#define POUTWLOW  outb(0x41, outport+2)
#define PARW(x) outb(x, outport)
#else
#define POUTRHIGH outb(outport+2, 0xa0)
#define POUTWHIGH outb(outport+2, 0x40)
#define POUTRLOW  outb(outport+2, 0xa1)
#define POUTWLOW  outb(outport+2, 0x41)
#define PARW(x) outb(outport, x)
#endif /* LINUX */
#endif /* AMIGA */

#ifdef SOLARIS
  /* Faxe port access XXX */
int outb(int p,int v)
{ return 0; 
}

int inb(int p)
{
return 255;
}
#endif /* SOLARIS */

void
init_hw (void)
{
#ifdef BSD
 if ((f = fopen ("/dev/io", "rw")) == NULL)
  {
    fatal_error ("Cannot get chip-bash privilege.\
                 \nPlease setgid to kmem (or similar)\n");
    exit (1);
  }
#endif /* BSD */

#ifdef LINUX
 int a;

 /* Enable port access to the range [outport] to [outport+3] */
 printf("LINUX: Asking for chip-bash privileges\n");
 a = ioperm(outport,3,3);
 printf("ioperm returned = %i\n", a);
 if (a != 0)
  {
    fatal_error ("Cannot get io space privilege. Please setsgid to root (or similar)\n");
    exit (1);
  }

 fflush(stdout);
 sleep(0);

#endif /* LINUX */

#ifdef AMIGA
  const char *user;
  char myname[] = "64NET/2 - Parallel";

  if (steal_parallel_port == 0)
  {
#ifdef DEBUG
    printf ("Opening misc resource\n");
#endif /* DEBUG */

    if ((MiscBase = OpenResource ("misc.resource")) == NULL)
    {
      printf ("Can't open misc.resource\n");
      exit (10);
    }

#ifdef DEBUG
    printf ("Allocating parallel port\n");
#endif /* DEBUG */
    user = AllocMiscResource (MR_PARALLELPORT, myname);
    if (user != NULL && user != myname)
    {
      printf ("Printer data lines already in use by %s.\n", user);
      printf ("Unable to lock parallel port\n");
      exit (10);
    }
  }
#ifdef DEBUG
  printf ("Setting DDRs\n");
#endif /* DEBUG */
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

sync_loop:

  c = 0; d = 0;
  /* Set parallel port DDR = in, POUT=high */
  PARIN;
  POUTRHIGH;
#ifdef DEBUG2
  printf ("syncchar - Waiting for 64 ACK (!BUSY)\n");
  fflush (stdout);
#endif
  /* Wait for 64 ACK */
  while (!(BUSY))
  {
        c++;
	if (c > synctolerance) {

	c=0;
	debug_msg ("z"); fflush (stdout);
#ifdef AMIGA
	Delay (3);
#else
	usleep (snooz_time);	/* dont hog cpu  */
#endif
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
	if (c > synctolerance) {
	debug_msg ("x"); fflush (stdout);
#ifdef AMIGA
	Delay (3);
#else
	usleep (snooz_time_2);	/* dont hog cpu  */
#endif
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
  printf ("Trying to get char...");
  fflush (stdout);
#endif
  /* Parallel port DDR = in , POUT = high */
  PARIN;
  POUTRHIGH;
#ifdef DEBUG2
  printf ("getchar - Waiting for 64 ACK (!BUSY)\n");
  fflush (stdout);
#endif
  /* Wait for 64 ACK */
  while (!(BUSY));
  /* read value */
  value = PARR;
  /* Toggle POUT low, Parallel port DDR = in */
  POUTRLOW;
#ifdef DEBUG2
  printf ("getchar - Waiting for 64 ACK (BUSY)\n");
  fflush (stdout);
#endif
  /* Wait for 64 ACK */
  while (BUSY);
  /* Second toggle */
  POUTRHIGH;
  POUTRLOW;
#ifdef DEBUG2
  printf ("Got %02x\n", value);
  fflush (stdout);
#endif

  return (value);
}


/* send a single character back over the link */

int
sendchar (int byte)
{
#ifdef DEBUG2
  printf ("Trying to send %02x...", byte);
  fflush (stdout);
#endif
#ifdef DEBUG2
  printf ("sendchar - Waiting for 64 ACK (!BUSY)\n");
  fflush (stdout);
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
  printf ("sendchar - Waiting for 64 ACK (BUSY)\n");
  fflush (stdout);
#endif
  while (BUSY);
  PARIN;
  POUTRHIGH;
  POUTRLOW;
  POUTRHIGH;
/*  while(inb(outport)!=0xff) ; */
#ifdef DEBUG2
  printf ("Sent\n");
#endif
  return (0);
}

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
    /* wait for 64 NACK */
    while (BUSY);
    PARW (block[n]);
    POUTWLOW;
    POUTWHIGH;
    n++;
  }
  /* close off */
  while (!(BUSY));
  /* send ack */
  /* Parallel port DDR = in, POUT = low */
  PARIN;
  POUTRLOW;

  return (0);

}

int
fishgetblock (int size, uchar * block)
{
  int n = 0;

  PARIN;
  POUTRHIGH;	/* set port DDR in, don't toggle strobe, this is for non-AMIGA */

  while (n < size)
  {
    /* wait for 64 NACK */
    while (!(BUSY));
    /* get char & toggle POUT */
    block[n] = PARR;
    n++;
    POUTRLOW;
    POUTRHIGH;			/* raise here before busy loop to reduce port accesses */
    /* wait for ACK */
    while (BUSY);
    /* get char & toggle POUT */
    block[n] = PARR;
    n++;
    POUTRLOW;
    POUTRHIGH;
  }

  /* close off */
  while (!(BUSY));
  /* send ack */
  /* Parallel port DDR = in, POUT = low */
  PARIN;
  POUTRLOW;
  return (0);

}

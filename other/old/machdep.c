#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef BSD
#include "bsd/cpufunc.h"
#endif /* BSD */

#ifdef LINUX
#include <asm/io.h>
#include <sys/io.h>
#endif

#ifdef AMIGA
#include "amiga/my_exec_types.h"
#include <exec/semaphores.h>
#include <clib/exec_protos.h>
#include <clib/misc_protos.h>
#include <hardware/cia.h>
#include <resources/misc.h>
#endif

#ifdef AMIGA
#include "my_exec_types.h"
#include <exec/semaphores.h>
#include <clib/exec_protos.h>
#include <clib/misc_protos.h>
#include <hardware/cia.h>
#include <resources/misc.h>
extern volatile struct CIA ciaa;
extern volatile struct CIA ciab;
#endif

#define uchar unsigned char

#include "fs.h"
#include "machdep.h"

/*
extern int steal_parallel_port;
struct Library *MiscBase;
*/

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
/* Printer port transfer routines ----------------------------------------- */

/* receive synchronised character (begining of hand shake) */

void
init_hw (void)
{
#ifdef BSD
 if ((f = fopen ("/dev/io", "rw")) == NULL)
  {
    fatal_error ("Cannot get chip-bash privelige.\
                 \nPlease setgid to kmem (or similar)\n");
    exit (1);
  }
#endif /* BSD */

#ifdef LINUX
 /* Enable port access to the range 0x0 to 0x3ff */
 printf("LINUX: Asking for chip-bash priveliges\n");
 printf("ioperm returned = %d\n",ioperm(0x200,0x1ff,3));
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
    printf ("Allocatin parallel port\n");
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
  long c;

  c = 0;

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
    if (c > synctolerance)
    {
      value = -1;
      return (-1);
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
    if (c > synctolerance)
    {
      value = -1;
      return (-1);
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
  printf ("sebdchar - Waiting for 64 ACK (BUSY)\n");
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
fastsendblock (int addr, int size, uchar * the_block)
{
  /* send a block of data *super* fast */
  /* Algorithm:
     - Send block backwards (easier for 64 to receive!) 
     using much less handshaking due to mono-directionality.
     1) Send command
     2) send address
     3) send 254 bytes of data using minimal hand shaking
   */

#ifdef DEBUG2
  printf ("Inside fastsendblock\n");
  fflush (stdout);
#endif
  sendchar (0xf5);
  sendchar (addr & 0xff);
  sendchar (addr / 256);

#ifdef DEBUG2
  printf ("Outside fastsendblock\n");
  fflush (stdout);
#endif
  return (fishsendblock (254, the_block));

}

int
fastgetblock (int addr, int size, uchar * the_block)
{
  /* send a block of data *super* fast */
  /* Algorithm:
     - Send block backwards (easier for 64 to receive!) 
     using much less handshaking due to mono-directionality.
     1) Send command
     2) send address
     3) send 254 bytes of data using minimal hand shaking
   */

#ifdef DEBUG2
  printf ("Inside fastgetblock\n");
  fflush (stdout);
#endif
  sendchar (0xf4);
  sendchar (addr & 0xff);
  sendchar (addr / 256);

#ifdef DEBUG2
  printf ("Outside fastgetblock\n");
  fflush (stdout);
#endif
  return (fishgetblock (254, the_block));

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

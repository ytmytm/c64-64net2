
#define inport 0x379
#define outport 0x378
#define DEBUG2

#include <stdio.h>
#include <asm/io.h>
#include <sys/io.h>
#define PAROUT
#define PARIN
#define POUTHIGH
#define POUTLOW
#define PARR inb(outport)
#define BUSY (inb(inport) & 0x80)
#define POUTRHIGH outb(0xa0, outport+2)
#define POUTWHIGH outb(0x40, outport+2)
#define POUTRLOW  outb(0xa1, outport+2)
#define POUTWLOW  outb(0x41, outport+2)
#define PARW(x) outb(x, outport)

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

void start(void) {
 printf("LINUX: Asking for chip-bash priveliges\n");
 printf("ioperm returned = %d\n",ioperm(0x200,0x1ff,3));
 fflush(stdout);
 sleep(0);
}

int main (void) {

    printf ("Hello!\n");
    start();
    return sendchar('A');
    }
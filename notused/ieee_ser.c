/*
 *  - common/prot.c
 *
 *  - generic serial protocol handling functions
 *
 *  This file is part of the VC 1541 Emulator.
 *  Copyright (c) 1997 Torsten Paul
 *
 *  Snaffu'd and modified for 64net/2
 *  Changes Copyright (c) 1997 Paul Gardner-Stephen
 */

#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>
#ifdef LINUX
#include <asm/io.h>
#endif
#include "machdep.h"
#include "iec.h"

#define byte_t unsigned char
#define word_t int 

/* XXX debug stuff */
int bits[8]={0};
int bc=0;


void   Set(byte_t bit, byte_t val);
byte_t Get(byte_t bit);
byte_t Wait(byte_t bit, byte_t val);
byte_t Wait_eoi(byte_t bit, byte_t val);
void   handle_ATN(void);

extern int outport,inport; 

/* XXX and so is this, but its nice and fast  */
#ifdef LINUX
/* #define Set(b,v) outb((inb(outport+2)&((0xff-(b))|(v^((CLOCK|DATA)&b)))),outport+2) */
void   Set(byte_t bit, byte_t val)
{
  outb(
       (inb(outport+2)&(0xff-bit)) | 
       ((val^(CLOCK|DATA))&bit),
       outport+2);
}

#else
#error Fix the next macro
#define Set(b,v) outb(outport+2,(inb(outport+2)&((!(b)))|(v)))
#endif

/* 
   #include "vc1541.h"
   #include "common.h"
*/


int timeout; 			                   /* this is set by Wait */

void handle_CLOSE(void)
{
}

int GetIECByte(byte_t *ret)
{
    int eoi;
    byte_t b=0;
    int foo=Get(CLOCK|DATA);

    eoi = 0;
    
    if (Wait(CLOCK,0))
      {
	printf("Clock didnt drop\n");
      }

    Set(DATA,0); 
    
    if (Wait(CLOCK,CLOCK)) 
      {
	printf("timeout on clock assertP %02x/%02x\n",foo,Get(DATA|CLOCK)); 
	Set(DATA,DATA);
	return 0;
      }
    /* printf("clock high\n"); */

    /* Hold off time is here */

    Set(DATA,DATA);
    /* Set(CLOCK,CLOCK); */

    /*
     *  check for eoi
     */
    if (Wait_eoi(CLOCK, 0)) {                     	       /* [$e9ee] */
	eoi = 1;
	Set(DATA, 0);                              	       /* [$e9f2] */
	MicroSleep(80);  /* min = 60usec */            	       /* [$e9f8] */
	Set(DATA, DATA);                                 	       /* [$e9fa] */
	if (Wait(CLOCK, 0))
	  {
	    printf("eoi clock drop timeout\n");
	    return 0;         	       /* [$ea05] */
	  }
    }

    /* printf("reading bits\n"); */

    bc=0;

    if (Wait(CLOCK, CLOCK)) return 0;                   	       /* [$ea13] */
    if (Get(DATA)) b|=1;
    bits[bc]=Get(DATA|CLOCK|ATN);
    if (Wait(CLOCK, 0)) { timeout=1; return 0; }               	       /* [$ea22] */

    bc=1;

    if (Wait(CLOCK, CLOCK)) return 0;                   	       /* [$ea13] */
    if (Get(DATA)) b|=2;
    bits[bc]=Get(DATA|CLOCK|ATN);
    if (Wait(CLOCK, 0)) { timeout=1; return 0; }               	       /* [$ea22] */

    bc=2;

    if (Wait(CLOCK, CLOCK)) return 0;                   	       /* [$ea13] */
    if (Get(DATA)) b|=4;
    bits[bc]=Get(DATA|CLOCK|ATN);
    if (Wait(CLOCK, 0)) { timeout=1; return 0; }               	       /* [$ea22] */

    bc=3;

    if (Wait(CLOCK, CLOCK)) return 0;                   	       /* [$ea13] */
    if (Get(DATA)) b|=8;
    bits[bc]=Get(DATA|CLOCK|ATN);
    if (Wait(CLOCK, 0)) { timeout=1; return 0; }               	       /* [$ea22] */

    bc=4;

    if (Wait(CLOCK, CLOCK)) return 0;                   	       /* [$ea13] */
    if (Get(DATA)) b|=16;
    bits[bc]=Get(DATA|CLOCK|ATN);
    if (Wait(CLOCK, 0)) { timeout=1; return 0; }               	       /* [$ea22] */

    bc=5;

    if (Wait(CLOCK, CLOCK)) return 0;                   	       /* [$ea13] */
    if (Get(DATA)) b|=32;
    bits[bc]=Get(DATA|CLOCK|ATN);
    if (Wait(CLOCK, 0)) { timeout=1; return 0; }               	       /* [$ea22] */

    bc=6;

    if (Wait(CLOCK, CLOCK)) return 0;                   	       /* [$ea13] */
    if (Get(DATA)) b|=64;
    bits[bc]=Get(DATA|CLOCK|ATN);
    if (Wait(CLOCK, 0)) { timeout=1; return 0; }               	       /* [$ea22] */

    bc=7;

    if (Wait(CLOCK, CLOCK)) return 0;                   	       /* [$ea13] */
    bits[bc]=Get(DATA|CLOCK|ATN);
    if (Get(DATA)) b|=128;

    bc=8;


    /* printf("b = $%02x\n",b); */
    if (Wait(CLOCK, 0)) { timeout=1; return 0; }               	       /* [$ea22] */

    Set(DATA, 0);                              	       /* [$ea28] */

    *ret = b;  


    if (eoi) {
      /* printf("EOI\n"); */
      Set(CLOCK, 0);                                /* c64: [$ee0d] */
      Set(DATA, 0);                                  /* c64: [$ee10] */
    }
#if 0
#endif
    /* PGS: Is this delay here *really* needed? */
    MicroSleep(100);                               	  /* c64: [$ee0a] */

    return eoi;
}

int PutIECByte(byte_t b, int eoi)
{
    int a;
    byte_t tmp;
    int flag = 0;


    /* DELAY c64: [$ee1b] */

    MicroSleep(200);

    tmp = Get(DATA);                                           /* [$e919] */
    Set(CLOCK, 0);                    			       /* [$e91f] */

    if (tmp) {                     			       /* [$e923] */
	if (Wait(DATA, 0)) return 0;			       /* [$e92d] */
	flag = eoi;
    } else {
	flag = 1;              		    /* [$e923] force eoi, why ??? */
    }
    if (flag) {
	if (Wait(DATA, 0)) return 0;			       /* [$e93f] */
	if (Wait(DATA, DATA)) return 0;			       /* [$e949] */
    }

    Set(CLOCK, CLOCK);        				       /* [$e94b] */
    if (Wait(DATA, 0)) return 0; 			       /* [$e956] */

    for (a = 0;a < 8;a++) {

	/* DELAY c64: [$ee5a] */
	MicroSleep(80);

	if (Get(DATA)) {
	    return 0;					       /* [$e961] */
	}
	if (b & (1 << a)) {
	    Set(DATA, 0);     				       /* [$e973] */
	} else {
	    Set(DATA, DATA);   				       /* [$e96e] */
	}
	Set(CLOCK, 0);         				       /* [$e976] */
	MicroSleep(80);       	       /* [$e97d -> $fef3] delay for 1541 */

	/* DELAY c64: [$ee67] */

	Set(CLOCK, CLOCK);     			      /* [$e980 -> $fefb] */
	Set(DATA, 0);   		              /* [$e980 -> $fefb] */
    }
    if (Wait(DATA, DATA)) return 0;         		       /* [$e98f] */

    return flag;
}

/*
 *  [$e909]
 */
void handle_TALK(byte_t dev, byte_t sec_addr, char *iobuf)
{
	switch (sec_addr) {
	case 15:
	  /* ErrorInfo("handle_TALK command channel..."); */
	  PutIECByte('0', 0); if (timeout) return;
	  PutIECByte('0', 0); if (timeout) return;
	  PutIECByte(',', 0); if (timeout) return;
	  PutIECByte(' ', 0); if (timeout) return;
	  PutIECByte('O', 0); if (timeout) return;
	  PutIECByte('K', 0); if (timeout) return;
	  PutIECByte(',', 0); if (timeout) return;
	  PutIECByte('0', 0); if (timeout) return;
	  PutIECByte('0', 0); if (timeout) return;
	  PutIECByte(',', 0); if (timeout) return;
	  PutIECByte('0', 0); if (timeout) return;
	  PutIECByte('0', 0); if (timeout) return;
	  PutIECByte(0x0d, 1); if (timeout) return;
	  break;
	default:
	  /* ErrorInfo("handle_TALK..."); */
	  break;
	}
}

/*
 *  [$ea2e]
 *
 *  If iobuf != NULL we have an open command and the filename
 *  is following. The other case is a data transfer.
 *  FIXME: buffer overflow not tested!
 *
 */
void handle_LISTEN(byte_t dev, byte_t sec_addr, char *iobuf)
{

	switch (sec_addr) 
	  {
	  case 15:
	    /* ErrorInfo("handle_LISTEN command channel..."); */
	    break;
	  default:
	    /* ErrorInfo("handle_LISTEN..."); */
	    break;
	  }
}

/*
 *  [$e85b]
 */
void handle_ATN(void)
{
  int i;
  byte_t b, c;
  char iobuf[256];
  byte_t mode, dev, sec_addr;
  
  if (Wait(CLOCK, 0))             			       /* [$e882] */
    {
      printf("ATN dropped, but CLK didnt\n");
      return ;
    }
  Set(DATA,0);
  timeout = 0;
  
  /* MicroSleep(200); */
  GetIECByte(&b);	         		     /* get device number */
  printf("Read %02x  (%d)\n",b,bc);
  for(i=0;i<bc;i++)
    printf("%02x ",bits[i]);
  printf("\n");
  if (timeout) return;
  printf("no timeout\n");
  dev = b & IEC_DEVICE;
  mode = b & IEC_COMMAND;
  switch (mode) 
    {
    case IEC_UNLISTEN:
      Wait(ATN, 0);
      printf("Unlisten to device %d\n",dev);
      return;
    case IEC_UNTALK:
      Wait(ATN, 0);
      printf("Untalk to device %d\n",dev);
      return;
    case IEC_LISTEN:
      if (1) /* XXX if this device is _NOT_ active */ 
	{
	  Set(DATA | CLOCK, 0);  			       /* [$e8ff] */
	  while (Wait(ATN, 0));	 		       /* [$e907] */
	  printf("Listen to device %d\n",dev);
	  return;
	}
      break;
    case IEC_TALK:
      if (1) /* XXX if this device is _NOT_ active */
	{
	  Set(DATA | CLOCK, 0);    		       /* [$e8ff] */
	  while (Wait(ATN, 0));	 		       /* [$e907] */
	  printf("Talk to device %d\n",dev);
	  return;
	}
      break;
    }
  
  MicroSleep(200);
  GetIECByte(&c);              		 /* get secondary address */
  
  if (timeout) return;
  
  switch (c & IEC_MODE) {
  case IEC_OPEN:
    Wait(ATN, 0);	         			       /* [$e907] */
    sec_addr = c & IEC_ADDRESS;
    handle_LISTEN(dev, sec_addr, iobuf);  	      /* [$e8e7 -> $ea2e] */
    break;
  case IEC_CLOSE:
#if 0
    Wait(ATN, 0);    				       /* [$e907] */
#endif
    handle_CLOSE();         	              /* [$e8ce -> $dac0] */
    break;
  case IEC_DATA:
    sec_addr = c & IEC_ADDRESS;
    switch (mode) {
    case IEC_LISTEN:
      Wait(ATN, 0);	   		   	       /* [$e907] */
      handle_LISTEN(dev, sec_addr, NULL);	      /* [$e8e7 -> $ea2e] */
      break;
    case IEC_TALK:
      Set(DATA | CLOCK, 0); 			       /* [$e8ff] */
      Wait(ATN, 0);	   			       /* [$e907] */
      MicroSleep(200); /* FIXME */
      Set(DATA, 0);                		       /* [$e8f1] */
      Set(CLOCK, CLOCK);           		       /* [$e8f4] */
      MicroSleep(200); /* FIXME */
      handle_TALK(dev, sec_addr, iobuf);	      /* [$e8f7 -> $e909] */
      break;
    }
  }
  
  if (!timeout) return;
}

byte_t Get(byte_t u_bit)
{
  byte_t a,b;
  int bit;

  if (u_bit&DATA) bit|=DTA_in_VALUE;
  if (u_bit&CLOCK) bit|=CLK_in_VALUE;
  if (u_bit&ATN) bit|=ATN_in_VALUE;

  /* Read value from port, with settling */
  a=inb(outport+1); b=inb(outport+1);
  while(a!=b)
    {
      a=inb(outport+1); b=inb(outport+1);
    }

  b=0;
  if (a&DTA_in_VALUE) b|=DATA;
  if (a&CLK_in_VALUE) b|=CLOCK;
  if (a&ATN_in_VALUE) b|=ATN;
  
  return(b&u_bit);
}

byte_t Wait_eoi(byte_t u_bit, byte_t u_val)
{
  /* word_t count = IO_TIMEOUT1;  40ms (20*200usec) ?? */ 
  int b=0,bv=1;
  int c=200; /* x20 usec = 40ms */

  int bit=0,val=0;

  if (u_bit&DATA) bit|=DTA_in_VALUE;
  if (u_bit&CLOCK) bit|=CLK_in_VALUE;
  if (u_bit&ATN) bit|=ATN_in_VALUE;

  if (u_val&DATA) val|=DTA_in_VALUE;
  if (u_val&CLOCK) val|=CLK_in_VALUE;
  if (u_val&ATN) val|=ATN_in_VALUE;


  while(bv!=b)
    {
      bv=inb(outport+1)&bit;
      b=inb(outport+1)&bit;
    }

  while(bv!=val)
    {
      if (c--)
	MicroSleep(20);
      else
	{ timeout=1; return 1; }
      bv=inb(outport+1)&bit;
      b=inb(outport+1)&bit;
      while(bv!=b)
	{
	  bv=inb(outport+1)&bit;
	  b=inb(outport+1)&bit;
	}
    }

  return 0;
}

byte_t Wait(byte_t u_bit, byte_t u_val)
{
  /* Wait for a bit to have the correct value.
     Timeout is 1sec */
  /* word_t count = IO_TIMEOUT2; ~1sec delay */

  int bit=0,val=0;
  int bv=1;
  int b=0;
  int c=50000; /* x 20usec = 1sec */

  if (u_bit&DATA) bit|=DTA_in_VALUE;
  if (u_bit&CLOCK) bit|=CLK_in_VALUE;
  if (u_bit&ATN) bit|=ATN_in_VALUE;

  if (u_val&DATA) val|=DTA_in_VALUE;
  if (u_val&CLOCK) val|=CLK_in_VALUE;
  if (u_val&ATN) val|=ATN_in_VALUE;

  /* printf("Waiting for mask $%02x to be $%02x\n",bit,val); */
  /* printf("(ubit = $%02x, u_val = $%02x)\n",u_bit,u_val); */


  while(bv!=b)
    {
      bv=inb(outport+1)&bit;
      b=inb(outport+1)&bit;
    }
  
  /* gettimeofday(&tv0,NULL);
     gettimeofday(&tv,NULL); */

  while(bv!=val)
    {
      if (c--)
	MicroSleep(20);
      else
	{
	  timeout=1;
	  printf("wait timed out, end value $%02x\n",bv);
	  return 1;
	}
      bv=inb(outport+1)&bit;
      b=inb(outport+1)&bit;
      while(bv!=b)
	{
	  bv=inb(outport+1)&bit;
	  b=inb(outport+1)&bit;
	}
    }

  return 0;
}


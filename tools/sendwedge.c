#include "config.h"
#include <machine/cpufunc.h>
#include <machine/sysarch.h>
#include <fcntl.h>

int main(int argc,char **argv)
{
  int port;
  int p[3],o[3];
  int i=0,c;
  FILE *f;

  if (argc!=3)
    {
      fprintf(stderr,"64net/2 wedge send utility.\n");
      fprintf(stderr,"synopsis: Send the wedge to a C64 using simplified protocol.\n");
      fprintf(stderr,"sendwedge <port> <wedgefile>\n");
      exit(2);
    }
  
  open("/dev/io",O_RDWR);
  if ((f=fopen(argv[2],"r"))==NULL)
    {
      fprintf(stderr,"Failed to read file '%s'\n",argv[2]);
      exit(3);
    }

  port=strtol(argv[1],NULL,16);
  printf("Port is 0x%x\n",port);

  /* Enable EPP non-FIFO mode */
  outb(port+0x402,0x80);

  /* Clear EPP timeout bit */
  outb(port+1,0xfe); /* Status port */

  /* Set port in forward mode (bit 5) and
     idle port */
  outb(port+2,0x04);

  outb(port,0);

  o[1]=inb(port+1);

  c=fgetc(f);
  while(c>-1)
    {
      /* Wait for BUSY to drop */
      while(inb(port+1)&0x80) usleep(0);

      outb(port,c);

      outb(port+1,0xfe);
      printf("Sent %d bytes a\r",++i); fflush(stdout);
      usleep(1000);

      /* strobe the strobe line */
      outb(port+2,1);
      outb(port+2,0);

      /* Wait for BUSY to rise */
      while((inb(port+1)&0x80)!=0x80) usleep(0);

      /* strobe the strobe line */
      outb(port+2,1);
      outb(port+2,0);

      c=fgetc(f);

    }
  printf("*** Sent all %d chars.\n",i);
  printf("\n");
}

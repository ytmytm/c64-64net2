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
  char line[80];

  if (argc!=2)
    {
      fprintf(stderr,"64net/2 port testing utility.\n");
      fprintf(stderr,"port_fiddle <port>\n");
      exit(2);
    }
  
  open("/dev/io",O_RDWR);

  port=strtol(argv[1],NULL,16);
  printf("Port is 0x%x\n",port);

  /* Enable EPP non-FIFO mode */
  outb(port+0x402,0x80);

  /* Clear EPP timeout bit */
  outb(port+1,0xfe); /* Status port */

  /* Set port in forward mode (bit 5) and
     idle port */
  outb(port+2,0x04);

  while(1)
    {
      p[0]=inb(port); p[1]=inb(port+1); p[2]=inb(port+2);
      printf("$%03x : $%02x $%02x $%02x",port,p[0],p[1],p[2]);
      printf(" : > ");
      line[0]=0; fgets(line,80,stdin);
      c=strtol(&line[2],NULL,10);
      if (line[2]=='$')
	c=strtol(&line[3],NULL,16);
      if (line[2]=='%')
	c=strtol(&line[3],NULL,2);
      if (!strncmp("d=",line,2))
	{
	  outb(port,c);
	  printf("Set data port to %02x (%d)\n",c,c);
	}
      else
	if (!strncmp("s=",line,2))
	  {
	    outb(port+1,c);
	    printf("Set status port to %02x (%d)\n",c,c);
	  }
	else
	  if (!strncmp("c=",line,2))
	    {
	      outb(port+2,c);
	      printf("Set control port to %02x (%d)\n",c,c);
	    }
	  else
	    {
	      if (strcmp(line,"\n"))
		{
		  printf("Unknown command.\n");
		  printf("{d,s,c}=[$%]value sets data, status and control ports\n");
		  printf("A blank line shows the current port values.\n");
		}
	    }

    }

}


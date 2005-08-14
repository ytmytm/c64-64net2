/* chip-bash includes */

#include <stdlib.h>
#include <dev/ppbus/ppi.h> 
#include <dev/ppbus/ppbconf.h>
#include <machine/sysarch.h>
#include <machine/cpufunc.h>

/* int portout;
   int portin; */

int lpt_fd;

#define ATN_IN 0x40
#define REQUEST_IN 0x80
#define VALID_OUT 0x01
#define ERROR_CLOCK 0x02
#define ERROR_DATA 0x04

void set_datalines_input();

int bind_to_port (const char *name) 
{
  int a;
  
  lpt_fd = open (name, O_RDWR);
  if (lpt_fd == -1) {
    fprintf(stderr,"Error openning communications device '%s'\n",
	   name);
    perror ("open");
    exit(1);
  }

  /* Set "byte mode" - i.e. unbuffered PS/2 bidirectional mode */
  a=0x20;
  ioctl(lpt_fd,PPISECR,&a);

  ioctl(lpt_fd,PPIGECR,&a);
  printf("ECP mode is $%02x\n",a);

  /* Enable EPP non-FIFO mode */
  /*  outb(portout+0x402,0x80); */

  /* Clear EPP timeout bit */
  /* outb(portout+1,0xfe); */

  set_datalines_input();

  return lpt_fd;
}

void set_lpt_control(int status)
{
  /* outb(portout+2,status); */
  ioctl(lpt_fd,PPISCTRL,&status);
}

void write_data(unsigned char data) 
{
  /* outb(portout,data); */
  ioctl(lpt_fd,PPISDATA,&data);
#ifdef DEBUG_PIEC
  printf("Presenting data: $%X\n",(unsigned char)data);
#endif
  return;		
}

int read_data() {
  int data; /* =inb(portout);  */
  ioctl(lpt_fd,PPIGDATA,&data);
#ifdef DEBUG_PIEC
  printf("Receiving data: $%X\n",(unsigned char)data);
#endif
  return data;
}

int get_status() {
  int status;

  ioctl(lpt_fd,PPIGSTATUS,&status);
  return status; /* inb(portin); */
}

int get_ATN() {
  if((get_status()&ATN_IN) == ATN_IN) { return 0; }
  else { return 1; }
}

void set_datalines_output() {
  //set datalines to outputmode
  int control;

  /* Clear EPP/ECP timeout error */
  control=0xfe;
  ioctl(lpt_fd,PPISECR,&control);

  /* Select EPP non-fifo mode */
  control=0x80;
  ioctl(lpt_fd,PPISECR,&control);

  /* Set port direction to output */
  set_lpt_control(0);

  /* int control;
     control=inb(portout+2);
     control&=0x0f; 
     outb(portout+2,control); */
  
  return;		
}

void set_datalines_input() {
  //set datalines to outputmode
  int control;

  /* Clear EPP/ECP timeout error */
  control=0xfe;
  ioctl(lpt_fd,PPISECR,&control);

  /* Select EPP non-fifo mode */
  control=0x80;
  ioctl(lpt_fd,PPISECR,&control);

  /* Set port direction to input */
  set_lpt_control(0x00);
  set_lpt_control(0xa4);

  /* outb(portout,0xff);
     control=inb(portout+2);
     control|=0x40;
     outb(portout+2,control); */
  return;		
}


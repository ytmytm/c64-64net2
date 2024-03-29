/* Hardware Access Routines for Linux */
#include <sys/io.h>
#include <linux/parport.h>
#include <linux/ppdev.h>
#include <linux/ioctl.h>

/* XXX we chould move those to another include file probably? TB */
#define VALID_OUT	PARPORT_CONTROL_STROBE
#define ERROR_DATA	PARPORT_CONTROL_INIT
#define REQUEST_IN	PARPORT_STATUS_BUSY
#define ATN_IN		PARPORT_STATUS_ACK 
#define ERROR_CLOCK	PARPORT_CONTROL_AUTOFD

static int DDR_READ =	0x1;
static int DDR_WRITE =	0x0;

int lpt_fd;

void set_lpt_control(int status)
{
  ioctl(lpt_fd,PPWCONTROL,&status);
}

void write_data(uchar data) {
#ifdef DEBUG_PIEC
  printf("Presenting data: $%X\n",(unsigned char)data);
#endif
  ioctl(lpt_fd,PPWDATA,&data);
  return;		
}

int read_data() {
  int data;
  ioctl(lpt_fd,PPRDATA,&data);
#ifdef DEBUG_PIEC
  printf("Receiving data: $%X\n",(unsigned char)data);
#endif
  return data;
}

int get_status() {
  int status;
  ioctl(lpt_fd,PPRSTATUS,&status);
  return status;
}

void set_datalines_output() {
  ioctl(lpt_fd,PPDATADIR,&DDR_WRITE);                 //set datalines to outputmode
  return;		
}

void set_datalines_input() {
  ioctl(lpt_fd,PPDATADIR,&DDR_READ);                 //set datalines to outputmode
  return;		
}

int bind_to_port (const char *name) {
  int lpt_fd;
  lpt_fd = open (name, O_RDWR);
  if (lpt_fd == -1) {
    perror ("open");
    exit(1);
  }
  /* so we can be suer no other modules use the port and bring us into trouble
   * like cups or something trying to print and such. */
  if (ioctl (lpt_fd, PPEXCL)) {
    perror ("PPEXCL");
    printf("Couldn't get exclusiv access to \"%s\". It is possible that other programms or modules interfere with the actions of 64NET/2 while communicating on lpt and thus cause problems during transfer. If you encounter problems you know where to start first.\n", name);
   // close (lpt_fd);
   // exit(1);
  }
  if (ioctl (lpt_fd, PPCLAIM)) {
    perror ("PPCLAIM");
    close (lpt_fd);
    exit(1);
  }
  return lpt_fd;
}


/* Hardware Access Routines for Linux */
#include <sys/io.h>
#include <linux/parport.h>
#include <linux/ppdev.h>
#include <linux/ioctl.h>

#define VALID_OUT	PARPORT_CONTROL_STROBE
#define ERROR_OUT	PARPORT_CONTROL_SELECT
#define REQUEST_IN	PARPORT_STATUS_BUSY
#define ATN_IN		PARPORT_STATUS_ACK 
#define ERROR		PARPORT_CONTROL_AUTOFD

static int DDR_READ =	0x1;
static int DDR_WRITE =	0x0;

void set_lpt_control(int status)
{
  ioctl(lpd_fd,PPWCONTROL,&status);
}

void acknowledge() {
  set_lpt_control(VALID_OUT);
  set_lpt_control(0);
}

void write_data(uchar data) {
  ioctl(fd,PPWDATA,&data);
#ifdef DEBUG_PIEC
  printf("Presenting data: $%X\n",(unsigned char)data);
#endif
  return;		
}

int read_data() {
  int data;
  ioctl(fd,PPRDATA,&data);
#ifdef DEBUG_PIEC
  printf("Receiving data: $%X\n",(unsigned char)data);
#endif
  return data;
}

int get_status() {
  int status;
  ioctl(fd,PPRSTATUS,&status);
  return status;
}

int get_ATN() {
  int status;
  ioctl(fd,PPRSTATUS,&status);
  if((status&ATN_IN) == ATN_IN) { return 0; }
  else { return 1; }
}

void set_datalines_output() {
  ioctl(fd,PPDATADIR,&DDR_WRITE);                 //set datalines to outputmode
  return;		
}

void set_datalines_input() {
  ioctl(fd,PPDATADIR,&DDR_READ);                 //set datalines to outputmode
  return;		
}

int bind_to_port (const char *name) {
  int fd;
  fd = open (name, O_RDWR);
  if (fd == -1) {
    perror ("open");
    return 1;
  }
  if (ioctl (fd, PPCLAIM)) {
    perror ("PPCLAIM");
    close (fd);
    return 1;
  }
  return fd;
}


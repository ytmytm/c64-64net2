/* 
   64NET/2 Commune module
   (C)Copyright Paul Gardner-Stephen 1995, 1996, 2003 All Rights Reserved.

   This module does all the C64 <--> Server communications
   Printer port version
 */

/*
   BUGS:
   This programme will have to be greatly generalised as time progresses.
   At present it will hopefully support the full drive redirection scope
   intended.  Communication device generalisation will be next on the 
   list.
 */

#include "config.h"

#include "fs.h"
#include "comm-lpt.h"
#include "client-comm.h"
#include "fs_accel.h"
#include "misc_func.h"
#include "comm-work.h"
#include "datestamp.h"

int parallel_iec_commune(int first_char);

extern int piec_delaycount;

#define PADDING 1

/* ports */
#ifdef USE_SERIAL_DRIVER
uchar port[1024] = COMM_DEVICE;
#else
uchar port[1024] = LPT_DEVICE;
#endif

/* Current talk & listen logical files */
int talklf=-1;
int listenlf=-1;
int lastlf=-1;

/* ** DRIVE and PARTITION resolution: */
/* Partition base directories for each drive */
uchar *partn_dirs[MAX_NET_DEVS][256];

/* ** CBM DOS emulation stuff: */
/* DOS Statii */
uchar dos_status[MAX_NET_DEVS][256];
/* length of DOS statii */
uchar dos_stat_len[MAX_NET_DEVS];
/* DOS commands */
uchar dos_command[MAX_NET_DEVS][256];
/* length of DOS command */
uchar dos_comm_len[MAX_NET_DEVS]={0};

/* ** current directories & partitions: */
/* current partition on each drive */
int curr_par[MAX_NET_DEVS];
/* current subdirectory (absolute) for each partition on each
   drive */
uchar *curr_dir[MAX_NET_DEVS][256];

/* dir blocks for partitions */
int partn_dirtracks[MAX_NET_DEVS][256];
int partn_dirsectors[MAX_NET_DEVS][256];
int curr_dirtracks[MAX_NET_DEVS][256];
int curr_dirsectors[MAX_NET_DEVS][256];

/* Logical files */
fs64_file logical_files[MAX_NET_DEVS][16];

/* Device number as given in config file and passed to client upon boot */
int devnum=1;

/* client system type */
int client_type=0;

/* Last accessed drive, for the purposes of error channels etc */
int last_drive=-1; /* none */
int last_unit=-1; /* none */

/* Drive currently being accessed */
int file_unit=-1;

/* structures for redirected devices */
int devices[MAX_NET_DEVS]={0};

/* file that points to our communications device */
fs64_file file;

/* partition # that will be searched for programs, whatever this means, not used */
int pathdir;

/* #define DEBUG_PIEC */

#include <fcntl.h>
#include <stdio.h>
#include <sys/poll.h>

#ifdef LINUX
#include "lptio/lptio_linux.c"
#endif

#ifdef __FreeBSD__
#include "lptio/lptio_freebsd.c"
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

//different states
#define IEC_IDLE		0x0
#define IEC_TALK		0x40
#define IEC_LISTEN		0x20
#define IEC_UNTALK		0x5f
#define IEC_UNLISTEN	0x3f
#define IEC_IDENTIFY	0xff

//special SAs for LOAD/SAVE
#define IEC_LOAD		0x60
#define IEC_SAVE		0x61
#define uchar		unsigned char

unsigned char request;
int devicenumber=9;
uchar SA;
int state=IEC_IDLE;
int ATN=0;
int inputlen=0;
int inputpos=0;
int outputlen=0;
int outputpos=0;
unsigned char* input_buffer;
unsigned char* output_buffer;
int lpt_fd;

unsigned int iec_listen();
unsigned int talk();
unsigned char change_state(unsigned int);
int send_byte(unsigned char data);
int receive_byte();
int send_eoi();
int start_server();

//todo: maybe we add also the resetpin? If the c64 resets also the 64net/2 can
// reset and get into IDLE mode again. 

int parallel_iec_commune(int unused) 
{

  lpt_fd=bind_to_port(port);
  if (lpt_fd<0)
    {
      fprintf(stderr,"Failed to open parallel port.\n");
      perror("open_lptport");
    }

  set_datalines_input();
  set_lpt_control(0); // drop strobe

  /* Allow answering of queued request */
  request=(get_status()&REQUEST_IN)^REQUEST_IN;

  while(start_server()==-1);

  return 0;
}

int start_server() 
{
  int i;
#ifdef DEBUG_PIEC
  int a;
#endif

  unsigned int temp;
  for(;;) {
    switch(state) {
    case IEC_IDLE:
#ifdef DEBUG_PIEC
      printf("changed state: IDLE\n");
      printf("request=%d\n",request);
#endif
      temp=receive_byte(); 
      if (temp==0xff)
	{
	  /* 0xff received unsolicited is possibly a case of both sides
	     thinking they are reading.  Send an EOI to prevent infinite
	     reading by C64 */
	  send_eoi();
	}
      else
	{
	  if((temp&0x100)==0) { change_state(IEC_IDLE); break; }
	  change_state(temp);
	}
      break;
      
    case IEC_LISTEN:
#ifdef DEBUG_PIEC
      printf("changed state: LISTEN\n");
#endif
      temp=receive_byte(); 
      if((temp&0x100)==0) { change_state(IEC_IDLE); break; }
      SA=temp;
#ifdef DEBUG_PIEC
      printf("SA=%d\n",(uchar)SA);
#endif
      iec_listen();
#ifdef DEBUG_PIEC
      //print received data for debug
      for(a=0;a<inputlen;a++) 
	{ printf("$%X ",(uchar)input_buffer[a]); } 
      printf("\n");
#endif
      
      if((SA&0xf)==1) {
	//received data will be saved to disk
#ifdef DEBUG_PIEC
	printf("I think I am saving\n");
#endif
	// Write data which has been received
	for(i=0;i<inputlen;i++)
	  fs64_writechar (&logical_files[file_unit][SA&0xf], input_buffer[i]);
	printf("Wrote %d bytes to file.\n",i);

	if(inputlen>0) free(input_buffer);
	inputlen=0;
      }
      if ((SA&0xf)==0xf)
	{
	  /* DOS command - so write it out */
	  last_unit=file_unit;
	  input_buffer[inputlen]=0;
	  strncpy(dos_command[last_unit],input_buffer,255);
	  dos_command[last_unit][255]=0;
	  dos_comm_len[last_unit]=(inputlen<256) ? inputlen : 255;
	  do_dos_command();
	}

      break;
      
    case IEC_TALK:
#ifdef DEBUG_PIEC
      printf("changed state: TALK\n");
#endif
      temp=receive_byte(); 
      if((temp&0x100)==0) { change_state(IEC_IDLE); break; }
      SA=temp;
#ifdef DEBUG_PIEC
      printf("SA=%d\n",(uchar)SA);
#endif
      if((SA&0xf0)==IEC_LOAD) {
	/* Read all data in current file, and then send it.
	   XXX - Should use a more interactive talk routine 
	   which doesn't spend the time pre-reading the file
	   before sending it. */
	talklf=SA&0xf;
	file_unit=last_unit;

	output_buffer=malloc(1048576);
	outputlen=0;
	
	if (talklf!=0xf)
	  {
	    while(!fs64_readchar(&logical_files[file_unit][talklf], 
				 &output_buffer[outputlen]))
	      outputlen++;
	  }
	else
	  {
	    /* Read DOS status */
	    bcopy(dos_status[last_unit],output_buffer,dos_stat_len[last_unit]);
	    outputlen=dos_stat_len[last_unit];
	    set_error(0,0,0);
	  }
      }
      else {
	//output_buffer=execute_command(input_buffer);

	//this data is not needed anymore
	if(inputlen>0) free(input_buffer);
      }
      talk();
      change_state(IEC_IDLE);
      if(outputlen>0) free(output_buffer);
      break;
      
    case IEC_UNLISTEN:
      //what to do here actually? The transfer is stoped, so all done
#ifdef DEBUG_PIEC
      printf("changed state: UNLISTEN\n");
#endif
      //and we can fall into IDLE state to wait for new challanges.
      change_state(IEC_IDLE);
      break;
      
    case IEC_UNTALK:
      //what to do here actually? Guess same as in UNLISTEN case
#ifdef DEBUG_PIEC
      printf("changed state: UNTALK\n");
#endif
      change_state(IEC_IDLE);
      break;
      
    case IEC_IDENTIFY:
#ifdef DEBUG_PIEC
      printf("changed state: IDENTIFY\n");
#endif
      /* we are asked to idenitfy ourself, so present the deviceumber under
	 which we listen. */
      send_byte(devicenumber);				
      change_state(IEC_IDLE);
      break;
    }
  }
  return -1;
}

unsigned int iec_listen() {
  unsigned int temp;
  int maxcount=1024;
  inputlen=0;
  input_buffer=malloc(maxcount);

  /* Record file number for listen */
  listenlf=SA&0xf;

#ifdef DEBUG_PIEC
  printf("Receiving data...\n");
#endif
  while(1) {
    /* receive a byte */
    temp=receive_byte();
    //check if under ATN
    if((temp&0x100)!=0) {
      //yes: abort transfer and change to new state
      change_state(temp);
      break; 
    }
    // byte not under attention: store in input buffer
    input_buffer[inputlen]=temp;
    inputlen++;
    if(inputlen>=maxcount) 
      { 
	maxcount+=1024; 
	input_buffer=realloc(input_buffer,maxcount); 
      }
  }
  /* We have received all data - so crop array to correct size and return */
  input_buffer=realloc(input_buffer,inputlen+1+2 /* for ,R or ,W */);
  input_buffer[inputlen]=0;

  /* Having received data, decide what to do with it */
  switch (SA&0xf0)
    {
    case 0xf0: /* OPEN */
      /* Data received was a filename to open */
      
      /* Add ,W to logical file 1 (save) and ,R to logical file 0 (load) */
      if ((SA&0xf)==1) strcat(input_buffer,",W");
      if ((SA&0xf)==0) strcat(input_buffer,",R");

#ifdef DEBUG_PIEC
      printf("Received filename \"%s\" for logical file %d\n",
	     input_buffer,listenlf);
#endif

      file_unit=last_unit;

      if (logical_files[file_unit][listenlf].open == 1)
	fs64_closefile_g (&logical_files[last_unit][listenlf]);
      debug_msg ("*** Opening [%s] on channel $%02x\n",input_buffer, listenlf);

      if (listenlf==15)
	{
	  /* filename is DOS command */
	  input_buffer[inputlen]=0;
	  strncpy(dos_command[last_unit],input_buffer,255);
	  dos_command[last_unit][255]=0;
	  dos_comm_len[last_unit]=(inputlen<256) ? inputlen : 255;
	  do_dos_command();
	}
      else
	if (fs64_openfile_g (curr_dir[last_unit][curr_par[last_unit]],
			     input_buffer, &logical_files[file_unit][listenlf]))
	  {
	    /* open failed
	       XXX - What do we do here to signal file not found?
	       How about send an EOI? */
	    send_eoi();
	  }
	else 
	  {
	    /* open succeeded.
	       XXX - What do we do here? nothing I suppose */
	  }
      free(input_buffer); inputlen=0;
      break;
    case 0xe0: /* CLOSE */
      /* Close file from SA */
#ifdef DEBUG_PIEC
      printf("Closing logical file %d\n",listenlf);
#endif
      fs64_closefile_g (&logical_files[last_unit][listenlf]);

      break;
    case 0x60: /* Unknown, seems to get called when opening files. */
      break;
    }

  return 0;
}

unsigned int talk() {
  unsigned int temp=0;
  outputpos=0;
#ifdef DEBUG_PIEC
  printf("Serving %d bytes of requested data...\n",outputlen);
#endif

  /* Special case for if we are asked to send 0 bytes. */
  if (outputlen<=0) 
    {
      send_eoi();
      return 0;
    }

  while(outputpos<outputlen) {	
    //send byte
    temp=send_byte(output_buffer[outputpos]);
    // Notify the C64 when we have sent the last byte
    if(outputpos==outputlen-1) send_eoi();
    else acknowledge();
    
    //Any problems during sending?
    if(temp!=0) {
      /* ATN went high during sending, quickly redraw from bus and wait
	 for further instructions */
      change_state(IEC_IDLE);
      break;							
    }
    outputpos++;

#ifdef DEBUG_PIEC
  printf("  sent %d bytes\n",outputpos);
#endif

  }
  return temp;
}

int send_byte(unsigned char data) 
{
  /* only send char if there is no ATN!
     If attention is high, wait until it goes low again */
  while(get_ATN()!=0);
  ATN=0;

  //let c64 prepare things first (#$00 -> $dd03)	
  while((get_status()&REQUEST_IN)==request) {
    if(get_ATN()==1) {
      set_datalines_input();
#ifdef DEBUG_PIEC
      printf("ATN changed!\n");
#endif
      ATN=1;
      /* ATN went high, we better stop! */
      return -1;
    }
  }
  request^=REQUEST_IN;
#ifdef DEBUG_PIEC
  printf("Request toggled\n");
#endif

  //set datalines to outputmode
  set_datalines_output();

  //write data
  write_data((data&0xff));

  /* When we send a byte, we do not immediately provide an ACK signal to the 
     C64.  This is to allow for the calling routine to decide when the data
     has been completely sent, and so if we need to assert EOI instead of 
     ACK. */

  return 0;
}

int receive_byte() {
  int data=0;

  //set datalines to inputmode
  set_datalines_input();
  
  //let c64 prepare things first (#$00 -> $dd03)
  while((get_status()&REQUEST_IN)==request);

  request^=REQUEST_IN;
#ifdef DEBUG_PIEC
  printf("Request toggled r\n");
#endif
  
  /* Check if the byte was received under attention.
     If so, OR the read byte with $100, so that we know it was received 
     under attention. */
  if(get_ATN()==1) 
    { 
#ifdef DEBUG_PIEC
      printf("data received under ATN\n");
#endif
      data|=0x100; ATN=1; 
    }
  else { ATN=0; }
  data|=(read_data()&0xff);
  acknowledge();								//we are finished, c64 can go off the bus again		
  return data;
}

int send_eoi() 
{ 
  //tested: works! :-) Connect pin 14 on lpt to CNT pin on userport

  int status;
  int a;
#ifdef DEBUG_PIEC
  printf("Sending eoi...\n");
#endif

  /* To indicate an EOI, we toggle the c64's cnt pin 8 times.
     This causes a shift register byte received notification, which
     sets bit 3 in $dd0d, which the 64net/2 wedge can see */

  status=0x00;
  ioctl(lpt_fd,PPISECR,&status);

  status=0;
  set_lpt_control(status);
  /* XXX My C128D seems to need 16 toggles, even though the documentation
     suggests 8 should be enough. */
  for(a=0;a<16;a++) {
    status|=ERROR;					
    set_lpt_control(status);
    status^=ERROR;					
    set_lpt_control(status);
  }

  status=0x00;
  ioctl(lpt_fd,PPISECR,&status);

  return 0;
}

uchar change_state(unsigned int new_state)
{
  /* Change TALK/LISTEN state based on character received under attention. */
  new_state=(new_state&0xff);
  if((new_state&0xf0)==IEC_LISTEN) 
    {
      int unit;    
      for(unit=0;unit<MAX_NET_DEVS;unit++)
	if ((new_state&0x1f)==devices[unit]) 
	  {
	    state=IEC_LISTEN;
	    last_unit=unit;
#ifdef DEBUG_PIEC
	    printf("Received Listen command for device #%d\n",
		   devices[last_unit]);
#endif
	    break;
	  }
    
	if (unit==MAX_NET_DEVS) {
#ifdef DEBUG_PIEC
	  printf("Received Listen, but for different device.\n");
#endif
	}
	return 0;
    }
  else if((new_state&0xf0)==IEC_TALK) 
    {
      int unit;    
      for(unit=0;unit<MAX_NET_DEVS;unit++)
	if ((new_state&0x1f)==devices[unit]) 
	  {
	    state=IEC_TALK;
	    last_unit=unit;
#ifdef DEBUG_PIEC
	    printf("Received Talk command for device #%d\n",
		   devices[last_unit]);
#endif
	    break;
	  }
	if (unit==MAX_NET_DEVS) {
#ifdef DEBUG_PIEC
	  printf("Received Talk, but for different device\n");
#endif
	}
	return 0;
    }
  else if((new_state&0xff)==IEC_UNLISTEN) {
    state=IEC_UNLISTEN;
#ifdef DEBUG_PIEC
    printf("Received Unlisten command\n");
#endif
    return 0;
  }	
  else if((new_state&0xff)==IEC_UNTALK) {
    state=IEC_UNTALK;
#ifdef DEBUG_PIEC
    printf("Received Untalk command\n");
#endif
    return 0;
  }

  else if((new_state&0xff)==IEC_IDLE) {
    state=IEC_IDLE;
#ifdef DEBUG_PIEC
    printf("Going into IDLE mode...\n");
#endif
    return 0;
  }

  else {
    /* Some unknown command has been received, so lets go back to IDLE,
       and wait for the next command char */
#ifdef DEBUG_PIEC
    printf("Received unknown char: 0x%X\n", new_state);
#endif
    state=IEC_IDLE;
    return -1;
  }
}

/* These configuration reading routines really should be in another module,
   but that would entail global variables which i want to avoid.
 */

int 
read_config (uchar *file)
{
  /* Read in the 64netrc file */

  FILE *cf = 0;
  uchar temp[256];

  if ((cf = fopen (file, "r")) == NULL)
    fatal_error ("Cannot read configuration file.");

  while (!feof (cf))
  {
    fgets ((char*)temp, 256, cf);
    if ((temp[0] == '#') || (temp[0] == 0) || (temp[0] == '\n') || (temp[0] == '\r'))
    {
      /* its a comment or blank line */
    }
    else
    {
      /* its a real line of stuff */
#ifdef WITH_OLD_STUFF
      if (!strncmp ("ram ", temp, 3))
      {
	/* create `geoRAM' */
	if (georam_size != 0)
	{
	  printf ("INIT: GEOS-RAM already specified - ignoring redeclaration\n");
	}
	else
	{
	  georam_size = atol (&temp[4]);
	  if (georam_size > 0)
	  {
	    if (!(georam =
		  (uchar *) malloc (georam_size * 256 + PADDING)))
	      /* couldnt malloc */
	      fatal_error ("Cannot allocate memory for GEOS-RAM (try smaller size!).");
	    printf ("INIT: GEOS-RAM created (%d pages = %dKB)\n", georam_size, georam_size / 4);
	  }
	  else
	  {
	    fatal_error ("INIT: Bad value for GEOS-RAM size - Barf!\n");
	  }
	}
      }
      else 
#endif
	if (!strncmp ("port ", temp, 4))
      {
	/* its a port line */
#ifdef AMIGA
	printf ("INIT: Using internal parallel port\n");
	strcpy (port, &temp[5]);
#else
	strcpy (port, &temp[5]);
	/* chop CR/LF */
	while((port[strlen(port)-1]=='\n')||(port[strlen(port)-1]=='\r'))
	  port[strlen(port)-1]=0;
	printf ("INIT: Communication port set to %s\n", port);
#endif
      }
      else if (!strncmp ("path ", temp, 4))
      {
	/* path partition */
	pathdir = atol (&temp[5]);
	printf ("64NET/2 will look in partition %d for programmes\n", pathdir);
      }
      else if (!strncmp ("devnum ", temp, 6))
      {
	/* 64net2 emulated device number */
	devnum = atol (&temp[7]);
	printf ("64NET/2 will be seen from client as device %d\n", devnum);
      }
      else if (!strncmp ("device ", temp, 6))
      {
	/* its a device line */
	read_device (cf);
      }
      else if (!strncmp ("ledfile", temp, 6))
      {
	/* its a device line */
	if (temp[strlen (temp) - 1] < ' ')
	  temp[strlen (temp) - 1] = 0;
	if (temp[7] < ' ')
	  sprintf ((char*)&temp[7], " %s/.64net2.leds", getenv ("HOME"));
	client_init (&temp[8]);
	client_activity (0);
	client_error (0);
      }
#ifdef WITH_OLD_STUFF
      else if (!strncmp ("timeout ", temp, 7))
      {
	/* set max_tries */
	max_tries = atoi (&temp[8]);
	printf ("INIT: Timeout set to %d attempts\n", max_tries);
      }
#endif
      else if (!strncmp ("debug mode", temp, 9))
      {
	/* debug mode */
	debug_mode = 1;
	printf ("INIT: Debug mode messages will be displayed.\n");
      }
#ifdef WITH_OLD_STUFF
      else if (!strncmp ("fishload", temp, 7))
      {
        /* enable fishload */
        allowFishLoad = 1;
	printf("INIT: enabled fishload routine.\n");
#ifdef USE_SERIAL_DRIVER
	allowFishLoad = 0;
	printf("INIT: fishload disabled on serial driver.\n");
#endif
      }
      else if (!strncmp ("fishsave", temp, 7))
      {
        /* enable fishsave */
	allowFishSave = 1;
	printf("INIT: enabled fishsave routine.\n");
#ifdef USE_SERIAL_DRIVER
	allowFishSave = 0;
	printf("INIT: fishsave disabled on serial driver.\n");
#endif
      }
#endif
      else
      {
	/* Unknown line type */
	fprintf(stderr,"Unknown configuration directive:\n%s\n",temp);
	fatal_error ("Bad configuration file.  Unknown line types.");
      }
    }
  }
  fclose (cf);
  cf = 0;

  /* all done! */

  /* Check for required bits */
  if (!devices[0])
  {
    /* no devices defined */
    fatal_error ("Configuration file contains no device lines.");
  }
  if (port[0] == 0)
  {
    /* no port lines */
    fatal_error ("Configuration file contains no port line.");
  }

  return (0);
}

int 
read_device (FILE * cf)
{
  /* read a device section */
  int dev_num, i, pn;
  char temp[256];

  for (dev_num = 0; dev_num < MAX_NET_DEVS; dev_num++)
    if (!devices[dev_num])
      break;

  if (dev_num >= MAX_NET_DEVS)
  {
    /* too many network devices */
    fatal_error ("Too many network devices defined.");
  }
  else
  {
    /* yup, we can cope */

    while (!feof (cf))
    {
      fgets (temp, 256, cf);
      if ((temp[0] == '#') || (temp[0] == 0))
      {
	/* comment line */
      }
      else
      {
	/* real line */
	/* Acceptables are:
	   NUMBER <dev_num>            - Sets the device # for this dev
	   PARTITION <dev_num>,<path>  - Define a partition
	   END DEVICE                  - End definition of device
	   PRINTER                     - Define device to be a printer,
	   not a drive.
	 */
	if (!strncmp ("number", temp, 6))
	{
	  /* device number */
	  devices[dev_num] = (uchar) atol (&temp[6]);
	  printf ("Networked device $%02x assigned to IEC device $%02x\n",
		  dev_num, devices[dev_num]);
	}
	else if (!strncmp ("partition", temp, 9))
	{
	  /* partition for drive */
	  /* find first comma */
	  for (i = 9; i < strlen (temp); i++)
	    if (temp[i] == ',')
	      break;
	  if (i >= strlen (temp))
	    fatal_error ("Bad partition line (no commas).");
	  pn = atol (&temp[9]);
	  if ((pn < 1) || (pn > 255))
	  {
	    /* bad partition # */
	    fatal_error ("Bad partition number. Must be between 1 and 255.");
	  }
	  else
	  {
	    /* okey */
	    partn_dirs[dev_num][pn] =
	      (uchar *) malloc (strlen (&temp[i + 1]) + PADDING);
	    if (!partn_dirs[dev_num][pn])
	      /* couldnt malloc */
	      fatal_error ("Cannot allocate memory.");
	    else
	    {
	      /* strip newline */
	      uchar partition[8], path[1024];
	      partition[0] = 'n';
	      temp[strlen (temp) - 1] = 0;
	      strcpy (partn_dirs[dev_num][pn], &temp[i + 1]);
	      printf ("  %s added as partition %d on unit $%02x\n",
		      partn_dirs[dev_num][pn], pn, dev_num);
	      /* parse for .DHD sub-directories */
	      partn_dirtracks[dev_num][pn] = -1;
	      partn_dirsectors[dev_num][pn] = -1;
	      curr_dirtracks[dev_num][pn] = -1;
	      curr_dirsectors[dev_num][pn] = -1;
	      strcpy (path, partn_dirs[dev_num][pn]);
	      if (path[0] != '@')
	      {
		if (fs64_resolve_partition (partition, path,
					    &partn_dirtracks[dev_num][pn], &partn_dirsectors[dev_num][pn]))
		{
		  /* failed */
		  printf ("Invalid partition path for %d\n", pn);
		  fatal_error ("Invalid partition table\n");
		}
		curr_dirtracks[dev_num][pn] = partn_dirtracks[dev_num][pn];
		curr_dirsectors[dev_num][pn] = partn_dirsectors[dev_num][pn];
		debug_msg ("  (%s T%d S%d)\n", path,
		partn_dirtracks[dev_num][pn], partn_dirsectors[dev_num][pn]);
		free (partn_dirs[dev_num][pn]);
		if (!(partn_dirs[dev_num][pn] =
		    (uchar *) malloc (strlen (path) + 1 + PADDING)))
		  /* couldnt malloc */
		  fatal_error ("Cannot allocate memory.");
		strcpy (partn_dirs[dev_num][pn], path);
	      }
	    }
	  }
	}
	else if (!(strncmp ("end ", temp, 3)))
	{
	  /* Done definition */
	  return (0);
	}
      }				/* end of not a comment */
    }				/* end of while !feof */
    fatal_error ("Unexpected end of configuration file.");
  }				/* end of we can handle another drive */

  return (0);
}

int 
which_unit (int dev)
{
  /* return the network unit number for dev */
  int i;

  for (i = 0; i < MAX_NET_DEVS; i++)
  {
    if ((dev & 0x1f) == devices[i])
      return (i);
  }

  /* no luck */
  return (-1);
}

int 
set_drive_status (uchar *string, int len)
{
  /* set the drive message */
  int d;

  /* Only one drive at present */
  /*   d=which_unit(last_drive); */
  d = 0;

  if (d < 0)
    return (1);			/* no drive polled */

  /* copy string */
  memcpy (dos_status[d], string, len+1);

  /* and set length */
  dos_stat_len[d] = len;
  return (0);
}


/*
  -------------------------------------------------------------------------
  End of new parallel IEC code 
*/

#ifdef OLD_OPERATION

int commune (void)
{
  /* The actual programme loop which does the work of talking to the C64/C128
     or C65.
     This should be sufficiently general to work over serial, parallel or 
     otherwise (including real IEC buss).
   */

  int a;

  init_hw ();

  /* Announce registration details */
  printf ("64NET/2 is covered by the 64net/2 BSD license\n");

  starthw();

  while (1)
  {

    a = syncchar();

    if (a&0x100)
      {
	/* We have received a character under attention,
	   so switch to fast IEC mode */
	printf("Char under attention: %02x\n",a&0xff);
	parallel_iec_commune(a);
      }
    else
      {
	switch (a)
	  {
	  case FSACCEL:
	    /* File System Accelerator */
	    fs_accel (charget());
	    goto next;
	  case OPENDISABLE:
	    dont_open=1;
	    goto next;
	  case SAVE:
	    do_save();
	    goto next;
	  case LOAD:
	    do_load();
	    goto next;
	  case TIME:
	    {
	      int i, timebuf[8];
	      current_time(&timebuf[0]);
	      for (i=0;i!=8;i++)
		sendchar(timebuf[i]);
	    }
	    goto next;
	  case TIMESTRING:
	    {
	      int i;
	      char *text;
	      
	      text = current_time_string();
	      asciitopetscii(text);
	      for (i = 0; i < strlen (text); i++)
		sendchar (text[i]);
	      sendchar (0);
	    }
	    goto next;
	  case DEVCHECK:
	    {
	      /* check device, is it one we being maintained by 64net.
		 This is used by the client to determine which buss to
		 talk to */
	      debug_msg ("Device check\n");
	      if (which_unit (charget ()) > -1)
		{
		  /* 64net device */
		  sendchar (1);
		}
	      else
		{
		  /* non-64net device */
		  sendchar (0);
		}
	    }
	    goto next;
	  case BOOT:
	    {
	      do_boot();
	      /* return to (almost) startup state */
	      listenlf = -1;
	      talklf = -1;
	    }
	    goto next;
	  case OPEN:
	    do_open_small();
	    goto next;
	  case CLOSE:
	    do_close_small();
	    goto next;
	  case CHKIN:
	    do_chkinout(0);
	    goto next;
	  case CHKOUT:
	    do_chkinout(1);
	    goto next;
	  case SECOND:
	    {
	      /* second system call, get secondary address after LISTEN, that is
		 filenumber (channel) and mode (6x=data flow, ex=name, fx=close) */
	      listenlf = charget();
	      debug_msg ("Second call, sa=$%02x, channel=%i\n",listenlf & 0xf0,listenlf & 0x0f);
	      switch (listenlf & 0xf0) {
	      case 0xf0:	/* name incoming */
		fnlen = 0;
		filename[fnlen]=0;
		break;
	      case 0xe0:	/* close a file */
	      case 0x60:	/* get some data - open a file */
		break;
	      }
	    }
	    goto next;
	  case TKSA:
	    {
	      /* tksa system call, get secondary address after TALK, that is
		 filenumber and mode, mode can be only 0x6x or 0xfx (data or close) */
	      talklf = charget();
	      debug_msg ("Tksa call, sa=$%02x, channel=%i\n",talklf & 0xf0,talklf & 0x0f);
	      switch (talklf & 0xf0) {
	      case 0xf0:	/* name??? - shouldn't happen */
	      case 0xe0:	/* close */
	      case 0x60:	/* send some data - open a file */
	    	break;
	      }
	      
	    }
	    goto next;
	  case LISTEN:
	  case TALK:
	    {
	      int i;
	      /* listen/talk system call, get device number and merely ignore it
		 (multi-device mode not yet supported) */
	      i = charget();
	      if (a==LISTEN) debug_msg ("Listen call on device %i\n",i);
	      if (a==TALK) debug_msg ("Talk call on device %i\n",i);
	    }
	    goto next;
	  case CIOUT:
	    {
	      /* ciout system call, receive something and store it */
	      if (listenlf < 0)
		{
		  /* listening without a listener */
		  /* this one's for the bit bucket */
		  debug_msg ("Listen without listener: %i\n",charget());
		  /* charget (); */
		} else {
		  switch (listenlf & 0xf0) {
		  case 0xf0:		/* fill in name buffer */
		    if ((listenlf & 0x0f) == 0x0f) {
		      do_ciout(listenlf); }
		    else {
		      filename[fnlen++]=charget();
		      filename[fnlen]=0; }
		    break;
		  case 0xe0:
		    charget();	/* illegal - drop it */
		    debug_msg ("Unneeded CIOUT call (on close)\n");
		    break;
		  case 0x60:		/* do file stuff */
		    do_ciout(listenlf);
		    break;
		  }
		}
	    }
	    goto next;
	  case ACPTR:
	    {
	      /* acptr system call, get something and send it */
	      if (talklf < 0)
		{
		  debug_msg ("talking without a talker\n");
		  /* talking without a talker */
		  /* send reply */
		  sendchar (128);
		  sendchar (199);
		  /* and set error */
		  set_error (70, 0, 0);
		} else {
		  
		  switch (talklf & 0xf0) {
		  case 0xf0:
		  case 0xe0:
		    sendchar (128);	/* send kinda error */
		    sendchar (199);
		    break;
		  case 0x60:		/* send file data */
		    do_acptr(talklf);
		    break;
		  }
		}
	    }
	    goto next;
	  case UNLISTEN:
	  case UNTALK:
	    {
#ifdef DEBUG
	      if (a==UNLISTEN) debug_msg ("Unlisten call on channel %i\n",listenlf & 0x0f);
	      if (a==UNTALK) debug_msg ("Untalk call on channel %i\n",talklf & 0x0f);
#endif
	      /* untalk/unlisten system call, lower all talk flags, close files, (send error?) */
	      lastlf = (a==UNLISTEN) ? listenlf : talklf;
	      /* do_dos_command if after listen and channel 15 */
	      if (a==UNLISTEN) listenlf = -1; else talklf = -1;
	      
	      if ((a==UNLISTEN) && ((lastlf & 0x0f)==15)) {
    		debug_msg ("Processing dos command\n");
		if (dos_comm_len[last_unit]!=0) do_command();
		sendchar (0);	/* UN{TALK,LISTEN} always return status code OK, read dos_status for more */
	      } else {
		switch(lastlf & 0xf0) {
		case 0xf0:	/* got full name - open it */
		  if ((lastlf & 0x0f)!=0x0f) do_open(lastlf);
#ifdef USE_SERIAL_DRIVER
		  sendchar(0);
#endif
		  break;
		case 0xe0:	/* close that file */
		  if ((lastlf & 0x0f)!=0x0f) do_close(lastlf);
		  break;
		case 0x60:	/* err, what was that? flush buffers? anyway return no errors */
		  sendchar (128);	/* XXX EOF here? or 0 as OK? */
		}		/* end switch(devlf...	*/
	      }			/* end else...		*/
	    }			/* end UNTALK:		*/
	  }				/* end switch(a)	*/
      }
  next:
    ;
  }				/* while(1)		*/
  /* will be never reached */
  return 0;
}				/* int commune()	*/

/* do a peek from the C64's memory */
int 
c64peek (long location)
{

  sendchar (0xff);
  sendchar (0x02);
  sendchar (0x02);
  sendchar (location & 0xff);
  sendchar (location >> 8);

  return (charget ());

}

int 
c64poke (long location, int value)
{

  sendchar (0xff);
  sendchar (0x01);
  sendchar (0x02);
  sendchar (location & 0xff);
  sendchar (location >> 8);
  sendchar (value);

  return (0);

}

int 
c64print (uchar *text)
{
#ifdef USE_LINUX_KERNEL_MODULE
    asciitopetscii(text);
    sendchar (0xfd);
    fishsendblock(strlen(text)+1,text);
#else
  int i;

  asciitopetscii(text);

  sendchar (0xfd);
  for (i = 0; i < strlen (text); i++)
    sendchar (text[i]);
  sendchar (0);
#endif
  return (0);
}

int 
c64jsr (int addr, int a, int x, int y)
{
  sendchar (0xf8);
  sendchar (addr & 0xff);
  sendchar (addr >> 8);
  sendchar (y & 0xff);
  sendchar (x & 0xff);
  sendchar (a & 0xff);

  return (0);
}

#endif


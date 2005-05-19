/* 
   64NET/2 Commune module
   (C)Copyright Paul Gardner-Stephen 1995, 1996, 2003-2005 All Rights Reserved.

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
#include "dosemu.h"

int parallel_iec_commune(int first_char);

extern int piec_delaycount;

#define PADDING 1

/* ports */
#ifdef USE_SERIAL_DRIVER
uchar port[1024] = COMM_DEVICE;
#else
uchar port[1024] = "/dev/parport0"; // LPT_DEVICE;
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
/* position of char we actually talking */
uchar dos_stat_pos[MAX_NET_DEVS];
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

/* our lpt device filedescriptor */
int lpt_fd;


/* #define DEBUG_PIEC */
//#define DEBUG_PIEC
#define FLIPFLOP

#include <fcntl.h>
#include <stdio.h>
#include <sys/poll.h>
#include <sys/select.h>
#include <time.h>

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
#define IEC_UNLISTEN		0x3f
#define IEC_IDENTIFY		0xff

//special SAs for LOAD/SAVE
#define IEC_LOAD		0x60
#define IEC_SAVE		0x60
#define IEC_OPEN		0xf0
#define IEC_CLOSE		0xe0
#define ERROR_EOI		0x40
#define ERROR_FILE_NOT_FOUND	0x02
#define ERROR_FILE_EXISTS	0x03
#define uchar		unsigned char

unsigned char request;
int devicenumber=9;
uchar SA;
int state=IEC_IDLE;
int myfilenamelen=0;
unsigned char* myfilename;

/* for measuring transfertimes */
clock_t transfertime;
long transferred_amount=0;

unsigned int iec_listen();
unsigned int iec_talk();
void iec_unlisten();
void iec_untalk();
void iec_idle();
unsigned char change_state(unsigned int);
int send_byte(unsigned char data, int error_code);
int receive_byte(int error_code, int wait);
int start_server();
int send_error(unsigned char err);
void begin_measure();
void end_measure();


//todo: maybe we add also the resetpin? If the c64 resets also the 64net/2 can
// reset and get into IDLE mode again. 

int parallel_iec_commune(int unused) {
	lpt_fd=bind_to_port(port); /*doesn't soemhow work with the port variable form above*/
	if (lpt_fd<0) {
		fprintf(stderr,"Failed to open parallel port.\n");
		perror("open_lptport");
	}
	last_unit=0; file_unit=0;
	set_datalines_input();
	set_lpt_control(0); // drop strobe
//	while(1) {
//		printf("$%x\n",get_status());
//	}
	request=get_status()&REQUEST_IN; //get initial state of PA2 from C64
	while(start_server()==-1);
	return 0;
}

int start_server() {
	for(;;) {
		switch(state) {
			case IEC_IDLE:
				#ifdef DEBUG_PIEC
				printf("changed state: IDLE\n");
				#endif
      				iec_idle();
			break;

			case IEC_LISTEN:
				#ifdef DEBUG_PIEC
				printf("changed state: LISTEN\n");
				#endif
				iec_listen();
			break;
      
			case IEC_TALK:
				#ifdef DEBUG_PIEC
				printf("changed state: TALK\n");
				#endif
				iec_talk();
			break;
      
			case IEC_UNLISTEN:
				#ifdef DEBUG_PIEC
				printf("changed state: UNLISTEN\n");
				#endif
      				iec_unlisten();
			break;
      
			case IEC_UNTALK:
				#ifdef DEBUG_PIEC
				printf("changed state: UNTALK\n");
				#endif
				iec_untalk();
			break;
			
			case IEC_IDENTIFY:
				#ifdef DEBUG_PIEC
				printf("changed state: IDENTIFY\n");
				#endif
				//iec_identify();
			break;
		}
	}
	return -1;
}

void iec_idle() {
	int temp;
	//no need to waste 100% CPU time when being idle, so set a polling interval of 1ms
	temp=receive_byte(-1,1000); 
	if((temp&0x100)!=0) change_state(temp);
	return;
}

void iec_identify() {
	/* we are asked to idenitfy ourself, so present the deviceumber under
	 which we listen. */
	change_state(IEC_IDLE);
	send_error(devicenumber);				
	return;
}

void iec_untalk() {
	/* all we should do si stop talking, and that we do anyway as
	 * atn came high. So we just fall into idle mode and wait
	 * for new commands */
	change_state(IEC_IDLE);
	return;
}

void iec_unlisten() {
	change_state(IEC_IDLE);
	//now we can clear the pos, as we closed the file anyway and clear the status;
	//dos_stat_pos[file_unit]=0;
	//set_error(0, 0, 0);
	//now we can fall into IDLE state to wait for new challanges.
	return;
}

unsigned int iec_listen() {
	unsigned int temp;
	int maxcount=1024;
	int pos;
	change_state(IEC_IDLE);
	temp=receive_byte(-1,0); 
	/* we expect a SA coming under ATN high, if something is suspect we fall back to idle */
	if((temp&0x100)==0) return -1;
	SA=temp;
#ifdef DEBUG_PIEC
	printf("listen SA=$%X\n",(uchar)SA);
#endif
	/* Record file number for listen */
	listenlf=SA&0xf;

	/* the data we'll receive should be saved, so we prepare for that */
	if((SA&0xf0)==IEC_SAVE && listenlf!=0xf) {
		#ifdef DEBUG_PIEC
		printf("SA commands us to save\n");
		#endif
		
		/* if the file doesn't exist yet we end up here and try to create it now */
		if (logical_files[file_unit][listenlf].open != 1) {
			#ifdef DEBUG_PIEC
			printf("Trying to open file...\n");
			#endif
			if (fs64_openfile_g (curr_dir[last_unit][curr_par[last_unit]], myfilename, &logical_files[file_unit][listenlf])) {
				#ifdef DEBUG_PIEC
				printf("Can't find file, creating it...\n");
				#endif
				/* file not found */
				/* postpend ",W" to the filename and try again */
				strcat ((char*)myfilename, ",W");
				#ifdef DEBUG_PIEC
				printf("Filename now:%s\n",myfilename);
				#endif
				if (fs64_openfile_g (curr_dir[last_unit][curr_par[last_unit]], myfilename, &logical_files[file_unit][listenlf])) {
					/* cannot open file for some reason (missing permissions or such) */
					/* simply abort */
					temp=receive_byte(ERROR_FILE_EXISTS,0);
					//unsure about the next line, but should be there for savety
					if((temp&0x100)!=0) { change_state(temp); return -1; }
					return -1;
				}
			}
		}
		//we managed to open a file and have write permissions so we can start saving into it now
		if (logical_files[file_unit][listenlf].open == 1) {
			#ifdef DEBUG_PIEC
				printf("File is open. Now receiving data to save...\n");
			#endif
			set_error(0,0,0);
			/* pour all data into that file until finished or ATN gets high */
			while(1) {
				/* receive a byte */
				temp=receive_byte(-1,0);
				/* check if data received under ATN high */
				if((temp&0x100)!=0) { change_state(temp); return -1; }
				#ifdef DEBUG_PIEC
				printf("Got byte $%x\n",temp);
				#endif
				fs64_writechar(&logical_files[file_unit][listenlf], temp);
			}
		}
	}
	else {
		switch (SA&0xf0) {
			case IEC_OPEN: /* OPEN */
				begin_measure();
				myfilenamelen=0;
				myfilename=malloc(maxcount);
				#ifdef DEBUG_PIEC
					printf("Now receiving data...\n");
				#endif
				while(1) {
					/* receive a byte */
					temp=receive_byte(-1,0);
					if((temp&0x100)!=0) { change_state(temp); break; }
					/* byte not under attention: store in input buffer */
					myfilename[myfilenamelen]=(uchar)temp;
					myfilenamelen++;
					/* XXX we should do error handling on too long filenames here. TB */
					if(myfilenamelen>=maxcount) { 
						maxcount+=1024; 
						myfilename=realloc(myfilename,maxcount); 
					}
				}
				myfilename=realloc(myfilename,myfilenamelen+1);
				myfilename[myfilenamelen]=0;

				/* we are commanded to open, either a file or a command, lets decide further */
				if(listenlf==0x0f) {
					/* Data received was a command */
					#ifdef DEBUG_PIEC
						printf("Received command \"%s\"\n", myfilename);
					#endif
					strcpy(dos_command[last_unit],myfilename);
					dos_comm_len[last_unit]=myfilenamelen;
					do_dos_command();
					if(myfilenamelen>0) { free(myfilename); myfilenamelen=0; } 
				}
				else {
					/* first we strip off any added stuff like ,p,w (TurboAss likes 
					 * to add that and else really brings us in trouble! TB */
					
					//is there anything like a ',' in our filename?
					if((char*)strchr(myfilename, ',')!=NULL) {
						//yes, so get rid of that
		         			pos=(int)((char*)strchr(myfilename, ',')-(char*)myfilename);
						myfilename[pos]=0;
						myfilenamelen=pos;
					}
					
					/* Data received was a filename to open */
					#ifdef DEBUG_PIEC
						printf("Received filename \"%s\" for logical file %d %d\n", myfilename,listenlf,myfilenamelen);
					#endif
					//if (logical_files[file_unit][listenlf].open == 1) fs64_closefile_g (&logical_files[last_unit][listenlf]);
					debug_msg ("*** Opening [%s] on channel $%02x\n", myfilename, listenlf);
					fs64_openfile_g (curr_dir[last_unit][curr_par[last_unit]], myfilename, &logical_files[file_unit][listenlf]);
				}
			break;
			case IEC_CLOSE: /* CLOSE */
				end_measure();
				if (logical_files[file_unit][listenlf].open == 1) { 
					fs64_closefile_g (&logical_files[last_unit][listenlf]);
					set_error(0,0,0);
				}
				/* Close file from SA */
				#ifdef DEBUG_PIEC
					printf("Closing logical file %d\n",listenlf);
				#endif
				/* do nothing except cleaning up */
				if(myfilenamelen>0) { free(myfilename); myfilenamelen=0; }
			       break;
			default:
				if(listenlf!=0xf) {
					#ifdef DEBUG_PIEC
						printf("Strange SA received. Let's interpret it as command then...\n");
					#endif
					/* can't handle SA, so we assume it is a just a new command */
					change_state(SA);
				}
				else {
					#ifdef DEBUG_PIEC
						printf("Receiving DOS command\n");
					#endif
						while(1) {
				/* receive a byte */
						  temp=receive_byte(-1,0);
				/* check if data received under ATN high */
						  if((temp&0x100)!=0) { change_state(temp); break; }
#ifdef DEBUG_PIEC
						  printf("Got DOS command byte $%x\n",temp);
#endif
						  if (dos_comm_len[last_unit]<255)
						    dos_command[last_unit][dos_comm_len[last_unit]++]=temp;
						}
						debug_msg ("Processing dos command\n");
						if (dos_comm_len[last_unit]!=0) do_dos_command();
						
				}
			break;
		}
	}
	return 0;
}

void begin_measure() {
	transferred_amount=0;
	transfertime=clock();
	return;
}

void end_measure() {
	float time;
	float rate;
	long timediff;
	clock_t endtime=clock();
	timediff=endtime-transfertime;
	time=((float)timediff/(float)CLOCKS_PER_SEC);
	rate=(float)transferred_amount/time;
	printf("Transferred bytes:%ld\n", transferred_amount);
	printf("Time needed:%f\n", time);
//	printf("Starttime:%d\n", transfertime);
//	printf("Endtime:%d\n", endtime);
//	printf("Timediff:%d\n", timediff);
//	printf("CLK_TCK:%d\n", CLOCKS_PER_SEC);
	printf("Transferrate:%8.2f bps (%8.3fKB/sec)\n", rate,rate/1024);
	return;	
}
	
	

unsigned int iec_talk() {
	unsigned int temp=0;
	unsigned char data=0;
	unsigned char buffer=0;
	change_state(IEC_IDLE);
	temp=receive_byte(-1,0); 
	/* we expect a SA coming under ATN high, if something is suspect we fall back to idle */
	if((temp&0x100)==0) return -1;
	SA=temp;
#ifdef DEBUG_PIEC
	printf("SA=$%X\n",(uchar)SA);
#endif
	#ifdef DEBUG_PIEC
		printf("Waiting for ATN going low first...\n");
	#endif
	/* don't start talking while atn is high, as this is really not good for our CIA
	 * and will bring us seriously out of sync too :-) */	
	while((get_status()&ATN_IN)!=ATN_IN) {
	#ifdef DEBUG_PIEC
		printf(".");
	#endif
	}
	#ifdef DEBUG_PIEC
		printf("ATN is low now, so serving requested data...\n");
	#endif

	talklf=SA&0xf;
	if((SA&0xf0)==IEC_LOAD && talklf!=0x0f) {
		//we can now clear the filename, hopefully
		//not really needed here, as it will be done on close
		//if(myfilenamelen>0) { free(myfilename); myfilenamelen=0; }
		//do we have an open file?
		if (logical_files[file_unit][listenlf].open != 1) {
			/* File not found, now signal it with sending a dummy char followed by a timeout */
			set_error(62,0,0);
			if(send_byte(0,ERROR_FILE_NOT_FOUND)==-1) return -1;
			return 0;
		}
		else {
			/* start sending bytes until finished or ATN high */
			
			if(!fs64_readchar(&logical_files[file_unit][talklf],&data)) {
				while(1) {
					buffer=data;
					if(fs64_readchar(&logical_files[file_unit][talklf],&data)) break;
					if(send_byte(buffer, -1)==-1) return -1; 	//send actual byte 
				}
			}
			set_error(0,0,0);
			if(send_byte(buffer,ERROR_EOI)==-1) return -1; 			//now send last byte + EOI
			return 0;
		}
	}
	if(talklf==0x0f) {
		/* the c64 reads out the error channel, so talk the status */
		#ifdef DEBUG_PIEC
			printf("Sending status123...\n");
			//printf("Status: %s", dos_status[last_unit]);
		#endif
		while(1) {
			data=dos_status[last_unit][dos_stat_pos[last_unit]];
			if(send_byte(data,-1)==-1) return -1;
			if(data!=0x0d) {
				dos_stat_pos[last_unit]++;
			}
			else {
				set_error(0,0,0);
				return 0;
			}  
		}
	}
	return 0;
}

int send_byte(unsigned char data, int error_code) {
	unsigned char temp;

	/* we have to make a decision here out of a single look
	 * on the status. If we do two checks we read the status 
	 * twice and probably do a check on two different values 
	 * of status! Thus the decision might be faulty and we 
	 * get into big troubles! */
#ifdef FLIPFLOP
	set_datalines_output();
	write_data((data&0xff));
	if(error_code==-1) acknowledge();
	else send_error(error_code);
#endif
	while(1) {
		temp=get_status();
		if((temp&ATN_IN)!=ATN_IN) {
			set_datalines_input();
			request=temp&REQUEST_IN;
			#ifdef DEBUG_PIEC
				printf("ATN changed!\n");
			#endif
			/* ATN went high, we better stop! */
			return -1;
		}
		if((temp&REQUEST_IN)!=request) break;
	}
	request^=REQUEST_IN;
	#ifdef DEBUG_PIEC
		printf("Request toggled $%x $%x\n",get_status(),request);
	#endif
#ifndef FLIPFLOP
	set_datalines_output();
	write_data((data&0xff));
	if(error_code==-1) acknowledge();
	else send_error(error_code);
#endif
	/* Record bytes sent for calculating LOAD/SAVE speed */
	transferred_amount++;

	/* When we send a byte, we do not immediately provide an ACK signal to the 
	 * C64.  This is to allow for the calling routine to decide when the data
	 * has been completely sent, and so if we need to assert EOI instead of 
	 * ACK. */
	return 0;
}

int wait_request() {
	unsigned char temp;
	while(1) {
		temp=get_status();
		if((temp&ATN_IN)!=ATN_IN) {
			set_datalines_input();
			request=temp&REQUEST_IN;
			#ifdef DEBUG_PIEC
				printf("ATN changed!\n");
			#endif
			/* ATN went high, we better stop! */
			return -1;
		}
		if((temp&REQUEST_IN)!=request) break;
	}
	request^=REQUEST_IN;
	return 0;
}

/* Added a polling-interval value to avoid unnecessary 
 * cpu-cycles when being idle */
int receive_byte(int error_code, int wait) {
	int data=0;

	#ifdef DEBUG_PIEC
	printf("Receiving data...\n");
	#endif
	//set datalines to inputmode
	set_datalines_input();
	//let c64 prepare things first and wait (#$00 -> $dd03)
	#ifdef DEBUG_PIEC
	printf("Waiting for request to toggle...\n");
	#endif
	while((get_status()&REQUEST_IN)==request) {
		if(wait>0) usleep(wait);
	}
	request^=REQUEST_IN;
	#ifdef DEBUG_PIEC
	printf("Request toggled r\n");
	#endif
  
	/* Check if the byte was received under attention. 
	 * This is always in sync as the C64 can't change ATN in that situation 
	 * as it is polling for cnt or flag coming high from our acknowledge.
	 * If ATN high, OR the read byte with $100, so that we know it was received 
	 * under attention. */
	if((get_status()&ATN_IN)!=ATN_IN) { 
		#ifdef DEBUG_PIEC
		printf("data received under ATN\n");
		#endif
		data|=0x100; 
	}
	data|=(read_data()&0xff);
	if(error_code==-1) acknowledge();
	else send_error(error_code);

	/* Record number of bytes transfered so that we can determine SAVE speed */
	transferred_amount++;

	return data;
}

int send_error(unsigned char err) {
	int status;
	int a;
	struct timeval time;
	time.tv_sec = 0;
	time.tv_usec = 5; //5탎 is on teh save side! with 1탎 i sometimes fail to send the error! TB

	#ifdef DEBUG_PIEC
	printf("Triggering CNT...\n");
	#endif

	/* XXX My C128D seems to need 16 toggles, even though the documentation
	 * suggests 8 should be enough. */
	/* XXX unfortunatedly when toggling 16 times the c64 + 64net hangs (guess some
	 * double triggered error :-( */
	/* all this is hopefully solved now! Actually the pc sometimes toggles things 
	 * too fast here. So i keep every state up at least 1탎, that is more than 
	 * one cycle on c64 side. Since that also serial transfer of the errorcode
	 * to SP2 works stable. */
	for(a=0;a<8;a++) {
		status=ERROR_CLOCK;				
		if((err&0x80)!=0) status|=ERROR_DATA;
		select(1,NULL,NULL,NULL,&time);	//wait 1탎
		set_lpt_control(status);
		status^=ERROR_CLOCK;					
		select(1,NULL,NULL,NULL,&time); //wait 1탎
		set_lpt_control(status);
		err=err<<1;
	}
	return 0;
}

uchar change_state(unsigned int new_state) {
	/* Change TALK/LISTEN/UNLISTEN/UNTALK/IDENTIFY state based on character received under attention. */
	new_state=(new_state&0xff);
		if((new_state&0xf0)==IEC_LISTEN) {
			if(devicenumber==(new_state&0x0f)) {
				state=IEC_LISTEN;
				#ifdef DEBUG_PIEC
				printf("Received Listen command\n");
				#endif
    			}
			else {
				#ifdef DEBUG_PIEC
				printf("Received Listen, but for different device.\n");
				#endif
			}
			return 0;
		}
		else if((new_state&0xf0)==IEC_TALK) {
			if(devicenumber==(new_state&0x0f)) {
				state=IEC_TALK;
				#ifdef DEBUG_PIEC
				printf("Received Talk command\n");
				#endif
			}
			else {
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
		 * and wait for the next command char */
		#ifdef DEBUG_PIEC
		printf("Received unknown char: 0x%X\n", new_state);
		#endif
		state=IEC_IDLE;
		return -1;
	}
	return -1;
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
  //Fixed: need to set pos to 0!! TB
  dos_stat_pos[d] = 0;
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
		if (dos_comm_len[last_unit]!=0) do_dos_command();
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


/* 
   64NET/2 Commune module
   (C)Copyright Tobias Bindhammer 2006 All Rights Reserved.

   This module does all the C64 <--> Server communications
   RR-Net version
 */

#include "config.h"
#include "fs.h"
#include "comm-rrnet.h"
#include "client-comm.h"
#include "fs_accel.h"
#include "misc_func.h"
#include "comm-work.h"
#include "datestamp.h"
#include "dosemu.h"

#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#include <sys/timeb.h>
#include <sys/types.h>
#include <sys/time.h>


//#include <sys/ioctl.h>
//#include <sys/socket.h>
//#include <net/if.h>

#define PADDING 1

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

#define DEBUG_PIEC


//different states
#define IEC_IDLE		0x00
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

#define PACKET_DATA		0x44
#define PACKET_ACKNOWLEDGE	0x41
#define PACKET_COMMAND		0x43
#define PACKET_ERROR		0x45

#define DATA_PAYLOAD_SEND	0xfe
#define DATA_PAYLOAD_RECV	0xfe

int socket_out;
int socket_in;
int packet_number;
unsigned char in_buffer[256];
unsigned char out_buffer[256];
int in_size;
int out_size;
struct sockaddr_in receiver = { 0 };
struct sockaddr_in sender = { 0 };
int client_port = 5000; 
char client_ip[] = "192.168.2.64"; 
unsigned char port[1024]; 
char client_mac[1024] = "00:00:00:64:64:64"; 
int packet_type;

int devicenumber=9;
uchar SA;
int state=IEC_IDLE;
int myfilenamelen=0;
unsigned char* myfilename;

/* for measuring transfertimes */
clock_t starttime;
clock_t endtime;

struct timeb start = { 0 };
struct timeb end = { 0 };

long transferred_amount=0;

unsigned int iec_listen();
unsigned int iec_talk();
void iec_unlisten();
void iec_untalk();
void iec_idle();
unsigned char change_state(unsigned int);
int start_server();
void send_error(unsigned char err);
void begin_measure();
void end_measure();
void send_acknowledge();
void receive_packet();
int wait_for_acknowledge();


int iec_commune(int unused) {
        struct  hostent *hp, *gethostbyname();
//	struct arpreq ar = { 0 };
//	struct sockaddr req_ip = { 0 };
        hp = gethostbyname(client_ip);

	//install receiver socket
        receiver.sin_family=AF_INET;
        receiver.sin_port=htons(client_port);
        receiver.sin_addr.s_addr=htonl(INADDR_ANY);
        socket_in = socket (AF_INET,SOCK_DGRAM, IPPROTO_UDP);

	//install sender socket
        sender.sin_family = AF_INET;
        sender.sin_port = htons(client_port);
        socket_out = socket (AF_INET,SOCK_DGRAM,IPPROTO_UDP);
        bcopy ( hp->h_addr, &(sender.sin_addr.s_addr), hp->h_length);

        bind(socket_in, (struct sockaddr *) &receiver, sizeof(receiver));

	//bind input socket
	if (socket_out<0 || socket_in<0) {
		fprintf(stderr,"Failed to bind to port %d to listen for UDP packets.\n",client_port);
		perror("open_port");
	}
	last_unit=0; file_unit=0;
	
//	ar.arp_pa=gethostbyname(client_ip)->h_addr;
	
	//open(
	//ioctl(s, SIOCGARP, (caddr_t)&ar);
		
	while(start_server()==-1);
	return 0;
}

int start_server() {
	for(;;) {
		switch(state) {
			case IEC_IDLE:
      				iec_idle();
			break;

			case IEC_LISTEN:
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
	#ifdef DEBUG_PIEC
	printf("changed state: IDLE\n");
	#endif
	receive_packet();
	if(in_buffer[0]==PACKET_COMMAND) {
		#ifdef DEBUG_PIEC
		printf("We got a command packet\n");
		#endif
		change_state(in_buffer[1]);
	}
	else {
		#ifdef DEBUG_PIEC
		printf("We got an unknown packet. ignoring.\n");
		#endif
	}
	send_acknowledge();
	return;
}

int wait_for_acknowledge() {
	#ifdef DEBUG_PIEC
	printf("Waiting for acknowledge...");
	#endif
	while(1) {
		receive_packet();
		if(packet_type==PACKET_ACKNOWLEDGE) break;
		if(packet_type==PACKET_COMMAND) break;
	}
	if(packet_type==PACKET_COMMAND) { 
		send_acknowledge();
		change_state(in_buffer[1]); return -1; 
	}
	#ifdef DEBUG_PIEC
	printf("OK\n");
	#endif
	return 0;
}

void receive_packet() {
	#ifdef DEBUG_PIEC
	int i;
	#endif
	while(1) {
		in_size=recv(socket_in, in_buffer, sizeof(in_buffer), 0);
		in_buffer[in_size]=(char) NULL;
		if(in_size>1) {
			packet_type=in_buffer[0];
			if(packet_type==PACKET_ACKNOWLEDGE && in_size==2) break;
			if(packet_type==PACKET_COMMAND && in_size==2) break;
			if(packet_type==PACKET_ERROR && in_size==2) break;
			if(packet_type==PACKET_DATA) {
				#ifdef DEBUG_PIEC
				printf("size: %d\n", in_size);
				#endif
				in_size=in_buffer[in_size-1];
				transferred_amount+=(in_size);
				#ifdef DEBUG_PIEC
				printf("size: %d\n", in_size);
				for(i=0;i<0xc3;i++) {
					printf("$%X ",in_buffer[i]);
					if((i+1)%8==0) printf("\n");
				}
				#endif
				
				break;	
			}
		}
	}
	#ifdef DEBUG_PIEC
	printf("Received %d bytes large packet with content: %s\n",in_size, in_buffer);
	#endif
}

void send_data() {
	unsigned char reply[out_size+1];
	int i=0;
	reply[0]=0x44;
	reply[1]=out_size;
	while(i<out_size) { reply[2+i]=out_buffer[i]; i++; }
//	if(out_size&1==1) out_size++;
	sendto(socket_out, reply,out_size+2, 0, (struct sockaddr *) &sender, sizeof(sender));
	transferred_amount+=out_size;
	#ifdef DEBUG_PIEC
	printf("$%X bytes of data sent.\n", out_size);
	for(i=0;i<out_size+2;i++) {
		printf("$%X ",reply[i]);
		if((i+1)%8==0) printf("\n");
	}
	#endif
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
	//now we can fall into IDLE state to wait for new challanges.
	return;
}

unsigned int iec_listen() {
	int i;
	int maxcount=1024;
	#ifdef DEBUG_PIEC
	printf("changed state: LISTEN\n");
	#endif
	change_state(IEC_IDLE);
	receive_packet(); //we acknowledge later, either with ack or err!
	/* we expect a SA coming under ATN high, if something is suspect we fall back to idle */
	if(packet_type!=PACKET_COMMAND) return -1;
	SA=in_buffer[1];
	
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
//					temp=receive_byte(ERROR_FILE_EXISTS,0);
//					//unsure about the next line, but should be there for savety
//					if((temp&0x100)!=0) { change_state(temp); return -1; }
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
				receive_packet();
				if(packet_type==PACKET_COMMAND) {	//oops, new command 
					send_acknowledge();
					change_state(in_buffer[1]); break; 
				}
				for(i=0;i<in_size;i++) {
					fs64_writechar(&logical_files[file_unit][listenlf], in_buffer[i+2]);
				}
			}
		}
		else {
			//XXX missing errorhandling here. TB, could be disk full, or write protection on
		}
		return 0;
	}
	if((SA&0xf0)==IEC_OPEN || listenlf==0x0f) {  /* OPEN/DOS COMMAND */
		send_acknowledge();
		begin_measure();
		myfilenamelen=0;
		myfilename=malloc(maxcount);
		#ifdef DEBUG_PIEC
			printf("Now receiving data...\n");
		#endif
		//first receive something (filename/command)
		
		while(1) {
			receive_packet();
			if(packet_type==PACKET_COMMAND) {	//oops, new command 
				send_acknowledge();
				change_state(in_buffer[1]); break; 
			}
			if(packet_type==PACKET_DATA) {
				send_acknowledge();
				for(i=0;i<in_size;i++) {
					myfilename[myfilenamelen]=in_buffer[i+2];
					myfilenamelen++;
					/* XXX we should do error handling on too long filenames here. TB */
					if(myfilenamelen>=maxcount) {
						maxcount+=1024;
						myfilename=realloc(myfilename,maxcount);
					}
				}
			}
		}
		myfilename=realloc(myfilename,myfilenamelen+1);
		myfilename[myfilenamelen]=0;

		/* we are commanded to open, either a file or a command, lets decide further */
		if(listenlf==0x0f) {
			myfilename[myfilenamelen]=0;
			/* Data received was a command */
			#ifdef DEBUG_PIEC
				printf("Received command \"%s\"\n", myfilename);
			#endif
			strcpy((char*)dos_command[last_unit],(char*)myfilename);
			dos_comm_len[last_unit]=myfilenamelen;
			if (dos_comm_len[last_unit]!=0) do_dos_command();
			if(myfilenamelen>0) { free(myfilename); myfilenamelen=0; } 
		}
		else {
			/* Data received was a filename to open */
			debug_msg ("*** Opening [%s] on channel $%02x\n", myfilename, listenlf);
			//XXX openfile seems to be a bit buggy, has sometimes file.open==1 though it doesn't exist or is a dir.
			if(fs64_openfile_g (curr_dir[last_unit][curr_par[last_unit]], myfilename, &logical_files[file_unit][listenlf])!=0) {
				logical_files[file_unit][listenlf].open=0;
			}
		}
		return 0;
	}
	if((SA&0xf0)==IEC_CLOSE) { /* CLOSE */
		send_acknowledge();
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
		return 0;
	}
	//if non of the above SAs applies, interpret as new command.
	//TASS does such buggy things for example like sending 
	//$29, $29, $F0 = listen, listen, SA :-/
	change_state(SA);
	return 0;
}

void begin_measure() {
	transferred_amount=0;
	ftime(&start);
	return;
}

void end_measure() {
	float time;
	float rate;
	long timediff;
	ftime(&end);
	timediff=(end.time-start.time)*1000+(end.millitm-start.millitm);
	time=((float)timediff/(float)1000);
	rate=(float)transferred_amount/time;
	printf("Transferred bytes:%ld\n", transferred_amount);
	printf("Time needed:%f sec\n", time);
	printf("Transferrate:%8.2f bps (%8.3fKB/sec)\n", rate,rate/1024);
	return;	
}
	
	

unsigned int iec_talk() {
	unsigned char data=0;
	int i;
	change_state(IEC_IDLE);
	#ifdef DEBUG_PIEC
	printf("waiting for talk SA...\n");
	#endif
	receive_packet(); //we acknowledge later, either with ack or err!
	/* we expect a SA coming under ATN high, if something is suspect we fall back to idle */
	if(packet_type!=PACKET_COMMAND) return -1;
	SA=in_buffer[1];
	#ifdef DEBUG_PIEC
	printf("SA=$%X\n",(uchar)SA);
	#endif
	send_acknowledge();

	talklf=SA&0xf;
	if((SA&0xf0)==IEC_LOAD && talklf!=0x0f) {
		//we can now clear the filename, hopefully
		//not really needed here, as it will be done on close
		//if(myfilenamelen>0) { free(myfilename); myfilenamelen=0; }
		//do we have an open file?
		if (logical_files[file_unit][listenlf].open != 1) {
			/* File not found, now signal it with sending a dummy 
			 * char followed by a timeout */
			//maybe we just tried to load a dir?
			//if so, let's change to that dir!
			
			dos_comm_len[last_unit]=3+myfilenamelen;
			dos_command[last_unit][0]='C';
			dos_command[last_unit][1]='D';
			dos_command[last_unit][2]=':';
			
			for(i=0;i<myfilenamelen;i++) {
				dos_command[last_unit][3+i]=myfilename[i];
			}
			if(do_dos_command()==0) {
				printf("changed sucessful\n");
				set_error(0,0,0);
				out_buffer[0]=0x01;
				out_buffer[1]=0x08;
				out_buffer[2]=0x00;
				out_buffer[3]=0x00;
				out_size=3;
				send_data();
				if(wait_for_acknowledge()==-1) return -1;
				
				out_size=0;
				send_data();
				if(wait_for_acknowledge()==-1) return -1;
			//	if(packet_type==PACKET_COMMAND) { 
			//		send_acknowledge();
			//		change_state(in_buffer[1]); return -1; 
			//	}
				return 0;
			}
			else {
				set_error(62,0,0);
				out_size=0;
				send_data();
				if(wait_for_acknowledge()==-1) return -1;
				return 0;
			}
			#ifdef DEBUG_PIEC
			printf("File not found, sending empty data packet...\n",(uchar)SA);
			#endif
			out_size=0;
			send_data();
			if(wait_for_acknowledge()==-1) return -1;
		}
		else {
			#ifdef DEBUG_PIEC
			printf("Now trying to send data to C64...\n",(uchar)SA);
			#endif
			/* start sending bytes until finished or ATN high */
			out_size=0;
			while(!fs64_readchar(&logical_files[file_unit][talklf],&data)) {
				out_buffer[out_size]=data;
				out_size++;
				if(out_size > (DATA_PAYLOAD_SEND - 1) ) {
					send_data();
					out_size=0;
					if(wait_for_acknowledge()==-1) return -1;
				}
			}
			//now send last packet
			if(out_size>0) {
				send_data();
				if(wait_for_acknowledge()==-1) return -1;
			}
			//signal EOF
			out_size=0;
			send_data();
			if(wait_for_acknowledge()==-1) return -1;
			set_error(0,0,0);
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
			out_buffer[out_size]=data;
			out_size++;
			if(data==0x0d) {
				send_data();
				out_size=0;
				if(wait_for_acknowledge()==-1) {
					set_error(0,0,0);
					return -1;
				}
			}
			else {
				dos_stat_pos[last_unit]++;
				if(out_size>(DATA_PAYLOAD_SEND-1)) {
					send_data();
					out_size=0;
					if(wait_for_acknowledge()==-1) return -1;
				}
			}  
		}
	}
	return 0;
}

void send_acknowledge() {
	unsigned char reply[2];
	reply[0]=0x41;
	reply[1]=in_buffer[1];
	sendto(socket_out, reply,2, 0, (struct sockaddr *) &sender, sizeof(sender));
	#ifdef DEBUG_PIEC
	printf("acknowledge sent.\n");
	#endif
	return;
}

void send_error(unsigned char err) {
	unsigned char reply[2];
	reply[0]=0x45;
	reply[1]=err;
	sendto(socket_out, reply,2, 0, (struct sockaddr *) &sender, sizeof(sender));
	#ifdef DEBUG_PIEC
	printf("error status sent.\n");
	#endif
	return;
}

uchar change_state(unsigned int new_state) {
	/* Change TALK/LISTEN/UNLISTEN/UNTALK/IDENTIFY state based on character received under attention. */
	
	#ifdef DEBUG_PIEC
	printf("New state: 0x%X\n", new_state);
	#endif
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
read_config (char *file)
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
	if (!strncmp ("port ", (char*)temp, 4))
      {
	/* its a port line */
#ifdef AMIGA
	printf ("INIT: Using internal parallel port\n");
	strcpy ((char*)port, (char*)&temp[5]);
#else
	strcpy ((char*)port, (char*)&temp[5]);
	/* chop CR/LF */
	while((port[strlen(port)-1]=='\n')||(port[strlen(port)-1]=='\r'))
	  port[strlen(port)-1]=0;
	printf ("INIT: Communication port set to %s\n", port);
#endif
      }
      else if (!strncmp ("path ", (char*)temp, 4))
      {
	/* path partition */
	pathdir = atol (&temp[5]);
	printf ("64NET/2 will look in partition %d for programmes\n", pathdir);
      }
      else if (!strncmp ("devnum ", (char*)temp, 6))
      {
	/* 64net2 emulated device number */
	devnum = atol (&temp[7]);
	printf ("64NET/2 will be seen from client as device %d\n", devnum);
      }
      else if (!strncmp ("device ", (char*)temp, 6))
      {
	/* its a device line */
	read_device (cf);
      }
      else if (!strncmp ("ledfile", (char*)temp, 6))
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
      else if (!strncmp ("timeout ", (char*)temp, 7))
      {
	/* set max_tries */
	max_tries = atoi (&temp[8]);
	printf ("INIT: Timeout set to %d attempts\n", max_tries);
      }
#endif
      else if (!strncmp ("debug mode", (char*)temp, 9))
      {
	/* debug mode */
	debug_mode = 1;
	printf ("INIT: Debug mode messages will be displayed.\n");
      }
#ifdef WITH_OLD_STUFF
      else if (!strncmp ("fishload", (char*)temp, 7))
      {
        /* enable fishload */
        allowFishLoad = 1;
	printf("INIT: enabled fishload routine.\n");
#ifdef USE_SERIAL_DRIVER
	allowFishLoad = 0;
	printf("INIT: fishload disabled on serial driver.\n");
#endif
      }
      else if (!strncmp ("fishsave", (char*)temp, 7))
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
	      strcpy ((char*)partn_dirs[dev_num][pn], (char*)&temp[i + 1]);
	      printf ("  %s added as partition %d on unit $%02x\n",
		      partn_dirs[dev_num][pn], pn, dev_num);
	      /* parse for .DHD sub-directories */
	      partn_dirtracks[dev_num][pn] = -1;
	      partn_dirsectors[dev_num][pn] = -1;
	      curr_dirtracks[dev_num][pn] = -1;
	      curr_dirsectors[dev_num][pn] = -1;
	      strcpy ((char*)path, (char*)partn_dirs[dev_num][pn]);
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
		strcpy ((char*)partn_dirs[dev_num][pn], (char*)path);
	      }
	    }
	  }
	}
	else if (!(strncmp ("end ", (char*)temp, 3)))
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

int which_unit (int dev)
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


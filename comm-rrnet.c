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
#include "arp.h"

#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#include <sys/timeb.h>
#include <sys/types.h>
#include <sys/time.h>


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

//#define DEBUG_COMM


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

#define MODE_READ		0x01
#define MODE_WRITE		0x02

//#define ERRORHANDLING

#define WAITFORACK		0x01
#define IDLE			0x00
#define TIMEOUT			10


//current state: 64net: resent of data,command,ack if answer missing. check on correct pktnumber on ack, cmd. check on received data still missing?
//rrnet: receive command: sends ack with pktnumber it is up to c64 to resend. packet should get one byte bigger to accomodate a real packet#. at the moment the commandvalue is used as packetnumber
//rrnet: receive data:    sends ack with pktnumber from datapacket it is up to c64 to resend
//rrnet: send data:	  resend until receiving correct ack.
//rrnet: receive ack:	  check if it is for recent sent data. if yes, stop resending and start with next packet
//
//c64: receive data:	  just send ack when fitting packet received
//c64: send data/command: resend i until correct ack received


int sendfd;
int receivefd;
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
char client_mac[] = "00:00:00:64:64:64"; 
int packet_type;

#ifdef ERRORHANDLING
	unsigned char pktnumber;
	int datalen;
	int ack_expected;
	struct timeb time_sent = { 0 };
	struct timeb time_current = { 0 };
#endif

int devicenumber=9;
int acknowledge;
unsigned SA;
int state=IEC_IDLE;
unsigned char myfilename[1024];
int myfilenamesize;

/* for measuring transfertimes */
clock_t starttime;
clock_t endtime;

struct timeb start = { 0 };
struct timeb end = { 0 };

long transferred_amount=0;

struct packet {
        unsigned char* data;
#ifdef ERRORHANDLING
	unsigned char pktnumber;
#endif
        unsigned char type;
        int size;
};
struct packet* out;

unsigned char* status;

int iec_commune(int unused) {
        struct hostent *hp;
        hp = gethostbyname(client_ip);

#ifdef _WIN32
	WSADATA wsaData;
	if (WSAStartup (MAKEWORD (1, 1), &wsaData) != 0) {
		fprintf (stderr, "WSAStartup (): Can't initialize Winsock.\n");
		exit (EXIT_FAILURE);
		return 0;
	}
#endif
	
	
	//install receiver socket
        receiver.sin_family=AF_INET;
        receiver.sin_port=htons(client_port);
        receiver.sin_addr.s_addr=htonl(INADDR_ANY);
        receivefd = socket (AF_INET,SOCK_DGRAM, IPPROTO_UDP);

	//install sender socket
        sender.sin_family = AF_INET;
        sender.sin_port = htons(client_port);
        sendfd = socket (AF_INET,SOCK_DGRAM,IPPROTO_UDP);
        bcopy (hp->h_addr, &(sender.sin_addr.s_addr), hp->h_length);

        bind(receivefd, (struct sockaddr *) &receiver, sizeof(receiver));

	//bind input socket
	if (sendfd<0 || receivefd<0) {
		fprintf(stderr,"Failed to bind to port %d to listen for UDP packets.\n",client_port);
		perror("open_port");
		exit (EXIT_FAILURE);
		return 0;
	}
	last_unit=0; file_unit=0;
	
	initialize();
	debug_msg("Adding static arp entry (IP=%s MAC=%s)\n",client_ip, client_mac);
	if(arp_set(client_ip,client_mac)) {
//		debug_msg(" success!\n");
		start_server();
	}
	else { 
		debug_msg("\n");
		fatal_error("Failed to make static arp entry\n"); 
	}
	return 0;
}

void rrnet_quit() {
	debug_msg("Deleting arp-entry for %s\n",client_ip);
	arp_del(client_ip);
}

void initialize() {
        out=(struct packet*)malloc(sizeof(struct packet));
        out->data=(unsigned char*)malloc(DATA_PAYLOAD_SEND);
        myfilenamesize=0;
        out->size=0;
        acknowledge=0;
#ifdef ERRORHANDLING
	pktnumber=0;
	datalen=0;
	ack_expected=0;
#endif
        return;
}

/* this has to be moved elsewhere into a kind of dispatcher which dispatches the appropriate
   packets to the respective server processes */

void start_server() {
	int i;
	struct packet* p;
	unsigned char buffer[1024];
	int bsize;
        p=(struct packet*)malloc(sizeof(struct packet));
        p->data=(unsigned char*)malloc(1024);
	
	while(1) {					
		bsize=recv(receivefd, buffer, sizeof(buffer), MSG_DONTWAIT);
		if(bsize>1) {
			p->type=buffer[0];
			if(p->type==PACKET_DATA) {			//assemble a data packet (0x44, len, data)
				if(bsize>1) {
					for(i=0;i<bsize-2;i++) {
						p->data[i]=buffer[i+2];
					}
					p->size=buffer[i+1];
#ifdef ERRORHANDLING
					p->pktnumber=buffer[1];
#endif
				}
				transferred_amount+=p->size;
			}
			else {						//assemble other packet (code, data)
				p->data[0]=buffer[1];
				p->size=1;
#ifdef ERRORHANDLING
				p->pktnumber=buffer[1];
#endif
			}
			process_packet(p);				//let the respective server process process the packet
		}
#ifdef ERRORHANDLING
		else {
			if(ack_expected==1) {
				ftime(&time_current);
				if ( ((time_current.time*1000+time_current.millitm)-(time_sent.time*1000+time_sent.millitm)) > TIMEOUT) {
					printf("timeout after %d ms. resending packet...\n",(int)((time_current.time*1000+time_current.millitm)-(time_sent.time*1000+time_sent.millitm)));
        				send_packet(out);
				}
				
				
			}
		}
#endif
	}
	free(p->data);
	free(p);
	return;
}

void process_packet(struct packet* p) {
	#ifdef DEBUG_COMM
	int i;
	printf ("Packet type: ");
	switch(p->type) {
		case PACKET_DATA:
			printf("data");
		break;
		case PACKET_COMMAND:
			printf("command");
		break;
		case PACKET_ACKNOWLEDGE:
			printf("acknowledge");
		break;
		case PACKET_ERROR:
			printf("error");
		break;
	}
	
	printf ("\nreceived packetsize: $%X",p->size);
	for(i=0;i<p->size;i++) {
		if((i&0x7)==0) printf("\n");
		if(p->data[i]>15) printf ("$%X ",p->data[i]);
		else printf ("$0%X ",p->data[i]);
	}
	printf("\n");
	printf("\n\n");
	#endif

	
        if(pkt_is_acknowledge(p)==1) {
		acknowledge=1;	//set flag if we got an ack
#ifdef ERRORHANDLING
		pktnumber++;	//increase pktnumber/pkt number	
		out->size=0;	//now we can clear the packet
		ack_expected=0;	//received ack, so stop expecting one
//		printf("ack_expected=0\n");
#ifdef DEBUG_COMM
	printf("\n\n");
#endif
#endif
	}
        if(p->type==PACKET_COMMAND && p->size==1) {			//got a command? 
		change_state(p->data[0]);				//then change state 
#ifdef ERRORHANDLING
		pktnumber=p->pktnumber;
		out->size=0;
		ack_expected=0;						//stop expecting acks, we got a command and new task and thus start from scratch
//		printf("ack_expected=0\n");
#ifdef DEBUG_COMM
	printf("\n\n");
#endif
#endif
		acknowledge=0;						//ignore pending acknowledges
		send_acknowledge();					//acknowledge command packet
	}
        if(p->type==PACKET_DATA) {
#ifdef ERRORHANDLING
		out->size=0;
		pktnumber=p->pktnumber;
#endif
		send_acknowledge();					//acknowledge data packet
	}
	/* now enter state machine and do the next step */
        switch(state) {
                case IEC_TALK:
                        iec_talk(p);
                break;
                case IEC_LISTEN:
                        iec_listen(p);
                break;
                case IEC_UNLISTEN:
                        iec_unlisten(p);
                break;
                case IEC_UNTALK:
                        iec_untalk(p);
                break;
        }
	
        return;
}

int pkt_is_acknowledge(struct packet* p) {
#ifdef ERRORHANDLING
	if(p->type==PACKET_ACKNOWLEDGE && p->size==1) {			//new ack with pktnumber
		if(p->pktnumber==pktnumber) {
			//offste was okay, so advance
			return 1;
		}
	}
#else
	if(p->type==PACKET_ACKNOWLEDGE && p->size==1) return 1;
#endif
	return 0;
}

void change_state(unsigned char command) {
        /* Change TALK/LISTEN/UNLISTEN/UNTALK state based on character received 
	   as for talk and listen, mask out devicenumber */

        if((command&0xf0)==IEC_LISTEN) {
                state=IEC_LISTEN;
                return;
        }
        else if((command&0xf0)==IEC_TALK) {
                state=IEC_TALK;
                return;
        }
        else if(command==IEC_UNLISTEN) {
                state=IEC_UNLISTEN;
                return;
        }
        else if(command==IEC_UNTALK) {
                state=IEC_UNTALK;
                return;
        }
        else if(command==IEC_IDLE) {
                state=IEC_IDLE;
                return;
        }
        return;
}

/* the way we act when we are in listen state */
void iec_listen(struct packet* p) {
        int i;
        unsigned char command;
        if(p->type==PACKET_COMMAND) {							//we receive another command?
                command=p->data[0];
                if((command&0xf0)==IEC_SAVE || 
		   (command&0xf0)==IEC_OPEN || 
		   (command&0xf0)==IEC_CLOSE) {						//is it something we can handle?
                        SA=command&0xf0;                        			//set SA and lf
                        listenlf=command&0x0f;

                        if(SA==IEC_CLOSE) {						//close actual file
				closefile();
                        }
                        if(SA==IEC_SAVE && listenlf!=0x0f) {				//prepare for saving
                                myfilename[myfilenamesize]=0;				
                                if(openfile(myfilename, MODE_WRITE)<0) {		//try to get write access on requested file
                                        send_error(ERROR_FILE_EXISTS);			//failed
					return;
                                }
                        }
                        if(SA==IEC_OPEN || listenlf==0xf) { 				//prepare filename
                                myfilenamesize=0;
                                myfilename[myfilenamesize]=0;
                        }
                }
                else {									//no idea what we got
                        if((command&0xf0)!=IEC_LISTEN) {
                                state=IEC_IDLE;                       			//so drop current state
                                process_packet(p);                      		//and reinterpret command packet
                        }
                }
		return;
        }
        if(p->type==PACKET_DATA) {							//we receive data
                if(SA==IEC_OPEN || (listenlf==0x0f && SA==IEC_LOAD)) {			//what we get is filename/diskcommand	
                        for(i=0;i<p->size;i++) {
                                if(myfilenamesize<1024) myfilename[myfilenamesize]=p->data[i];
                                myfilenamesize++;
                        }
                }
                if(SA==IEC_SAVE && listenlf!=0x0f) {					//what we get is data to be saved
                        for(i=0;i<p->size;i++) {
				fs64_writechar(&logical_files[file_unit][listenlf], p->data[i]);
                        }
                }
        }
        return;
}

void iec_talk(struct packet* p) {
        unsigned char command;
        unsigned char temp;
        if(p->type==PACKET_COMMAND) {
                command=p->data[0];
                if((command&0xf0)==IEC_LOAD) {
                        SA=command&0xf0;
                        talklf=command&0x0f;
                        acknowledge=1;                          			//fake acknowledge, so that we get can immediatedly send first packet (passing if statement with acknowlegde!=0)
                        if(talklf!=0x0f) {						//file or command?				
				myfilename[myfilenamesize]=0;				//finish filename
                                if(openfile(myfilename,MODE_READ)<0) {			//try to open file
                        		send_error(ERROR_FILE_NOT_FOUND);		//failed
                                        set_error(62,0,0);				//signal it
					acknowledge=0;					//clear ack flag, to recognize new acks
					return;
                                }
                        }
                }
        }
        if(SA==IEC_LOAD) {								//c64 wants to load?
                if(acknowledge!=0) {							//already got our okay for next package?
                        acknowledge=0;                  				//allow next acknowledge again
                        if(talklf!=0x0f) {						//file? yes, send file data
				while(out->size<DATA_PAYLOAD_SEND) {			//still bytes free in packet?
					if(fs64_readchar(&logical_files[file_unit][talklf],&temp)) break;	//oops, reached file end
                                	out->data[out->size]=temp;			//all okay, add byte
                                       	out->size++;
                                }
				if(out->size==0) {					//didn't manage to get a single byte into packet
					send_error(ERROR_EOI);				//so must be end of file
					set_error(0,0,0);
				}
				else {							//have some bytes, send them
					send_data(out);
				}
                        }
                        else {								//hmm, command, so send status
                                //send status until we reach end mark with 0x0d, then reset status
                                while(out->size<DATA_PAYLOAD_SEND) {			//still place in packet?
					temp=dos_status[last_unit][dos_stat_pos[last_unit]];
		                        out->data[out->size]=temp;			//add byte
               			        out->size++;					 
		                        if(temp==0x0d) {				//end reached?
						//set_error(0,0,0);			//reset status and send last packet
						break;
					}
					dos_stat_pos[last_unit]++;			//count consumed byte
				}
                        	send_data(out);						//now send packet
                        }
                }
        }
        return;
}

void iec_unlisten(struct packet* p) {
        myfilename[myfilenamesize]=0;							//finalize filename/command
        if(listenlf==0x0f) {
		strcpy((char*)dos_command[last_unit],(char*)myfilename);
		dos_comm_len[last_unit]=myfilenamesize;
		do_dos_command();							//try to execute command if we should
	}
        state=IEC_IDLE;
        return;
}

void iec_untalk(struct packet* p) {							//we just need to shut up, so let's IDLE
	if(listenlf==0x0f) {
		if(dos_status[last_unit][dos_stat_pos[last_unit]]==0x0d) 
			set_error(0,0,0);						//reset status and send last packet
	}
        state=IEC_IDLE;
        return;
}


void send_error(unsigned char err) {							//assemble error packet
#ifdef ERRORHANDLING
//	ack_expected=1;
	//remember current time for timeout
//	printf("ack_expected=1\n");
#endif
        out->size=1;
        out->data[0]=err;
	out->type=PACKET_ERROR;
        send_packet(out);
//#ifndef ERRORHANDLING
	//wait with clearing until ack arrived (if waiting for timeout)
	out->size=0;
//#endif
        return;
}

void send_acknowledge() {								//assemble acknowledge packet
        out->size=1;
#ifdef ERRORHANDLING
        out->data[0]=pktnumber;
#else
        out->data[0]=0x00;
#endif
	out->type=PACKET_ACKNOWLEDGE;
        send_packet(out);
//#ifdef ERRORHANDLING
	out->size=0;
//#endif
        return;
}

void send_data(struct packet* p) {							//send data packet
#ifdef ERRORHANDLING
	ack_expected=1;
	//remember current time for timeout
//	printf("ack_expected=1\n");
#endif
	
	out->type=PACKET_DATA;
        send_packet(out);
#ifndef ERRORHANDLING
	//wait with clearing until ack arrived (if waiting for timeout)
	out->size=0;
#endif
	return;
}

void send_packet(struct packet* p) {
	unsigned char reply[p->size+2];
	int size;
	int i=0;
	switch(p->type) {
		case PACKET_DATA:							//data packet, so add size too (code, size, data)
			reply[0]=PACKET_DATA;
#ifdef ERRORHANDLING
			//add pktnumber to packet
			printf("paketnumber: %d\n",pktnumber);
			reply[1]=pktnumber;
			reply[2]=0;
			reply[3]=p->size;
			while(i<p->size) { reply[4+i]=p->data[i]; i++; }
			size=p->size+4;
#else
			reply[1]=p->size;
			while(i<p->size) { reply[2+i]=p->data[i]; i++; }
			size=p->size+2;
#endif			
			transferred_amount+=p->size;
		break;
		default:								//other packets, just do code, data
			reply[0]=p->type;
			reply[1]=p->data[0];
			size=p->size+1;
		break;
	}
		
	sendto(sendfd, reply,size, 0, (struct sockaddr *) &sender, sizeof(sender));
#ifdef DEBUG_COMM
	printf ("Packet type: ");
	switch(p->type) {
		case PACKET_DATA:
			printf("data");
		break;
		case PACKET_COMMAND:
			printf("command");
		break;
		case PACKET_ACKNOWLEDGE:
			printf("acknowledge");
		break;
		case PACKET_ERROR:
			printf("error");
		break;
	}
	printf("\nsent packet size: $%X\n", p->size);
	for(i=0;i<size;i++) {
		if(reply[i]<16) printf("$0%X ",reply[i]);
		else printf("$%X ",reply[i]);
		if((i+1)%8==0) printf("\n");
	}
	if((i)%8!=0) printf("\n");

	#ifdef ERRORHANDLING
	if(ack_expected==1) printf ("Expecting ack\n");
	if(ack_expected==0) printf ("Not expecting ack\n");
	#endif
	printf("\n\n");
#endif
	
#ifdef ERRORHANDLING
	ftime(&time_sent);
#endif
}

void begin_measure() {									//set measure start point
	transferred_amount=0;
	ftime(&start);
	return;
}

void end_measure() {									//end of measure, calculate time needed
	float time;
	float rate;
	long timediff;
	ftime(&end);
	timediff= (end.time*1000+end.millitm)-(start.time*1000+start.millitm);
					
//	timediff=(end.time-start.time)*1000+(end.millitm-start.millitm);
	time=((float)timediff/(float)1000);
	rate=(float)transferred_amount/time;
	printf("Transferred bytes:%ld\n", transferred_amount);
	printf("Time needed:%f sec\n", time);
	printf("Transferrate:%8.2f bps (%8.3fKB/sec)\n", rate,rate/1024);
	return;	
}
	
int openfile(unsigned char* name, int mode) {						//try to open a file/dir/whatever
	int i;
	unsigned char fname[strlen(name)];
#ifdef ERRORHANDLING
	pktnumber=0;
#endif
	if (logical_files[file_unit][listenlf].open == 1) {
		debug_msg ("*** Closing previous opened file first");
		closefile();
	}
	if (logical_files[file_unit][listenlf].open != 1) {				//open already?
		if (fs64_openfile_g (curr_dir[last_unit][curr_par[last_unit]], name, &logical_files[file_unit][listenlf])) {
			/* open failed, got to react on that */
			logical_files[file_unit][listenlf].open=0;
			if(mode==MODE_WRITE) {
				/* if the file doesn't exist yet we end up here and try to create it now */
				/* file not found */
				/* postpend ",W" to the myfilename and try again */
				strcat ((char*)name, ",W");
				if (fs64_openfile_g (curr_dir[last_unit][curr_par[last_unit]], name, &logical_files[file_unit][listenlf])) {
					/* cannot open file for some reason (missing permissions or such) */
					/* simply abort */
					return -1;
				}
			}
			if(mode==MODE_READ) {
				//maybe we just tried to load a dir?
				//if so, let's change to that dir!
		
				printf("namesize:%d",strlen(name));
				dos_comm_len[last_unit]=3+strlen(name);
				dos_command[last_unit][0]='C';
				dos_command[last_unit][1]='D';
				dos_command[last_unit][2]=':';
		
				for(i=0;i<(int)strlen(name);i++) {
					dos_command[last_unit][3+i]=name[i];
				}
				if(do_dos_command()==0) {
					printf("changed sucessful\n");
					set_error(0,0,0);
					out->data[0]=0x01;
					out->data[1]=0x08;
					out->data[2]=0x00;
					out->data[3]=0x00;
					out->size=4;
					set_error(0,0,0);
					return 0;
				}
				set_error(62,0,0);
				return -1;
			}
		}
	}
	if (logical_files[file_unit][listenlf].open == 1) {
		strcpy((char*)fname,(char*)name);
		petscii2ascii(fname,strlen(fname));
		debug_msg ("*** Opening [%s] on channel $%02x\n", fname, listenlf);
		begin_measure();
		return 0;
	}
	return -1;
}

int closefile() {
	if (logical_files[file_unit][listenlf].open == 1) {
                 fs64_closefile_g (&logical_files[last_unit][listenlf]);
        }
        set_error(0,0,0);
	end_measure();
	return 1;

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
	  for (i = 9; i < (int)strlen (temp); i++)
	    if (temp[i] == ',')
	      break;
	  if (i >= (int)strlen (temp))
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


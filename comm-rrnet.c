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

#define MAX_CLIENTS	256

#define PADDING 1

/* Current talk & listen logical files */
int talklf[MAX_CLIENTS]={-1};
int listenlf[MAX_CLIENTS]={-1};

/* ** DRIVE and PARTITION resolution: */
/* Partition base directories for each drive */
uchar *partn_dirs[MAX_CLIENTS][256];

/* ** CBM DOS emulation stuff: */
/* DOS Statii */
uchar dos_status[MAX_CLIENTS][256];
/* length of DOS statii */
uchar dos_stat_len[MAX_CLIENTS];
/* position of char we actually talking */
uchar dos_stat_pos[MAX_CLIENTS];
/* DOS commands */
uchar dos_command[MAX_CLIENTS][256];
/* length of DOS command */
uchar dos_comm_len[MAX_CLIENTS]={0};

/* ** current directories & partitions: */
/* current partition on each drive */
int curr_par[MAX_CLIENTS];
/* current subdirectory (absolute) for each partition on each drive */
uchar *curr_dir[MAX_CLIENTS][256];

/* dir blocks for partitions */
int partn_dirtracks[MAX_CLIENTS][256];
int partn_dirsectors[MAX_CLIENTS][256];
int curr_dirtracks[MAX_CLIENTS][256];
int curr_dirsectors[MAX_CLIENTS][256];

/* Logical files */
fs64_file logical_files[MAX_CLIENTS][16];

/* Last accessed drive, for the purposes of error channels etc */
int last_client=0;

/* number of clients */
char client_ip[MAX_CLIENTS][256];
char client_mac[MAX_CLIENTS][256];
char client_name[MAX_CLIENTS][256];


/* file that points to our communications device */
//fs64_file file;

/* partition # that will be searched for programs, whatever this means, not used */
//int pathdir;

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

#define WAITFORACK		0x01
#define IDLE			0x00
#define TIMEOUT			10

int sendfd[MAX_CLIENTS];
int receivefd;
unsigned char in_buffer[256];
unsigned char out_buffer[256];
struct sockaddr_in receiver = { 0 };
struct sockaddr_in sender[MAX_CLIENTS] = { 0 };

char port[256]; 

int acknowledge[MAX_CLIENTS];
unsigned SA[MAX_CLIENTS];
int state[MAX_CLIENTS]={IEC_IDLE};
unsigned char myfilename[MAX_CLIENTS][1024];
int myfilenamesize[MAX_CLIENTS];

/* for measuring transfertimes */
clock_t starttime[MAX_CLIENTS];
clock_t endtime[MAX_CLIENTS];

struct timeb start[MAX_CLIENTS] = { 0 };
struct timeb end[MAX_CLIENTS] = { 0 };

long transferred_amount[MAX_CLIENTS]={0};

struct packet {
        unsigned char* data;
        unsigned char type;
        int size;
};
struct packet* out;

int curr_client;

int iec_commune(int unused) {
	int a;
        struct hostent *hp;

	initialize();
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
        receiver.sin_port=htons(atoi(port));
        receiver.sin_addr.s_addr=htonl(INADDR_ANY);
        receivefd = socket (AF_INET,SOCK_DGRAM, IPPROTO_UDP);

	//install sender sockets fro all clients
	for(a=0;a<=last_client;a++) {
        	myfilenamesize[a]=0;
	        acknowledge[a]=0;
		hp = gethostbyname(client_ip[a]);
		sender[a].sin_family = AF_INET;
		sender[a].sin_port = htons(atoi(port));
		sendfd[a] = socket (AF_INET,SOCK_DGRAM,IPPROTO_UDP);
		bcopy (hp->h_addr, &(sender[a].sin_addr.s_addr), hp->h_length);
		if (sendfd[a]<0) {
			fprintf(stderr,"Failed to bind socket for client %s.\n",client_ip[a]);
			perror("open_port");
			exit (EXIT_FAILURE);
			return 0;
		}
	}

        bind(receivefd, (struct sockaddr *) &receiver, sizeof(receiver));

	//bind input socket
	if (receivefd<0) {
		fprintf(stderr,"Failed to bind to port %d to listen for UDP packets.\n",atoi(port));
		perror("open_port");
		exit (EXIT_FAILURE);
		return 0;
	}
	
	for(a=0;a<=last_client;a++) {
		//printf("Adding static arp entry (IP=%s MAC=%s)\n",client_ip[a], client_mac[a]);
		if(!arp_set(client_ip[a],client_mac[a])) {
			fatal_error("Failed to make static arp entry\n"); 
		}
	}
	start_server();
	return 0;
}

void rrnet_quit() {
	int a;
	for(a=0;a<=last_client;a++) {
		//printf("Deleting arp-entry for %s\n",client_ip[a]);
		arp_del(client_ip[a]);
	}
}

void initialize() {
        out=(struct packet*)malloc(sizeof(struct packet));
        out->data=(unsigned char*)malloc(DATA_PAYLOAD_SEND);
        out->size=0;
	curr_client=1;
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
	
	printf ("Network started.\n");
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
				}
			}
			else {						//assemble other packet (code, data)
				p->data[0]=buffer[1];
				p->size=1;
			}
			//dispatch();
			//set curr_client on base of ip first, then process
			//all stuff and variables for otehr clients should stay untouched
			//as each packet = one cycle for one client
			process_packet(p);				//let the respective server process process the packet
		}
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
		acknowledge[curr_client]=1;	//set flag if we got an ack
	}
        if(p->type==PACKET_COMMAND && p->size==1) {			//got a command? 
		change_state(p->data[0]);				//then change state 
		acknowledge[curr_client]=0;						//ignore pending acknowledges
		send_acknowledge();					//acknowledge command packet
	}
        if(p->type==PACKET_DATA) {
		transferred_amount[curr_client]+=p->size;
		send_acknowledge();					//acknowledge data packet
	}
	/* now enter state machine and do the next step */
        switch(state[curr_client]) {
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
	if(p->type==PACKET_ACKNOWLEDGE && p->size==1) return 1;
	return 0;
}

void change_state(unsigned char command) {
        /* Change TALK/LISTEN/UNLISTEN/UNTALK state based on character received 
	   as for talk and listen, mask out devicenumber */

        if((command&0xf0)==IEC_LISTEN) {
                state[curr_client]=IEC_LISTEN;
                return;
        }
        else if((command&0xf0)==IEC_TALK) {
                state[curr_client]=IEC_TALK;
                return;
        }
        else if(command==IEC_UNLISTEN) {
                state[curr_client]=IEC_UNLISTEN;
                return;
        }
        else if(command==IEC_UNTALK) {
                state[curr_client]=IEC_UNTALK;
                return;
        }
        else if(command==IEC_IDLE) {
                state[curr_client]=IEC_IDLE;
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
                        SA[curr_client]=command&0xf0;                        			//set SA and lf
                        listenlf[curr_client]=command&0x0f;

                        if(SA[curr_client]==IEC_CLOSE) {						//close actual file
				closefile();
                        }
                        if(SA[curr_client]==IEC_SAVE && listenlf[curr_client]!=0x0f) {				//prepare for saving
                                myfilename[curr_client][myfilenamesize[curr_client]]=0;				
                                if(openfile(myfilename[curr_client], MODE_WRITE)<0) {		//try to get write access on requested file
                                        send_error(ERROR_FILE_EXISTS);			//failed
					return;
                                }
                        }
                        if(SA[curr_client]==IEC_OPEN || listenlf[curr_client]==0xf) { 				//prepare filename
                                myfilenamesize[curr_client]=0;
                                myfilename[curr_client][myfilenamesize[curr_client]]=0;
                        }
                }
                else {									//no idea what we got
                        if((command&0xf0)!=IEC_LISTEN) {
                                state[curr_client]=IEC_IDLE;                       			//so drop current state
                                process_packet(p);                      		//and reinterpret command packet
                        }
                }
		return;
        }
        if(p->type==PACKET_DATA) {							//we receive data
                if(SA[curr_client]==IEC_OPEN || (listenlf[curr_client]==0x0f && SA[curr_client]==IEC_LOAD)) {			//what we get is filename/diskcommand	
                        for(i=0;i<p->size;i++) {
                                if(myfilenamesize[curr_client]<1024) myfilename[curr_client][myfilenamesize[curr_client]]=p->data[i];
                                myfilenamesize[curr_client]++;
                        }
                }
                if(SA[curr_client]==IEC_SAVE && listenlf[curr_client]!=0x0f) {					//what we get is data to be saved
                        for(i=0;i<p->size;i++) {
				fs64_writechar(&logical_files[curr_client][listenlf[curr_client]], p->data[i]);
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
                        SA[curr_client]=command&0xf0;
                        talklf[curr_client]=command&0x0f;
                        acknowledge[curr_client]=1;                          			//fake acknowledge, so that we get can immediatedly send first packet (passing if statement with acknowlegde!=0)
                        if(talklf[curr_client]!=0x0f) {						//file or command?				
				myfilename[curr_client][myfilenamesize[curr_client]]=0;				//finish filename
                                if(openfile(myfilename[curr_client],MODE_READ)<0) {			//try to open file
                        		send_error(ERROR_FILE_NOT_FOUND);		//failed
                                        set_error(62,0,0);				//signal it
					acknowledge[curr_client]=0;					//clear ack flag, to recognize new acks
					return;
                                }
                        }
                }
        }
        if(SA[curr_client]==IEC_LOAD) {								//c64 wants to load?
                if(acknowledge[curr_client]!=0) {							//already got our okay for next package?
                        acknowledge[curr_client]=0;                  				//allow next acknowledge again
                        if(talklf[curr_client]!=0x0f) {						//file? yes, send file data
				while(out->size<DATA_PAYLOAD_SEND) {			//still bytes free in packet?
					if(fs64_readchar(&logical_files[curr_client][talklf[curr_client]],&temp)) break;	//oops, reached file end
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
					temp=dos_status[curr_client][dos_stat_pos[curr_client]];
		                        out->data[out->size]=temp;			//add byte
               			        out->size++;					 
		                        if(temp==0x0d) {				//end reached?
						//set_error(0,0,0);			//reset status and send last packet
						break;
					}
					dos_stat_pos[curr_client]++;			//count consumed byte
				}
                        	send_data(out);						//now send packet
                        }
                }
        }
        return;
}

void iec_unlisten(struct packet* p) {
        myfilename[curr_client][myfilenamesize[curr_client]]=0;							//finalize filename/command
        if(listenlf[curr_client]==0x0f) {
		strcpy((char*)dos_command[curr_client],(char*)myfilename[curr_client]);
		dos_comm_len[curr_client]=myfilenamesize[curr_client];
		do_dos_command();							//try to execute command if we should
	}
        state[curr_client]=IEC_IDLE;
        return;
}

void iec_untalk(struct packet* p) {							//we just need to shut up, so let's IDLE
	if(listenlf[curr_client]==0x0f) {
		if(dos_status[curr_client][dos_stat_pos[curr_client]]==0x0d) 
			set_error(0,0,0);						//reset status and send last packet
	}
        state[curr_client]=IEC_IDLE;
        return;
}


void send_error(unsigned char err) {							//assemble error packet
        out->size=1;
        out->data[0]=err;
	out->type=PACKET_ERROR;
        send_packet(out);
	out->size=0;
        return;
}

void send_acknowledge() {								//assemble acknowledge packet
        out->size=1;
        out->data[0]=0x00;
	out->type=PACKET_ACKNOWLEDGE;
        send_packet(out);
	out->size=0;
        return;
}

void send_data(struct packet* p) {							//send data packet
	out->type=PACKET_DATA;
        send_packet(out);
	out->size=0;
	return;
}

void send_packet(struct packet* p) {
	unsigned char reply[p->size+2];
	int size;
	int i=0;
	switch(p->type) {
		case PACKET_DATA:							//data packet, so add size too (code, size, data)
			reply[0]=PACKET_DATA;
			reply[1]=p->size;
			while(i<p->size) { reply[2+i]=p->data[i]; i++; }
			size=p->size+2;
			transferred_amount[curr_client]+=p->size;
		break;
		default:								//other packets, just do code, data
			reply[0]=p->type;
			reply[1]=p->data[0];
			size=p->size+1;
		break;
	}
		
	sendto(sendfd[curr_client], reply,size, 0, (struct sockaddr *) &sender[curr_client], sizeof(sender[curr_client]));
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

	printf("\n\n");
#endif
}

void begin_measure() {									//set measure start point
	transferred_amount[curr_client]=0;
	ftime(&start[curr_client]);
	return;
}

void end_measure() {									//end of measure, calculate time needed
	float time;
	float rate;
	long timediff;
	ftime(&end[curr_client]);
	timediff= (end[curr_client].time*1000+end[curr_client].millitm)-(start[curr_client].time*1000+start[curr_client].millitm);
					
//	timediff=(end.time-start.time)*1000+(end.millitm-start.millitm);
	time=((float)timediff/(float)1000);
	rate=(float)transferred_amount[curr_client]/time;
	debug_msg("%ld bytes transferred in %.2fs => %.3f kb/s\n", transferred_amount[curr_client],time,rate/1024);
	
//	debug_msg("Time needed:%f sec\n", time);
//	debug_msg("Transferrate:%8.2f bps (%8.3fKB/sec)\n", rate,rate/1024);
	return;	
}
	
int openfile(unsigned char* name, int mode) {						//try to open a file/dir/whatever
	int i;
	unsigned char fname[strlen(name)];
	if (logical_files[curr_client][listenlf[curr_client]].open == 1) {
		debug_msg ("*** Closing previous opened file first");
		closefile();
	}
	if (logical_files[curr_client][listenlf[curr_client]].open != 1) {				//open already?
		if (fs64_openfile_g (curr_dir[curr_client][curr_par[curr_client]], name, &logical_files[curr_client][listenlf[curr_client]])) {
			/* open failed, got to react on that */
			logical_files[curr_client][listenlf[curr_client]].open=0;
			if(mode==MODE_WRITE) {
				/* if the file doesn't exist yet we end up here and try to create it now */
				/* file not found */
				/* postpend ",W" to the myfilename and try again */
				strcat ((char*)name, ",W");
				if (fs64_openfile_g (curr_dir[curr_client][curr_par[curr_client]], name, &logical_files[curr_client][listenlf[curr_client]])) {
					/* cannot open file for some reason (missing permissions or such) */
					/* simply abort */
					return -1;
				}
			}
			if(mode==MODE_READ) {
				//maybe we just tried to load a dir?
				//if so, let's change to that dir!
		
				//printf("namesize:%d",strlen(name));
				dos_comm_len[curr_client]=3+strlen(name);
				dos_command[curr_client][0]='C';
				dos_command[curr_client][1]='D';
				dos_command[curr_client][2]=':';
		
				for(i=0;i<(int)strlen(name);i++) {
					dos_command[curr_client][3+i]=name[i];
				}
				if(do_dos_command()==0) {
					//debug_msg("changed sucessful\n");
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
	if (logical_files[curr_client][listenlf[curr_client]].open == 1) {
		strcpy((char*)fname,(char*)name);
		petscii2ascii(fname,strlen(fname));
		debug_msg ("*** Opening [%s] on channel $%02x\n", fname, listenlf[curr_client]);
		begin_measure();
		return 0;
	}
	return -1;
}

int closefile() {
	if (logical_files[curr_client][listenlf[curr_client]].open == 1) {
                 fs64_closefile_g (&logical_files[curr_client][listenlf[curr_client]]);
        }
        set_error(0,0,0);
	end_measure();
	return 1;

}

/* These configuration reading routines really should be in another module,
   but that would entail global variables which i want to avoid.
 */

int read_config (char *file) {
	/* Read in the 64netrc file */

	last_client=-1;
	curr_client=-1;
	FILE *cf = 0;
	uchar temp[256];

	if ((cf = fopen (file, "r")) == NULL) fatal_error ("Cannot read configuration file.");

	while (!feof (cf)) {
		fgets ((char*)temp, 256, cf);
		if ((temp[0] == '#') || (temp[0] == 0) || (temp[0] == '\n') || (temp[0] == '\r')) {
			/* its a comment or blank line */
		}
		else {
		/* its a real line of stuff */
			if (!strncmp ("port ", (char*)temp, 4)) {
				/* its a port line */
#ifdef AMIGA
				printf ("INIT: Using internal parallel port\n");
				strcpy ((char*)port, (char*)&temp[5]);
#else
				strcpy ((char*)port, (char*)&temp[5]);
				/* chop CR/LF */
				chomp(port);
				printf ("INIT: Communication port set to %s\n", port);
#endif
			}
			/* deprecated, will be ignored anyway, hmm */
			else if (!strncmp ("path ", (char*)temp, 4)) {
				/* path partition */
				//pathdir = atol (&temp[5]);
				//printf ("64NET/2 will look in partition %d for programmes\n", pathdir);
			}
			//found a client block, read in config
			//XXX
			//also read out identifier so that we can separate clients (store in client_name[])
			else if (!strncmp ("[client", (char*)temp, 7)) {
				/* its a device line */
				read_client (cf);
			}
			else if (!strncmp ("ledfile", (char*)temp, 6)) {
				/* its a device line */
				if (temp[strlen (temp) - 1] < ' ') temp[strlen (temp) - 1] = 0;
				if (temp[7] < ' ') sprintf ((char*)&temp[7], " %s/.64net2.leds", getenv ("HOME"));
				client_init (&temp[8]);
				client_activity (0);
				client_error (0);
			}
			else if (!strncmp ("debug mode", (char*)temp, 9)) {
				/* debug mode */
				debug_mode = 1;
				printf ("INIT: Debug mode messages will be displayed.\n");
			}
			else {
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
	//better check if a client was configured
	if (last_client<0) {
		/* no clients defined */
		fatal_error ("Configuration file contains no client sections.");
	}
	if (port[0] == 0) {
		/* no port lines */
		fatal_error ("Configuration file contains no port line.");
	}
	return (0);
}

int is_empty(char* temp) {
	unsigned int b=0;
	if(strlen(temp)==0) return 1;
	while(b<strlen(temp)) {
		switch(temp[b]) {
			case 0x0a:
			case 0x0d:
			case 0x20:
			case '\t':
			case 0xa0:
			break;
			case '#':
				return 1;
			break;
			default:
				return 0;
			break;
		}
		b++;
	}
	return 1;
}

void chomp(char* temp) {
	unsigned int b=0;
	int a=0;
	while(b<strlen(temp) && (temp[b]==' ' || temp[b]=='\t')) b++;
	for(b=b;b<strlen(temp);b++) {
		if(temp[b]=='\n' || temp[b]=='\r') temp[a]=0x00;
		else temp[a]=temp[b];
		a++;
	}
	//while((temp[strlen(temp)-1]=='\n')||(temp[strlen(temp)-1]=='\r')) temp[strlen(temp)-1]=0;
	return;	
}

int read_client (FILE * cf) {
	/* read a device section */
	int i, pn;
	char temp[256];

	last_client++;
	curr_client++;
	
	while (!feof (cf)) {
		fgets (temp, 256, cf);
		if(is_empty(temp) || feof(cf)) {
			/* comment line */
		}
		else {
			/* real line */
			/* Acceptables are:
			   IP <ip_num>                  - Sets the device # for this dev
			   MAC <mac_num>                - End definition of device
			   PARTITION <part_num>,<path>  - Define a partition
			 */
			if (!strncmp ("partition", temp, 9)) {
				/* partition for drive */
				/* find first comma */
				for (i = 9; i < (int)strlen (temp); i++) {
					if (temp[i] == ',') break;
				}
				if (i >= (int)strlen (temp)) fatal_error ("Bad partition line (no commas).");
				pn = atol (&temp[9]);
				if ((pn < 1) || (pn > 255)) {
					/* bad partition # */
					fatal_error ("Bad partition number. Must be between 1 and 255.");
				}
				else {
					/* okey */
					partn_dirs[curr_client][pn] = (uchar *) malloc (strlen (&temp[i + 1]) + PADDING);
					if (!partn_dirs[curr_client][pn]) /* couldnt malloc */ fatal_error ("Cannot allocate memory.");
					else {
						/* strip newline */
						uchar partition[8], path[1024];
						partition[0] = 'n';
						temp[strlen (temp) - 1] = 0;
						strcpy ((char*)partn_dirs[curr_client][pn], (char*)&temp[i + 1]);
						printf ("INIT client #%d:  %s added as partition %d\n", curr_client, partn_dirs[curr_client][pn], pn);
						/* parse for .DHD sub-directories */
						partn_dirtracks[curr_client][pn] = -1;
						partn_dirsectors[curr_client][pn] = -1;
						curr_dirtracks[curr_client][pn] = -1;
						curr_dirsectors[curr_client][pn] = -1;
						strcpy ((char*)path, (char*)partn_dirs[curr_client][pn]);
						ascii2petscii(path,strlen(path));
						if (path[0] != '@') {
							if (fs64_resolve_partition (partition, path, &partn_dirtracks[curr_client][pn], &partn_dirsectors[curr_client][pn])) {
								/* failed */
								printf ("Invalid partition path for %d\n", pn);
								fatal_error ("Invalid partition table\n");
							}
							curr_dirtracks[curr_client][pn] = partn_dirtracks[curr_client][pn];
							curr_dirsectors[curr_client][pn] = partn_dirsectors[curr_client][pn];
							//debug_msg ("  (%s T%d S%d)\n", path,partn_dirtracks[curr_client][pn], partn_dirsectors[curr_client][pn]);
							free (partn_dirs[curr_client][pn]);
							if (!(partn_dirs[curr_client][pn] = (uchar *) malloc (strlen (path) + 1 + PADDING))) {
								/* couldnt malloc */
								fatal_error ("Cannot allocate memory.");
							}
							strcpy ((char*)partn_dirs[curr_client][pn], (char*)path);
						}
					}
				}
			}
			else if (!strncmp ("ip", (char*)temp,2)) {
				strcpy ((char*)client_ip[curr_client], (char*)&temp[3]);
				chomp(client_ip[curr_client]);
				printf ("INIT client #%d: client IP set to %s\n", curr_client, client_ip[curr_client]);
			}
			else if (!strncmp ("mac", (char*)temp,3)) {
				strcpy ((char*)client_mac[curr_client], (char*)&temp[4]);
				chomp(client_mac[curr_client]);
				printf ("INIT client #%d: client MAC set to %s\n", curr_client, client_mac[curr_client]);
			}
			else if (!strncmp ("[client", (char*)temp, 7)) {
				/* its a device line */
				read_client (cf);
			}
			else {
				printf("%x\n",temp[0]);
				/* Unknown line type */
				fprintf(stderr,"Unknown configuration directive:\n%s\n",temp);
				fatal_error ("Bad configuration file.  Unknown line types.");
			}
		}				/* end of not a comment */
	}				/* end of while !feof */
	return (0);
}

int set_drive_status (uchar *string, int len) {
	/* set the drive message */
	if(curr_client>=0 && curr_client<=last_client) {
		/* copy string */
		memcpy (dos_status[curr_client], string, len+1);
	
		/* and set length */
		dos_stat_len[curr_client] = len;
		//Fixed: need to set pos to 0!! TB
		dos_stat_pos[curr_client] = 0;
		return (0);
	}
	else return -1;
}


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

int socket_out;
int socket_in;
int packet_number;
unsigned char in_buffer[256];
unsigned char out_buffer[256];
unsigned char buffer[1024];
int bsize;
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
        unsigned char type;
        int size;
};

unsigned char* status;

struct packet* out;
void start_server();
void begin_measure();
void end_measure();
int openfile(unsigned char*, int);
int closefile();
void send_acknowledge(unsigned char);
void send_error(unsigned char);
void change_state(unsigned char);
void initialize();
void iec_listen(struct packet*);
void iec_unlisten(struct packet*);
void iec_talk(struct packet*);
void iec_untalk(struct packet*);
void process_packet(struct packet*);
void send_packet(struct packet*);

int iec_commune(int unused) {
        struct  hostent *hp, *gethostbyname();
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
	
	initialize();
	start_server();
	return 0;
}

void initialize() {
        out=malloc(sizeof(struct packet));
        out->data=malloc(DATA_PAYLOAD_SEND);
        myfilenamesize=0;
        out->size=0;
        acknowledge=0;
        return;
}

/* this has to be moved elsewhere into a kind of dispatcher which dispatches the appropriate
   packets to the respective server processes */

void start_server() {
	int i;
	struct packet* p;
        p=malloc(sizeof(struct packet));
        p->data=malloc(1024);
	
	while(1) {					
		bsize=recv(socket_in, buffer, sizeof(buffer), 0);
		if(bsize>1) {
			p->type=buffer[0];
			if(p->type==PACKET_DATA) {			//assemble a data packet (0x44, len, data)
				if(bsize>1) {
					for(i=0;i<bsize-2;i++) {
						p->data[i]=buffer[i+2];
					}
					p->size=buffer[i+1];
				}
				transferred_amount+=p->size;
			}
			else {						//assemble otehr packet (code, data)
				p->data[0]=buffer[1];
				p->size=1;
			}
			process_packet(p);				//let the respective server process process the packet
		}
	}
}

void process_packet(struct packet* p) {
	#ifdef DEBUG_COMM
	int i;
	printf ("received packetsize: $%X\n",p->size);
	for(i=0;i<p->size;i++) {
		if((i&0x7)==0) printf("\n");
		if(p->data[i]>15) printf ("$%X ",p->data[i]);
		else printf ("$0%X ",p->data[i]);
	}
	printf("\n");
	#endif

	
        if(p->type==PACKET_ACKNOWLEDGE && p->size==1) acknowledge=1;	//set flag if we got an ack
        if(p->type==PACKET_COMMAND && p->size==1) {			//got a command? 
		change_state(p->data[0]);				//then change state 
		acknowledge=0;						//and kick pending acknowledges
		send_acknowledge(0x00);					//acknowledge command packet
	}
        if(p->type==PACKET_DATA) {
		send_acknowledge(0x00);					//acknowledge data packet
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
                        acknowledge=1;                          //allow first packet to be recognized
                        if(talklf!=0x0f) {
                                out->size=0;
				myfilename[myfilenamesize]=0;
                                if(openfile(myfilename,MODE_READ)<0) {
                        		send_error(ERROR_FILE_NOT_FOUND);
                                        set_error(62,0,0);
					acknowledge=0;
					return;
                                }
                        }
                }
        }
        if(SA==IEC_LOAD) {
                if(acknowledge!=0) {
			out->type=PACKET_DATA;
			out->size=0;
                        if(talklf!=0x0f) {
				while(out->size<DATA_PAYLOAD_SEND) {
					if(fs64_readchar(&logical_files[file_unit][talklf],&temp)) break;
                                	out->data[out->size]=temp;
                                       	out->size++;
                                }
				if(out->size==0) {
					send_error(ERROR_EOI);
					set_error(0,0,0);
				}
				else send_packet(out);
                        }
                        else {
                                //copy status and if requested more we will send 0-sized packets
                                while(out->size<DATA_PAYLOAD_SEND) {
					temp=dos_status[last_unit][dos_stat_pos[last_unit]];
		                        out->data[out->size]=temp;
		                        if(temp!=0x0d) {
               			                out->size++;
		                                dos_stat_pos[last_unit]++;
                        		}
		                        else {
               			                if(out->size>0) out->size++;
						break;
					}
				}
                        	send_packet(out);
                        }
                        acknowledge=0;                  //lock out until we get an acknowledge
                }
        }
        return;
}

void iec_unlisten(struct packet* p) {
        myfilename[myfilenamesize]=0;
        if(listenlf==0x0f) {
                do_dos_command();
        }
        state=IEC_IDLE;
        return;
}

void iec_untalk(struct packet* p) {
        state=IEC_IDLE;
        return;
}


void send_error(unsigned char err) {
	send_acknowledge(err);
}

void send_acknowledge(unsigned char err) {
        out->size=1;
        out->data[0]=err;
	if(err==0x00) out->type=PACKET_ACKNOWLEDGE;
	else out->type=PACKET_ERROR;
        send_packet(out);
        return;
}

void begin_measure() {
	transferred_amount=0;
	ftime(&start);
	return;
}

void send_packet(struct packet* p) {
	unsigned char reply[p->size+2];
	int size;
	int i=0;
	switch(p->type) {
		case PACKET_DATA:
			reply[0]=PACKET_DATA;
			reply[1]=p->size;
			while(i<p->size) { reply[2+i]=p->data[i]; i++; }
			size=p->size+2;
			transferred_amount+=p->size;
		break;
		default:
			reply[0]=p->type;
			reply[1]=p->data[0];
			size=p->size+1;
		break;
	}
		
	sendto(socket_out, reply,size, 0, (struct sockaddr *) &sender, sizeof(sender));
	#ifdef DEBUG_COMM
	printf("$%X bytes of data sent.\n", size);
	for(i=0;i<size;i++) {
		if(reply[i]<16) printf("$0%X ",reply[i]);
		else printf("$%X ",reply[i]);
		if((i+1)%8==0) printf("\n");
	}
	if((i)%8!=0) printf("\n");
	#endif
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
	
int openfile(unsigned char* name, int mode) {
	int i;
	if(mode==MODE_WRITE) {
		/* if the file doesn't exist yet we end up here and try to create it now */
		if (logical_files[file_unit][listenlf].open != 1) {
			#ifdef DEBUG_COMM
			printf("Trying to open file...\n");
			#endif
			if (fs64_openfile_g (curr_dir[last_unit][curr_par[last_unit]], name, &logical_files[file_unit][listenlf])) {
				#ifdef DEBUG_COMM
				printf("Can't find file, creating it...\n");
				#endif
				/* file not found */
				/* postpend ",W" to the myfilename and try again */
				strcat ((char*)name, ",W");
				#ifdef DEBUG_COMM
				printf("Filename now:%s\n",name);
				#endif
				if (fs64_openfile_g (curr_dir[last_unit][curr_par[last_unit]], name, &logical_files[file_unit][listenlf])) {
					send_acknowledge(ERROR_FILE_EXISTS); //signal file exists error
					/* cannot open file for some reason (missing permissions or such) */
					/* simply abort */
					return -1;
				}
			}
		}
			
		//recheck again after first try, that is why we have an if again and not an else!
		//we managed to open a file and have write permissions so we can start saving into it now
		if (logical_files[file_unit][listenlf].open == 1) {
			begin_measure();
			set_error(0,0,0);
			return 0;
		}
		return -1;
	}
	if(mode==MODE_READ) {
		//we can now clear the myfilename, hopefully
		//not really needed here, as it will be done on close
		//if(mymyfilenamelen>0) { free(mymyfilename); mymyfilenamelen=0; }
		//do we have an open file?
		if(fs64_openfile_g (curr_dir[last_unit][curr_par[last_unit]], name, &logical_files[file_unit][listenlf])!=0) {
			logical_files[file_unit][listenlf].open=0;
			/* File not found, now signal it with sending a dummy 
			 * char followed by a timeout */
			//maybe we just tried to load a dir?
			//if so, let's change to that dir!
			
			printf("namesize:%d",strlen(name));
			dos_comm_len[last_unit]=3+strlen(name);
			dos_command[last_unit][0]='C';
			dos_command[last_unit][1]='D';
			dos_command[last_unit][2]=':';
			
			for(i=0;i<strlen(name);i++) {
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
				return 1;
			}
			else {
				set_error(62,0,0);
			}
			#ifdef DEBUG_COMM
			printf("File not found, sending empty data packet...\n");
			#endif
			return -1;
		}
		else {
			debug_msg ("*** Opening [%s] on channel $%02x\n", name, listenlf);
			set_error(0,0,0);
			begin_measure();
			return 0;
		}
	}	
	return -1;
}

int closefile() {
	if (logical_files[file_unit][listenlf].open == 1) {
                 fs64_closefile_g (&logical_files[last_unit][listenlf]);
                 set_error(0,0,0);
        }
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


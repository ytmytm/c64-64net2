/*
   Commune.H for 64NET/2 (C)Copyright Paul Gardner-Stephen 1995, 2004
   All rights reserved.
   
   Commune.H - Constants and structures for communicating to the C64/C65/C128
*/

/* port addresses */
extern int portout,portin;
extern int max_tries;
//extern char port[256];

/* function prototypes */
int commune(void);
int read_config (char *file);
int read_device (FILE * cf);
int read_client (FILE * cf);
int set_drive_status (uchar *string, int len);
int which_unit (int);

int iec_commune(int a);
int ascii2petscii(uchar*,int);
int petscii2ascii(uchar*,int);
void rrnet_quit(void);

void start_server();
void begin_measure();
void end_measure();
int openfile(unsigned char*, int);
int closefile();
void send_acknowledge();
void send_error(unsigned char);
void send_data(struct packet* p);
void change_state(unsigned char);
void initialize();
void iec_listen(struct packet*);
void iec_unlisten(struct packet*);
void iec_talk(struct packet*);
void iec_untalk(struct packet*);
void process_packet(struct packet*);
void send_packet(struct packet*);
int pkt_is_acknowledge(struct packet*);
void rrnet_quit(void);
void chomp(char*);

extern int listenlf[MAX_CLIENTS];
extern int talklf[MAX_CLIENTS];
extern int last_client;
extern int curr_client;
//extern int devnum;

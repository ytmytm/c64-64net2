/*
   Commune.H for 64NET/2 (C)Copyright Paul Gardner-Stephen 1995,
   All rights reserved.
   
   Commune.H - Constants and structures for communicating to the C64/C65/C128
*/

/* command enumeration */
//#define OPEN		'O'		//
//#define CLOSE		'C'		//
//#define SETTALKER	'D'		//
//#define TALK		'R'		//
//#define SETLISTENER	'A'		//
//#define LISTEN	'W'		//
//#define BOOT		'B'
//#define LOAD		'L'
//#define SAVE		'S'
//#define DEVCHECK	'?'

/* new, sane command enumeration, only few make sense, though */
#define BOOT		'B'
#define LOAD		'L'
#define SAVE		'S'
#define DEVCHECK	'?'
#define LISTEN		'W'
#define SECOND		'A'
#define CIOUT		'G'
#define TALK		'R'
#define TKSA		'D'
#define ACPTR		'H'
#define UNLISTEN	'I'
#define UNTALK		'J'
#define OPENDISABLE	'K'
/* `File System Accelerator' commands */
#define FSACCEL		'F'

#ifndef NULL
#define NULL 0
#endif

/* port addresses */
extern int outport,inport;

/* function prototypes */
int commune(void);
int read_config (char *file);
int read_device (FILE * cf);
int c64peek (long location);
int c64poke (long location,int val);
int c64print(char *text);
int c64jsr(int addr,int a,int x, int y);
int set_drive_status (unsigned char *string, int len);
int which_unit (int);

extern int talklf, lastlf, devnum;

/*
   Commune.H for 64NET/2 (C)Copyright Paul Gardner-Stephen 1995,
   All rights reserved.
   
   Commune.H - Constants and structures for communicating to the C64/C65/C128
*/

#include "fs.h"

/* debug messages flag */
int debug_mode=0;

/* command escape characters */

#define COMMCHAR 0x25
#define COMMCHAR2 0x24

/* attention commands recognised */

#define NONE 0
#define OPEN 0xf0
#define CLOSE 0xe0
#define COMMUNE 0x60

/* nitty gritty communications variables */

/* attention flag */
int attn=0;
/* current command sent under attention */
int attncmd=NONE;
/* device attention is directed at */
int attndev=0;
/* Current talk logical file */
int talklf=-1;
int talkdev=0;

/* Temprary variables used to store filenames and things for
   fs_openfile_g etc 
   */
int fnlen;
unsigned char filename[256];

/* device to communicate over */
unsigned char port[256]={0};

/* structures for redirected devices */

/* ** DRIVE and PARTITION resolution: */
/* C64 device numbers corresponding to our internal ones */
int devices[MAX_NET_DEVS];
/* Partition base directories for each drive */
unsigned char *partn_dirs[MAX_NET_DEVS][256];

/* ** current directories & partitions: */
/* current partition on each drive */
int curr_par[MAX_NET_DEVS];
/* current subdirectory (absolute) for each partition on each
   drive */
unsigned char *curr_dir[MAX_NET_DEVS][256];

/* ** CBM DOS emulation stuff: */
/* DOS Statii */
unsigned char dos_status[MAX_NET_DEVS][256];
/* length of DOS statii */
unsigned char dos_stat_len[MAX_NET_DEVS];
/* DOS commands */
unsigned char dos_command[MAX_NET_DEVS][256];
/* length of DOS command */
unsigned char dos_comm_len[MAX_NET_DEVS];

/* file readable flags (set when first character is read) */
int lf_flags[16];

/* Logical files */
fs64_file logical_files[MAX_NET_DEVS][16];

/* file that points to our communications device */
FILE *f;
fs64_file file;

/* Last accessed drive, for the purposes of error channels etc */
int last_drive=-1; /* none */
int last_unit=-1; /* none */

/* function prototypes - move to commune.h */
int iecin();
int actonattncmd();
int received();
int fatal_error(unsigned char *foo);
int read_device(FILE *foo);
int which_unit(int dev);

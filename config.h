/* 

    Main include file, with configuration and options
    also for generating precompiled headers
*/


#define UNIX
#define LINUX
/*
#define USE_LINUX_KERNEL_MODULE
#define BSD
#define AMIGA

#define DEBUG
*/

/* 1 - on, 0 - off */
#define FISHLOAD 0
#define FISHSAVE 0

/* 1 - network off, 0 - network on */
#define NONET 1

#define DEF_INPORT	0x3bd
#define DEF_OUTPORT	0x3bc

#define VER_MAJ 0
#define VER_MIN 8
#define VERSIONSTRING "V%02d.%02d ALPHA"

/* the longest that a filename & path may be in characters.
   This makes a significant impact on the amount of memory 64NET/2 requires
   at runtime. 78 should be more than adequate under MS-DOS, and 256 should
   be reasonable under UNIX & AMIGA-DOS, although 1024 would be preferable
   (memory permitting)
*/
#define MAX_FS_LEN 256

/* maximum number of 64net devices */
/* anything over 32 is useless, as the IEC bus only handles 32 devices! */
/* note also parallel versions (typically) only support one device, but 
   can possibly handle 255 devices (if such support were implemented!) */
#define MAX_NET_DEVS 1

/***********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <dirent.h>
#include <netdb.h>
#include <sys/stat.h>
#include <sys/socket.h>

#ifdef AMIGA
#include <dos.h>
#endif

#include "debug.h"
#ifdef DEBUG
#include "resman.h"
#endif

/* save lots of typing */
#define uchar unsigned char
/* #define uchar char */

#undef strlen
#define strlen(x) strlen((char*)x)
#undef atol
#define atol(x) atol((char*)x)
#undef atof
#define atof(x) atof((char*)x)
#undef atoi
#define atoi(x) atoi((char*)x)

#undef fopen
#define fopen(x,y) fopen((char*)x,y)

#ifndef NULL
#define NULL 0
#endif


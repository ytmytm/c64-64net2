/* 

    Main include file, with configuration and options
    also for generating precompiled headers
*/


/* target architecture */

#ifndef __FreeBSD__
#define LINUX
//#define BSD
//#define SOLARIS
//#define BEOS
//#define WINDOWS
//#define AMIGA
#else
#define UNIX
#endif

/* do not define this - debug stuff is buggy (it's not a joke) */
/* #define DEBUG */

/* resource debugging (file stuff) */
/* #define DEBUG_RES */

/* uncomment this if you want to use serial driver instead of parallel one */
// #define USE_SERIAL_DRIVER
/* if you uncommented line above, then you must define speed, 9600 is OK for C64 */
// #define BAUDRATE B9600

/* LINUX target features, don't use it */
/* #define USE_LINUX_KERNEL_MODULE */

/* default options */
/* 1 - on, 0 - off */
#define FISHLOAD 0
#define FISHSAVE 0

/* 1 - INET network off, 0 - INET network on */
#define NONET 0

/* default devices */
#define DEF_INPORT	0x3bd
#define DEF_OUTPORT	0x3bc
#define COMM_DEVICE	"/dev/ttyS0"

#define VER_MAJ 0
#define VER_MIN 9
#define VERSIONSTRING "V%02d.%02d ALPHA"

#ifdef LINUX
#define UNIX
#endif
#ifdef BSD
#define UNIX
#endif
#ifdef SOLARIS
#define UNIX
#endif
#ifdef BEOS
#define UNIX
#endif

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
#include <sys/stat.h>

#ifdef UNIX
#define HAS_FTIME
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#ifdef SOLARIS
#define statfs statvfs
#endif

#ifdef BEOS
#include <OS.h>
#define statfs statvfs
#define socklen_t int
#define usleep(x) snooze(x)
#endif

#ifdef WINDOWS
#define HAS_FTIME
#include <winsock.h>
#define socklen_t int
#define usleep(x) sleep(x/1000)
#endif

#ifdef AMIGA
#include <dos.h>
#define usleep(x) Delay(x)
#endif

/* save lots of typing */
#define uchar unsigned char

/* hides lots of warnings */
#undef strlen
#define strlen(x) strlen((char*)x)
#undef atol
#define atol(x) atol((char*)x)
#undef atof
#define atof(x) atof((char*)x)
#undef atoi
#define atoi(x) atoi((char*)x)

#ifndef NULL
#define NULL 0
#endif

#include "debug.h"
#ifdef DEBUG_RES
#include "resman.h"
#else
#ifdef WINDOWS
#undef opendir
#define opendir winhack_opendir
#endif
#endif


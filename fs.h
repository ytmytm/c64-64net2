/* 
    64net-bsd include file - 64NET virtual filesystem constants and
    definitions 
    (C)opyright Paul Gardner-Stephen 1995,1996
    Modified 6 July 1996 (and later)
*/

/* 64NET filesystem structure */
struct fs64_filesystemstructure {
  int media;          /* type of filesystem */
  int arctype;        /* filesystem variant */
  FILE *fsfile;       /* file which hosts file system */
  uchar fspath[1024];  /* full path & name of filesystem base */
  int dirtrack;       /* track where directory starts */
  int dirsector;     /* sector where directory starts */
};
#define fs64_filesystem struct fs64_filesystemstructure

/* 64NET directory search structure */
struct fs64_direntrystructure {
   uchar fs64name[17];     /* C64 name of file, 0xa0 padded (must be 17 not 16!) */ 
   uchar realname[MAX_FS_LEN];/* full pathname and filename of real file/fs object */
   uchar path[MAX_FS_LEN]; /* pathname from original fs64_findfirst */
   uchar glob[17][32]; /* glob mask for use in fs64_findfirst_g */
   int active; /* is this structure active, or spent, 1= active,0= spent */
   int invisible;  /* is this file "invisible", ie ignore if in a dir search */
   long blocks;
   long realsize;
   fs64_filesystem filesys;
   DIR *dir;
   int filetype;
   int track;
   int sector;
   long binbase;
   int first_track;
   int first_sector;
   uchar fs[1024];  /* file system source for such types */
   int intcount; /* internal directory entry in sector for such types,
		    or counter for "synthesised" directories, like the 
		    internet file system ones */
   int fileposs; /* position in stream based files for searches */
   int dirtype;  /* type of directory being searched (used by media_net 
		    searches */
} ;
#define fs64_direntry struct fs64_direntrystructure

/* 64NET file structure - fs64_openfile returns */
struct fs64_filestructure {
  int open; /* file open flag */
  
  /* Queued character resulting from call to fs64_unreadchar() */
  int char_queuedP;
  int queued_char;

  uchar fs64name[16]; /* C64 filename, 0xa0 padded */
  uchar realname[MAX_FS_LEN];/* full pathname and filename of realfile/fs object */
  int first_track;  /* First block of file in object */
  int first_sector; /*                               */
  long first_poss; /* first byte of real file which is the body of the file */
  fs64_filesystem filesys;
  int mode;  /* mode of openness - ie, is it read or write */
  int curr_track;
  int curr_sector;
  long curr_poss;
  long blocks;  /* # of blocks in the file */
  long realsize; /* # of bytes in the real file which are part of the file */
  int filetype;
  
  /* block buffer */
  int bp; /* buffer pointer */
  int be; /* buffer end, when bp==be, the buffer is empty */
  int lastblock;
  uchar buffer[256]; /* sector buffer */
  
  /* things for if it is a directory */
  int isdir;
  fs64_direntry de;
  /* blocks free on filesystem */
  long blocksfree;
  
  /* for if its a buffer */
  int isbuff;
  
  /* for network streams */
  int socket;
  int msocket;
  int ip;
  int port;
  int sockmode;
  struct sockaddr_in sockaddr;
};
#define fs64_file struct fs64_filestructure

/* shared variables accross modules */

extern uchar	dos_status[MAX_NET_DEVS][256];
extern uchar	dos_stat_len[MAX_NET_DEVS];
extern uchar	dos_command[MAX_NET_DEVS][256];
extern uchar	dos_comm_len[MAX_NET_DEVS];
extern uchar	*partn_dirs[MAX_NET_DEVS][256];
extern int partn_dirtracks[MAX_NET_DEVS][256];
extern int partn_dirsectors[MAX_NET_DEVS][256];
extern int curr_dirtracks[MAX_NET_DEVS][256];
extern int curr_dirsectors[MAX_NET_DEVS][256];
extern int curr_par[MAX_NET_DEVS];
extern uchar	*curr_dir[MAX_NET_DEVS][256];
extern int pathdir;
extern fs64_file logical_files[MAX_NET_DEVS][16];

/* defines for fs64direntry.media */
#define media_UFS 1
#define media_D64 2 
#define media_D71 3 
#define media_D81 4 
#define media_DHD 5
#define media_T64 6 
#define media_NET 7
#define media_LNX 8
#define media_BUF 9
/* invalid media types */
#define media_NOTFS 256
#define media_BAD  257

/* defines for cbm file types */
#define cbm_DEL 0
#define cbm_SEQ 1
#define cbm_PRG 2
#define cbm_USR 3
#define cbm_REL 4
#define cbm_CBM 5
#define cbm_DIR 6
#define cbm_UFS 7
#define cbm_NET 8
#define cbm_invalid 15 /* bad filetype - or missing */
#define cbm_CLOSED 0x80
#define cbm_LOCKED 0x40

/* file type masks */
/* = 2**cbm_XXX    */
#define DEL_mask 1
#define SEQ_mask 2
#define PRG_mask 4
#define USR_mask 8
#define REL_mask 16 
#define CBM_mask 32 
#define DIR_mask 64 
#define UFS_mask 128 
#define NET_mask 256

/* archive types for file if in UFS media */
#define arc_UFS 0
#define arc_RAW 1
#define arc_P00 2
#define arc_N64 3

/* file access modes */
#define mode_READ 1
#define mode_WRITE 2
#define mode_BAD 0

/* some commonly used functions */

//these two don't exist...
//int fs64_isblockfree(fs64_filesystem *fs,int track,int sector);
//long real_size(char *path,char *name,int mediatype);

/* media_NET #defines 
   These defines are used in the fs64_file structure to define the type
   of the directory
*/
#define net_ROOTDIR      1
#define net_BYTEDIR      2
#define net_HOSTSDIR     3
#define net_SERVERDIR    4
#define net_SERVICESDIR  5
#define net_SERVICE      6
#define net_PORT         7
#define net_PORTDIR      8

/* No net flag */
extern int no_net;

extern int     last_drive,last_unit;

/* fs includes */
#include "fs_fileio.h"
#include "fs_error.h"
#include "fs_readts.h"
#include "fs_media.h"
#include "fs_parse.h"
#include "fs_search.h"
#include "fs_glob.h"
#include "fs_hostdep.h"

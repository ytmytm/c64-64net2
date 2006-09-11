/*
   UNIX File System communication module for 64net/2
   (C)Copyright Paul Gardner-Stephen 1996
 */

#include "config.h"
#include "fs.h"
#include "misc_func.h"

#ifdef UNIX
	#ifdef BEOS
		#define EDQUOT 0
		#include <sys/statvfs.h>
	#else
		#ifdef __FreeBSD__ 
			#include <sys/param.h>
			#include <sys/mount.h>
		#endif
		#ifdef __APPLE__
			#include <sys/param.h>
			#include <sys/mount.h>
		#else
			#include <sys/vfs.h>
		#endif
	#endif
#endif

#ifdef WINDOWS
#include <sys/stat.h>
#define EDQUOT 0
#define ELOOP -1
#endif

//more advanced versions by Toby, now used at all occasions where we need to convert:

void ascii2petscii (uchar *message, int len) {
        int i;
        for (i=0;i<len;i++) {
                if( ((message[i]>0x40) && (message[i]<=0x5a)) ) message[i]^=0x80;
                else if( ((message[i]>0x60) && (message[i]<=0x7a)) ) message[i]^=0x20;
        }
        return;
}

void petscii2ascii (uchar *message, int len) {
        int i;
        for (i=0;i<len;i++) {
                if((message[i]>0x40) && (message[i]<=0x5a)) message[i]^=0x20;
                else if((message[i]>0xc0) && (message[i]<=0xda)) message[i]^=0x80;
        }
        return;
}

int fs_ufs_createfile (uchar *path, uchar *name, int t, int rel_len, fs64_file * f) {
	int i;
	
	uchar fname[1024];

//add all these checks also when scratching. 
//
	//filename too long anyway
	if(strlen(name)>=16) {
		/* filename too long */
		set_error (53, 0, 0);
		return -1;
	}

	/* get a proper ascii name first */
	petscii2ascii(name,strlen(name));

	debug_msg ("ufs: Trying to create file %s%s [%d]\n", path, name, t);
	
	/* check for nasty chars */
	for(i=0;i<=(int)strlen(name);i++) {
		switch (name[i]) {
                        case ',':
                        case '/':
                        case '\\':
                        case '*':
                        case '?':
                        case ':':
                        case ';':
                        case '~':
				/* illegal filename */
				set_error(54,0,0);
				return 1;
                        break;
                        default:
                        break;
                }
	}

	sprintf ((char*)fname, "%s%s", path, name); //%s%s.prg
//	debug_msg ("ufs: rendered filename to: %s\n", fname);

	/* create file, and put vital info in */
	if ((f->filesys.fsfile = fopen ((char*)fname, "w")) == NULL) {
		/* cant create file */
		switch (errno) {
			case EACCES:
			case EROFS:
				set_error (26, 0, 0);
			break;
			case ENOTDIR:
			case ENAMETOOLONG:
				set_error (39, 0, 0);
			break;
			case ELOOP:
				set_error (62, 0, 0);
			break;
			case EISDIR:
			case EEXIST:
				set_error (63, 0, 0);
			break;
			case EMFILE:
			case ENFILE:
			case ENOSPC:
			case EDQUOT:
				set_error (72, 0, 0);
			break;
			case EIO:
				set_error (21, 0, 0);
			break;
		}
		return (-1);
	}				/* end if file not created */

	/* maybe this must move before if-statement above */
	f->filesys.media = media_UFS;
	f->filesys.arctype = cbm_PRG;
	strcpy ((char*)f->filesys.fspath, (char*)path);

	// set buffer and poss infomation etc.. 
	f->open = 1;
	strcpy ((char*)f->fs64name, (char*)name);
	strcpy ((char*)f->realname, (char*)fname);
	f->first_track = 0;
	f->first_sector = 0;
	f->first_poss = 0x00;
	f->mode = mode_WRITE;
	f->curr_track = 0;
	f->curr_sector = 0;
	f->curr_poss = 0x00;
	f->blocks = 0;
	f->realsize = 0;
	f->filetype = UFS_mask | t;
	f->bp = 2;
	f->be = 2;

	/* done! */
	return (0);
}

int fs_ufs_getopenablename (fs64_file * f, fs64_direntry * de) {
	strcpy ((char*)f->realname, (char*)de->realname);
	return (0);
}

int fs_ufs_openfile (fs64_file * f) {
	/* open for read coz this is the open file routine,
	   not create file */
	strcpy ((char*)f->filesys.fspath, (char*)f->realname);
	if ((f->filesys.fsfile = fopen ((char*)f->realname, "r")) == NULL) {
		/* couldn't open it */
		/* 74,DRIVE NOT READY,00,00 */
		set_error (74, 0, 0);
		return (-1);
	}
	/* OKAY, all things are done */
	f->open = 1;
	return (0);
}

int fs_ufs_allocateblock (fs64_filesystem * fs, int track, int sector) {
	/* stub routine coz UFS doesnt look blocky to 64net */
	return (0);
}


int fs_ufs_blocksfree (fs64_filesystem * fs) {
#ifdef AMIGA
	struct InfoData info;

	if (getdfs (0, &info) != 0) return (2);
	else return ((info.id_NumBlocks - info.id_NumBlocksUsed) * info.id_BytesPerBlock / 254);
#endif
#ifdef UNIX
	/* unix file system */
	struct statfs buf;

	errno = 0;

	statfs ((char*)fs->fspath, &buf);

	if (!errno) return ((buf.f_bsize * buf.f_bavail) / 254);
	else return (2);
#endif
#ifdef WINDOWS
	/* XXX - fixme */
	debug_msg("fs_ufs_blocksfree: unimplemented\n");
	return (2);
#endif
	/* dead code */
	return(0);
}


int fs_ufs_findfreeblock (fs64_filesystem * fs, int *track, int *sector) {
	/* unix file system */
	/* BUGS: always say there is a free block for now.. will fix later */
	*track = 0;
	*sector = 0;
	return (0);
}

int fs_ufs_deallocateblock (fs64_filesystem * fs, int track, int sector) {
	return (0);
}

int fs_ufs_isblockfree (fs64_filesystem * fs, int track, int sector) {
	/* BUGS: always returns true - should check for space */
	return (0);
}

int fs_ufs_scratchfile (fs64_direntry * de) {
	//if (unlink((const char*)de->realname)) return(-1);
	//else return (0);
	return 0;
}

int fs_ufs_writeblock (fs64_file * f) {
	/* BUGS: Doesnt check for free "blocks" aka diskspace */

	//debug_msg ("ufs: Write block %d,%d\n", f->be, f->bp);

	/* seek_set the file pointer right */
	fseek (f->filesys.fsfile, f->curr_poss, SEEK_SET);

	/* write the last 254 bytes of the block */
	if (f->bp == 256) {
		/* whole sector - allocate another */
		/* is irrelevant for this file system! */
		if (fs64_blocksfree (&f->filesys) < 1) {
			/* partition full */
			set_error (72, 0, 0);
			return (-1);
		}
		/* ok (fall through) */
	}
	else {
		/* partial sector */
		/* irrelevant for this file system */
		/* dont allocate another block as it is the end of the file */
		f->buffer[0] = 0;
		f->buffer[1] = (f->bp - 2);
		/* ok (fallthrough) */
	}
	for (f->be = 2; f->be < (f->bp); f->be++) {
		fputc (f->buffer[f->be], f->filesys.fsfile);
		f->curr_poss++;
		f->realsize++;
	}
	/* update file size stuff */
	//This destroys our .prg file, we better avoid. TB
	//fseek (f->filesys.fsfile, 7, SEEK_SET);
	//fprintf (f->filesys.fsfile, "%c%c%c%c", (int) f->realsize & 0xff,
	//(int) (0xff & (f->realsize / 256)), (int) (0xff & (f->realsize / 65536)),
	//(int) (0xff & (f->realsize / (65536 * 256))));
	/* set buffer pointer */
	f->bp = 2;
	f->be = 2;
	return (0);
}

int fs_ufs_readblock (fs64_file * f) {
	int c;
	/* seek_set the file pointer right */
	fseek (f->filesys.fsfile, f->curr_poss, SEEK_SET);
	/* initialize pointers */
	f->bp=2;
	f->be=2;
	/* read as many bytes into the block as we can */
	while(f->be<256) {
		c=fgetc(f->filesys.fsfile);
		if(c==EOF) {
			if(f->be > f->bp) return 0;	//end of file, but at least we had some left chars
			else return -1;			//end without a single char in buffer;
		}
		f->buffer[f->be]=c;
		f->curr_poss++;
		f->be++;
	}
	return 0;					//read block without problems
}

int fs_ufs_headername (uchar *path, uchar *header, uchar *id, int par) {
	/* use right 16 chars from path */
	int i, j;

	header[0] = 0;

	i = strlen (path) - 1;
	/* strip trailing '/' */
	if (path[i] == '/') i--;
	/* find start of this "bit" */
	while ((i) && (path[i] != '/')) i--;
	/* strip leading '/' on name */
	if (path[i] == '/') i++;
	/* copy  chars */
	j = 0;
	for (; !((path[i] == '/') || (path[i] == 0)); i++) {
		header[j++] = path[i];
	}
	ascii2petscii(header,j);
	/* end f the string */
	header[j] = 0;
	/* default */
	if ((!strcmp ((char*)path, "/")) || (header[0] == 0)) sprintf ((char*)header, "PARTITION %d", par);
	strcpy ((char*)id, "64NET");
	return (0);
}

int fs_ufs_openfind (fs64_direntry * de, uchar *path) {
	/* UNIX filesystem file */
	de->filesys.media = media_UFS;
	/* path in use */
	strcpy ((char*)de->fs, (char*)path);
	/* open a directory stream and check for first file */
	de->dir = opendir ((char*)path);

	if (!de->dir) {
		/* file system error of some evil sort no doubt */
		/* 74,DRIVE NOT READY,00,00 */
		set_error (74, 0, 0);
		return (-1);
	}
	de->active = 1;
	return (0);
}

int fs_ufs_findnext (fs64_direntry * de) {
	struct dirent *dirent;
	unsigned int i;

	/* read raw entry */
	dirent = readdir (de->dir);
	if (dirent) {
		//  /* skip . & .. */
		if (dirent && (!strcmp (".", dirent->d_name))) dirent = readdir (de->dir);
		//  if (dirent && (!strcmp ("..", dirent->d_name)))
		//    dirent = readdir (de->dir);
	}

	if (dirent) {
		/* fill out thing */
		strcpy ((char*)de->realname, (char*)de->path);
		strcat ((char*)de->realname, (char*)dirent->d_name);
		/* default filename */
		for (i = 0; i < 16; i++) {
			if (i < strlen (dirent->d_name)) de->fs64name[i] = dirent->d_name[i];
			else de->fs64name[i] = 0xa0;	/* 0xa0 padding , like 1541 */
		}
		
		//bugfix: else the strlen is not 16 in later tests/compares!
		de->fs64name[16]=0;
		fs64_getinfo(de);	//will also convert name if necessary
		return (0);
	}
	else {
		/* no more entries */
		closedir (de->dir);
		de->dir = 0;
		de->active = 0;
		return (-1);
	}
}

int fs_ufs_getinfo (fs64_direntry * de) {
	uchar tarr[1025];	/* buffer for first kb of file */
#ifdef AMIGA
	BPTR filelock;
	struct FileInfoBlock myFIB;
#else
	struct stat buf;
#endif
	FILE *temp = 0;
	long i;
	unsigned long j;

	if ((temp = fopen ((char*)de->realname, "r")) == NULL) {
		/* could not open file - might be a directory or something */
		/* so we will fall back on our dim assumption - */
		de->filesys.arctype = arc_UFS;
	}
	else {
		/* read in first 1024 bytes to try to find the file's type */
		memset(tarr,0,1024);
		fread(tarr,1024,1,temp);

		/* now try to find the file's type */
		de->filesys.arctype = arc_UFS;	/* default */
		de->invisible = 0;		/* we want to be able to see the file */
		if ((tarr[0] == 'C') && (tarr[1] == '6') && (tarr[2] == '4') && (tarr[3] < 3)) {
			/* n64 file! */
			de->filesys.arctype = arc_N64;
		}
		//else {
		/* is it just a raw file?? */
		//XXX This causes problems if we have a .d64 starting with 0x01 0x08
		//i had already such cases a .d64 wasn't recgnized :-/ Toby
		//maybe we have bette checks to make out a LNX archive? maybe we also 
		//include the .lnx ending? 
		//    if ((tarr[0] == 0x01) && ((tarr[1] == 8) || (tarr[1] == 0x1c) || (tarr[1] == 0x20) || (tarr[1] == 0x40))) {
		/* raw binary file */
		//      de->filesys.arctype = arc_RAW;
		//    }
		fclose (temp);
		temp = 0;
	}

	/* now we know the file structure - lets do something about it ;) */
	switch (de->filesys.arctype) {
		case arc_UFS: {
#ifdef AMIGA
			if ((filelock = Lock (de->realname, ACCESS_READ)) != 0)
			if (Examine (filelock, &myFIB))	{
				de->realsize = myFIB.fib_Size;
				/* calculate the size in blocks */
				de->blocks = (long) ((de->realsize / 254) + ((de->realsize % 254) && 1));
				/* File is UFS */
				de->filetype = cbm_UFS;
				if (myFIB.fib_DirEntryType > 0) de->filetype = cbm_DIR;
				de->filetype |= cbm_CLOSED * (((myFIB.fib_Protection & 0x08) == 0x08) == 0);
				/* If no write permission then is write protected */
				de->filetype |= cbm_LOCKED * (((myFIB.fib_Protection & 0x04) == 0x04) != 0);
				UnLock (filelock);
			}
			else  return (-1);/* Lock failed - something else has exclusive access? */
#else
			/* find the size of the binary portion of a file */
			stat ((char*)de->realname, &buf);
			/* calculate the file binary size */
			de->realsize = buf.st_size;
			/* calculate the size in blocks */
			de->blocks = (long) ((de->realsize / 254) + ((de->realsize % 254) && 1));
			/* File is UFS */
			de->filetype = cbm_UFS;
			if (buf.st_mode & S_IFDIR) de->filetype = cbm_DIR;
			de->filetype |= cbm_CLOSED * ((buf.st_mode & S_IREAD) != 0);
			/* If no write permission then is write protected */
			de->filetype |= cbm_LOCKED * ((buf.st_mode & S_IWRITE) == 0);
#endif
			/* use files real name - so do nothing (pre-set) */
			/* track and sector = -1 as not a sector available file */
			de->track = -1;
			de->sector = -1;
			/* binary base for a arc_UFS is zero */
			de->binbase = 0;

			/* check if right 4 chars are .d64 or .t64 - if so, then this is
			   a logical sub-directory
			*/
			i = strlen (de->realname);
			j = de->realname[i - 4] * 65536 * 256 + de->realname[i - 3] * 65536;
			j += de->realname[i - 2] * 256 + de->realname[i - 1];
			switch (j) {
				//XXX at loadtime, if filetype=xxx_DIR, then
				//cd to dir and return 0 bytes
				case 0x2e4c4e58:		/* .LNX */
				case 0x2e6c6e78:		/* .lnx */
					de->filetype &= (cbm_CLOSED | cbm_LOCKED);
					de->filetype |= cbm_DIR;
					de->first_track = -1;
					de->first_sector = -1;
					de->binbase=0;
					de->filesys.arctype=arc_RAW;
					break;

				case 0x2e443634:		/* .D64 */
				case 0x2e643634:		/* .d64 */
				case 0x2e543634:		/* .T64 */
				case 0x2e743634:		/* .t64 */
				case 0x2e443731:		/* .D71 */
				case 0x2e643731:		/* .d71 */
					de->filetype &= (cbm_CLOSED | cbm_LOCKED);
					de->filetype |= cbm_DIR;
					de->first_track = 18;
					de->first_sector = 0;
					break;
				case 0x2e443831:		/* .D81 */
				case 0x2e643831:		/* .d81 */
					de->filetype &= (cbm_CLOSED | cbm_LOCKED);
					de->filetype |= cbm_DIR;
					de->first_track = 40;
					de->first_sector = 0;
					break;
				case 0x2e444844:		/* .DHD */
				case 0x2e646864:		/* .dhd */
					de->filetype &= (cbm_CLOSED | cbm_LOCKED);
					de->filetype |= cbm_DIR;
					de->first_track = 1;
					de->first_sector = 1;
					break;
					/*
				case 0x2e505247:		// .PRG 
				case 0x2e707267:		// .prg 
			//		end=strlen(de->realname)-4;
			//		start=strlen(de->realname)-4;
			//		while(start>0) {
			//			if(de->realname[start]=='/') { start++; break; }
			//			start--;
			//		}
			//		for (i=0;i<16;i++) {
			//			if(start<end) {
			//				de->fs64name[i]=de->realname[start];
			//				start++;
			//			}
			//			else {
			//				de->fs64name[i]=0xa0;
			//			}
			//		}
					de->fs64name[17]=0xa0;
					de->filetype &= (cbm_CLOSED | cbm_LOCKED);
					de->filetype |= cbm_PRG;
					de->first_track = -1;
					de->first_sector = -1;
					
					break;
					*/
				default:
					if((de->filetype & 0x0f) == cbm_UFS) {
						de->filetype &= (cbm_CLOSED | cbm_LOCKED);
						de->filetype = (de->filetype&0xf0) | cbm_PRG;
						de->first_track = -1;
						de->first_sector = -1;
					}

			}
			if ((de->filetype & 0xf) == cbm_DIR) {
				ascii2petscii(de->fs64name,16);
			}
			else {
				ascii2petscii(de->fs64name,16);
			}
			return (0);
		}
		case arc_N64: {
			/* lets extract some info =) */
			de->filetype = tarr[4];
			de->track = 0;
			de->sector = 0;
			de->binbase = 254;
			for (i = 0; i < 16; i++) {
				if (tarr[31 + i] > 0) de->fs64name[i] = tarr[i + 31];
				else de->fs64name[i] = 0xa0;	/* 0xa0 padding , like 1541 */
			}
			/* The size too small by one caused a bug! */
			de->realsize = tarr[7] + 256 * tarr[8] + 65536 * tarr[9] + (65536 * 256) * tarr[10] + 1;
			de->blocks = (de->realsize) / 254 + ((de->realsize % 254) != 0);
	
			return (0);
		}
		default: {
			/* Ooh.. a bad thing (tm) */
			/* 74, DRIVE NOT READY,00,00 */
			set_error (74, 0, 0);
			return (-1);
		}
	
						/* end switch de.arctype */
	}
}


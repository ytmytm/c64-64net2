/* 
   64NET Media testing routines.
   (C)Copyright Paul Gardner-Stephen 1995, All rights reserved.
   This code is indended soley for use in the 64net/2 system.
 */

#include "fs.h"
#include "fs_func.h"
#include <ctype.h>

int 
fs64_mediatype (char *path)
{
  /* find out the media type for the path
     media_BAD will be returned if the path is bad
     and media_NOTFS if the path points to a file which is not a file system
     If the end of the path is a subdirectory then it will be media_UFS
     If the end of the path is a .D64 it will be media_D64
     If the end of the path is a .T64 it will be media_T64
   */

  /* BUGS:  This routine should parse through each slash seperately, as at
     present a subdirectory in a disk image, or a subdirectory itself for
     that matter, ending in .D64 or .T64 will mistakenly be recognuised as a
     disk/tape image */

#ifdef AMIGA
#include <dos.h>
  BPTR filelock;
  struct FileInfoBlock myFIB;
#else
  struct stat buf;
#endif
  int n;
  unsigned char temp[1024];

  /* get file info */

  /* BUGS: The following is *very* kludgy! */

  if (path[0] == '@')
  {
    /* @ sounds like a good way to start the internet file system :) */
    return (media_NET);
  }

#ifdef AMIGA
  if ((filelock = Lock (path, ACCESS_READ)) != 0)
  {
    if (!Examine (filelock, &myFIB))
    {
      UnLock (filelock);
      /* try stripping the / off the end */
      if (path[strlen (path) - 1] == '/')
      {
	strcpy (temp, path);
	temp[strlen (path) - 1] = 0;
	return (fs64_mediatype (temp));
      }
      else
	/* no go */
	return (media_BAD);	/* file/path not found */
    }
  }
#else
  if (stat (path, &buf))
  {
    /* try stripping the / off the end */
    if (path[strlen (path) - 1] == '/')
    {
      strcpy (temp, path);
      temp[strlen (path) - 1] = 0;
      return (fs64_mediatype (temp));
    }
    else
      /* no go */
      return (media_BAD);	/* file/path not found */
  }
#endif

  /* Check for file types now */

  /* Check for media_UFS */
#ifdef AMIGA
  if (myFIB.fib_DirEntryType > 0)
  {
    UnLock (filelock);
    return (media_UFS);		/* dir = unix filesystem */
  }
#else
  if (buf.st_mode & S_IFDIR)
    return (media_UFS);		/* dir = unix filesystem */
#endif

  n = strlen (path);
  if ((path[n - 4] == '.') && (toupper (path[n - 3]) == 'D') && (path[n - 2] == '7') && (path[n - 1] == '1'))
    return (media_D71);
  if ((path[n - 4] == '.') && (toupper (path[n - 3]) == 'D') && (path[n - 2] == '8') && (path[n - 1] == '1'))
    return (media_D81);
  if ((path[n - 4] == '.') && (toupper (path[n - 3]) == 'D') && (toupper (path[n - 2]) == 'H') && (toupper (path[n - 1]) == 'D'))
    return (media_DHD);


  if ((path[n - 4] == '.') && (path[n - 2] == '6') && (path[n - 1] == '4'))
  {
    /* the end of the filename is '.?64' */
    /* this routine doesnt seem to check for .N64s, and yet
       N64's can still be accessed .. hmmm */

    /* Check for .D64 */
    if ((path[n - 3] == 'd') || (path[n - 3] == 'D'))
    {
      /* its a D64 file! */
      return (media_D64);
    }
    if ((path[n - 3] == 't') || (path[n - 3] == 'T'))
    {
      /* Its a T64 file! */
      return (media_T64);
    }

    /* it matches no known archive type - so its a media_NOTFS */
    return (media_NOTFS);
  }

  if ((path[n - 4] == '.') && (toupper (path[n - 3]) == 'L') &&
      (toupper (path[n - 2]) == 'N') && (toupper (path[n - 1]) == 'X'))
  {
    /* .LNX file */
    return (media_LNX);
  }

  /* no match */
  /* default = unix's native FS */
  return (media_UFS);
}

int 
fs64_readts (fs64_filesystem * fs, int track, int sector, unsigned char *sb)
{
  return (readts (fs, track, sector, sb));
}

int 
fs64_writets (fs64_filesystem * fs, int track, int sector, unsigned char *sb)
{
  return (writets (fs, track, sector, sb));
}

int 
fs64_deallocateblock (fs64_filesystem * fs, int track, int sector)
{
  /* mark block allocated, return(0) if ok, or return(-1) if the
     block was previously allocated */

  switch (fs->media)
  {
  case media_UFS:
    return (fs_ufs_deallocateblock (fs, track, sector));
  case media_D64:
    return (fs_d64_deallocateblock (fs, track, sector));
  case media_D71:
    return (fs_d71_deallocateblock (fs, track, sector));
  case media_D81:
    return (fs_d81_deallocateblock (fs, track, sector));
  case media_DHD:
    return (fs_dhd_deallocateblock (fs, track, sector));
  default:
    /* unknown media */
    /* 74,DRIVE NOT READY,00,00 */
    set_error (74, 0, 0);
    return (-2);
  }				/* end switch media */
}				/* end fs64_deallocateblock() */


int 
fs64_allocateblock (fs64_filesystem * fs, int track, int sector)
{
  /* mark block allocated, return(0) if ok, or return(-1) if the
     block was previously allocated */


  switch (fs->media)
  {
  case media_UFS:
    return (fs_ufs_allocateblock (fs, track, sector));
    break;
  case media_D64:
    return (fs_d64_allocateblock (fs, track, sector));
    break;
  case media_D71:
    return (fs_d71_allocateblock (fs, track, sector));
    break;
  case media_D81:
    return (fs_d81_allocateblock (fs, track, sector));
    break;
  case media_DHD:
    return (fs_dhd_allocateblock (fs, track, sector));
    break;
  default:
    /* unknown media */
    /* 74,DRIVE NOT READY,00,00 */
    set_error (74, 0, 0);
    return (-2);
  }				/* end switch media */
}				/* end fs64_allocateblock() */

int 
fs64_findfreeblock (fs64_filesystem * fs, int *track, int *sector)
{
  /* locate the first empty sector on the disk */

  switch (fs->media)
  {
  case media_UFS:
    return (fs_ufs_findfreeblock (fs, track, sector));
    break;
  case media_D64:
    return (fs_d64_findfreeblock (fs, track, sector));
    break;
  case media_D71:
    return (fs_d71_findfreeblock (fs, track, sector));
    break;
  case media_D81:
    return (fs_d81_findfreeblock (fs, track, sector));
    break;
  case media_DHD:
    return (fs_dhd_findfreeblock (fs, track, sector));
    break;
  default:
    /* if in doubt - its full! ;) */
    /* give no error, as its only a query */
    *track = -1;
    *sector = -1;
    return (-1);
  }				/* end switch media */
}				/* end fs64_findfreeblock */

int 
fs64_blocksfree (fs64_filesystem * fs)
{
  /* tell us how many free blocks in a file system object
     Called by fs64_writeblock to determine if there is any free space,
     and by directory searches to provide the BLOCKS FREE line */

  printf ("fs64_blocksfree(M=%d)\n", fs->media);

  switch (fs->media)
  {
  case media_UFS:
    return (fs_ufs_blocksfree (fs));
    break;
  case media_D64:
    return (fs_d64_blocksfree (fs));
    break;
  case media_D71:
    return (fs_d71_blocksfree (fs));
    break;
  case media_D81:
    return (fs_d81_blocksfree (fs));
    break;
  case media_DHD:
    return (fs_dhd_blocksfree (fs));
    break;
  case media_NET:
    return (fs_net_blocksfree (fs));
    break;
  default:
    /* if in doubt - its full! ;) */
    /* 74,DRIVE NOT READY,00,00 */
    set_error (74, 0, 0);
    return (0);
  }				/* end switch media */
}				/* end fs64_blocksfree */

/*
   UNIX File System communication module for 64net/2
   (C)Copyright Paul Gardner-Stephen 1996
 */

#include "config.h"
#include "fs.h"

#ifdef UNIX
#ifdef BSD
#include <sys/param.h>
#include <sys/mount.h>
#else
#include <sys/vfs.h>
#endif
#endif

#ifdef WINDOWS
#include <sys/stat.h>
#define EDQUOT 0
#define ELOOP -1
#endif

int
fs_ufs_createfile (uchar *path, uchar *name, int t, int rel_len, fs64_file * f)
{
  uchar fname[1024], rname[32];
  int i;

  debug_msg ("ufs: Trying to create file %s%s [%d]\n",
	     path, name, t);

  /* get shortened name for file */
  if (shortname (path, name, rname, (uchar*)".n64"))
  {
    /* disk full (or dir full etc..) */
    set_error (72, 0, 0);
    return (-1);
  }

  sprintf ((char*)fname, "%s%s.n64", path, rname);
  debug_msg ("ufs: rendered filename to: %s\n", fname);

  /* create file, and put vital info in */
  f->filesys.media = media_UFS;
  f->filesys.arctype = arc_N64;
  strcpy (f->filesys.fspath, path);
  if ((f->filesys.fsfile = fopen (fname, "w")) == NULL)
  {
    /* cant create file */
    switch (errno)
    {
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

  /* put default stuff into file */
  /* header & filetype & filesize placeholders */
  fprintf (f->filesys.fsfile, "C64%c%c%c%c%c%c", 0x01, t | 0x80, 0, 0, 0, 0);
  /* padding (reserved) */
  for (i = 0; i < 22; i++)
    fprintf (f->filesys.fsfile, "%c", 0);
  /* filename */
  for (i = 16; i > strlen (name); i--)
    name[i] = 0;
  for (i = 0; i < 16; i++)
    fprintf (f->filesys.fsfile, "%c", (char) name[i]);
  /* padding to actual start of file */
  for (i = 0; i < 0xcf; i++)
    fprintf (f->filesys.fsfile, "%c", 0);
  /* set buffer and poss infomation etc.. */
  f->open = 1;
  strcpy (f->fs64name, name);
  strcpy (f->realname, fname);
  f->first_track = 0;
  f->first_sector = 0;
  f->first_poss = 0xfe;
  f->mode = mode_WRITE;
  f->curr_track = 0;
  f->curr_sector = 0;
  f->curr_poss = 0xfe;
  f->blocks = 0;
  f->realsize = 0;
  f->filetype = 0x80 | t;
  f->bp = 2;
  f->be = 2;

  /* done! */
  return (0);

}

int
fs_ufs_getopenablename (fs64_file * f, fs64_direntry * de)
{
  strcpy (f->realname, de->realname);
  return (0);
}

int
fs_ufs_openfile (fs64_file * f)
{
  /* open for read coz this is the open file routine,
     not create file */
  strcpy (f->filesys.fspath, f->realname);
  if ((f->filesys.fsfile = fopen (f->realname, "r")) == NULL)
  {
    /* couldn't open it */
    /* 74,DRIVE NOT READY,00,00 */
    set_error (74, 0, 0);
    return (-1);
  }
  /* OKAY, all things are done */
  f->open = 1;
  return (0);
}

int
fs_ufs_allocateblock (fs64_filesystem * fs, int track, int sector)
{
  /* stub routine coz UFS doesnt look blocky to 64net */
  return (0);
}


int
fs_ufs_blocksfree (fs64_filesystem * fs)
{
#ifdef AMIGA
  struct InfoData info;

  if (getdfs (0, &info) != 0)
    return (2);
  else
    return ((info.id_NumBlocks - info.id_NumBlocksUsed) * info.id_BytesPerBlock
	    / 254);
#endif
#ifdef UNIX
  /* unix file system */
  struct statfs buf;

  errno = 0;

  statfs ((char*)fs->fspath, &buf);

  if (!errno)
    return ((buf.f_bsize * buf.f_bavail) / 254);
  else
    return (2);
#endif
#ifdef WINDOWS
    /* XXX - fixme */
    debug_msg("fs_ufs_blocksfree: unimplemented\n");
    return (2);
#endif
  /* dead code */
  return(0);
}


int
fs_ufs_findfreeblock (fs64_filesystem * fs, int *track, int *sector)
{
  /* unix file system */
  /* BUGS: always say there is a free block for now.. will fix later */
  *track = 0;
  *sector = 0;
  return (0);
}

int
fs_ufs_deallocateblock (fs64_filesystem * fs, int track, int sector)
{
  return (0);
}

int
fs_ufs_isblockfree (fs64_filesystem * fs, int track, int sector)
{
  /* BUGS: always returns true - should check for space */
  return (0);
}

int
fs_ufs_scratchfile (fs64_direntry * de)
{
  /* delete file currently pointed to in the de */

  debug_msg ("Pretending to delete UFS [%s]\n", de->realname);
  /* if (unlink(de->realname))
     return(-1);
   */
  return (-1);
}

int
fs_ufs_writeblock (fs64_file * f)
{
  /* BUGS: Doesnt check for free "blocks" aka diskspace */

  debug_msg ("ufs: Write block %d,%d\n", f->be, f->bp);

  /* seek_set the file pointer right */
  fseek (f->filesys.fsfile, f->curr_poss, SEEK_SET);

  /* write the last 254 bytes of the block */
  if (f->bp == 256)
  {
    /* whole sector - allocate another */
    /* is irrelevant for this file system! */
    if (fs64_blocksfree (&f->filesys) < 1)
    {
      /* partition full */
      set_error (72, 0, 0);
      return (-1);
    }
    /* ok (fall through) */
  }
  else
  {
    /* partial sector */
    /* irrelevant for this file system */
    /* dont allocate another block as it is the end of the file */
    f->buffer[0] = 0;
    f->buffer[1] = (f->bp - 2);
    /* ok (fallthrough) */
  }
  for (f->be = 2; f->be < (f->bp); f->be++)
  {
    fputc (f->buffer[f->be], f->filesys.fsfile);
    f->curr_poss++;
    f->realsize++;
  }
  /* update file size stuff */
  fseek (f->filesys.fsfile, 7, SEEK_SET);
  fprintf (f->filesys.fsfile, "%c%c%c%c", (int) f->realsize & 0xff,
   (int) (0xff & (f->realsize / 256)), (int) (0xff & (f->realsize / 65536)),
	   (int) (0xff & (f->realsize / (65536 * 256))));
  /* set buffer pointer */
  f->bp = 2;
  f->be = 2;
  return (0);
}

int
fs_ufs_readblock (fs64_file * f)
{
  /* seek_set the file pointer right */
  fseek (f->filesys.fsfile, f->curr_poss, SEEK_SET);

  /* read as many bytes into the block as we can */
  if (f->curr_poss < (f->first_poss + f->realsize))
  {
    for (f->be = 2; f->be < 256; f->be++)
    {
      f->buffer[f->be] = fgetc (f->filesys.fsfile);
      f->curr_poss++;
      if (f->curr_poss >= (f->first_poss + f->realsize))
	/* eof reached */
	break;
    }
    /* add one so things work happily */
    f->be++;
    /* and make sure its not over the ceiling */
    if (f->be > 256)
      f->be = 256;
    /* set buffer pointer */
    f->bp = 2;
    return (0);
  }
  else
  {
    /* were already at the end of the file */
    /* 61, FILE NOT OPEN,00,00 */
    set_error (61, 0, 0);
    return (-1);
  }
}

int
fs_ufs_headername (uchar *path, uchar *header, uchar *id, int par)
{
  /* use right 16 chars from path */
  int i, j;

  header[0] = 0;

  i = strlen (path) - 1;
  /* strip trailing '/' */
  if (path[i] == '/')
    i--;
  /* find start of this "bit" */
  while ((i) && (path[i] != '/'))
    i--;
  /* strip leading '/' on name */
  if (path[i] == '/')
    i++;
  /* copy  chars */
  j = 0;
  for (; !((path[i] == '/') || (path[i] == 0)); i++)
  {
    header[j++] = path[i];
    if (isalpha (header[j - 1]))
      header[j - 1] ^= 0x20;
  }
  /* end f the string */
  header[j] = 0;
  /* default */
  if ((!strcmp (path, "/")) || (header[0] == 0))
    sprintf ((char*)header, "PARTITION %d", par);

  strcpy (id, "64NET");

  return (0);
}

int
fs_ufs_openfind (fs64_direntry * de, uchar *path)
{
  /* UNIX filesystem file */
  de->filesys.media = media_UFS;
  /* path in use */
  strcpy (de->fs, path);
  /* open a directory stream and check for first file */
  de->dir = opendir ((char*)path);

  if (!de->dir)
  {
    /* file system error of some evil sort no doubt */
    /* 74,DRIVE NOT READY,00,00 */
    set_error (74, 0, 0);
    return (-1);
  }
  de->active = 1;
  return (0);
}

int
fs_ufs_findnext (fs64_direntry * de)
{
  struct dirent *dirent;
  int i;

  /* read raw entry */
  dirent = readdir (de->dir);
  if (dirent)
  {
    /* skip . & .. */
    if (dirent && (!strcmp (".", dirent->d_name)))
      dirent = readdir (de->dir);
    if (dirent && (!strcmp ("..", dirent->d_name)))
      dirent = readdir (de->dir);
  }

  if (dirent)
  {
    /* fill out thing */
    strcpy (de->realname, de->path);
    strcat ((char*)de->realname, dirent->d_name);
    /* default filename */
    for (i = 0; i < 16; i++)
      if (i < strlen (dirent->d_name))
	de->fs64name[i] = dirent->d_name[i];
      else
	de->fs64name[i] = 0xa0;	/* 0xa0 padding , like 1541 */
    fs64_getinfo (de);
    return (0);
  }
  else
  {
    /* no more entries */
    closedir (de->dir);
    de->dir = 0;
    de->active = 0;
    return (-1);
  }
}

int
fs_ufs_getinfo (fs64_direntry * de)
{
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

  if ((temp = fopen (de->realname, "r")) == NULL)
  {
    /* could not open file - might be a directory or something */
    /* so we will fall back on our dim assumption - */
    de->filesys.arctype = arc_UFS;
  }
  else
  {
    /* read in first 1024 bytes to try to find the file's type */
    fread(tarr,1024,1,temp);
    /* for (i = 0; i < 1024; i++)
       tarr[i] = fgetc (temp); */

    /* now try to find the file's type */
    de->filesys.arctype = arc_UFS;	/* default */
    de->invisible = 0;		/* we want to be able to see the file */
    if ((tarr[0] == 'C') && (tarr[1] == '6') && (tarr[2] == '4') && (tarr[3] < 3))
    {
      /* n64 file! */
      de->filesys.arctype = arc_N64;
    }
    /* is it just a raw file?? */
    if ((tarr[0] == 0x01) && ((tarr[1] == 8) || (tarr[1] == 0x1c) || (tarr[1] == 0x20) || (tarr[1] == 0x40)))
    {
      /* raw binary file */
      de->filesys.arctype = arc_RAW;
    }
    fclose (temp);
    temp = 0;
  }

  /* now we know the file structure - lets do something about it ;) */
  switch (de->filesys.arctype)
  {
  case arc_UFS:
    {
#ifdef AMIGA
      if ((filelock = Lock (de->realname, ACCESS_READ)) != 0)
	if (Examine (filelock, &myFIB))
	{
	  de->realsize = myFIB.fib_Size;
	  /* calculate the size in blocks */
	  de->blocks = (long) ((de->realsize / 254) + ((de->realsize % 254) && 1));
	  /* File is UFS */
	  de->filetype = cbm_UFS;
	  if (myFIB.fib_DirEntryType > 0)
	    de->filetype = cbm_DIR;
	  de->filetype |= cbm_CLOSED * (((myFIB.fib_Protection & 0x08) == 0x08) == 0);
	  /* If no write permission then is write protected */
	  de->filetype |= cbm_LOCKED * (((myFIB.fib_Protection & 0x04) == 0x04) != 0);
	  UnLock (filelock);
	}
	else
	  return (-1);		/* Lock failed - something else has exclusive access? */
#else
      /* find the size of the binary portion of a file */
      stat ((char*)de->realname, &buf);
      /* calculate the file binary size */
      de->realsize = buf.st_size;
      /* calculate the size in blocks */
      de->blocks = (long) ((de->realsize / 254) + ((de->realsize % 254) && 1));
      /* File is UFS */
      de->filetype = cbm_UFS;
      if (buf.st_mode & S_IFDIR)
	de->filetype = cbm_DIR;
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
      switch (j)
      {
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
      }

      if ((de->filetype & 0xf) == cbm_DIR)
      {
	/* mono case */
	for (i = 0; i < 16; i++)
	{
	  if (isalpha (de->fs64name[i]))
	    de->fs64name[i] = toupper (de->fs64name[i]);
	}
      }
      else
      {
	/* case invert */
	for (i = 0; i < 16; i++)
	{
	  if (isalpha (de->fs64name[i]))
	    de->fs64name[i] ^= 0x20;
	}
      }

      return (0);
    }
  case arc_RAW:
    {
#ifdef AMIGA
      if ((filelock = Lock (de->realname, ACCESS_READ)) != 0)
	if (Examine (filelock, &myFIB))
	{
	  de->realsize = myFIB.fib_Size;
	  /* calculate the size in blocks */
	  de->blocks = (long) ((de->realsize / 254) + ((de->realsize % 254) && 1));
	  /* File is UFS */
	  de->filetype = cbm_UFS;
	  if (myFIB.fib_DirEntryType > 0)
	    de->filetype = cbm_DIR;
	  de->filetype |= cbm_CLOSED * (((myFIB.fib_Protection & 0x08) == 0x08) == 0);
	  /* If no write permission then is write protected */
	  de->filetype |= cbm_LOCKED * (((myFIB.fib_Protection & 0x04) == 0x04) != 0);
	  UnLock (filelock);
	}
	else
	  return (-1);		/* Lock failed - something else has exclusive access? */
#else
      stat ((char*)de->realname, &buf);
      de->realsize = buf.st_size;
      de->blocks = (long) ((de->realsize / 254) + ((de->realsize % 254) && 1));
      if (buf.st_mode & S_IFDIR)
	de->filetype = cbm_DIR;
      else
	de->filetype = cbm_PRG;
      de->filetype |= cbm_CLOSED * ((buf.st_mode & S_IREAD) != 0);
      de->filetype |= cbm_LOCKED * ((buf.st_mode & S_IWRITE) == 0);
#endif
      /* use defualt filename (preset) */
      de->track = -1;
      de->sector = -1;
      de->binbase = 0;

      i = strlen (de->realname);
      j = de->realname[i - 4] * 65536 * 256 + de->realname[i - 3] * 65536;
      j += de->realname[i - 2] * 256 + de->realname[i - 1];
      /* check if its a lynx file */
      switch (j)
      {
      case 0x2e4c4e58:		/* .LNX */
      case 0x2e6c6e78:		/* .lnx */
	de->filetype &= (cbm_CLOSED | cbm_LOCKED);
	de->filetype |= cbm_DIR;
	{
	  /* mono case */
	  int i;
	  for (i = 0; i < 16; i++)
	  {
	    if (isalpha (de->fs64name[i]))
	      de->fs64name[i] = toupper (de->fs64name[i]);
	  }
	}
      }
      return (0);
    }
  case arc_N64:
    {
      /* lets extract some info =) */
      de->filetype = tarr[4];
      de->track = 0;
      de->sector = 0;
      de->binbase = 254;
      for (i = 0; i < 16; i++)
	if (tarr[31 + i] > 0)
	  de->fs64name[i] = tarr[i + 31];
	else
	  de->fs64name[i] = 0xa0;	/* 0xa0 padding , like 1541 */
      /* The size too small by one caused a bug! */
      de->realsize = tarr[7] + 256 * tarr[8] + 65536 * tarr[9] + (65536 * 256) * tarr[10] + 1;
      de->blocks = (de->realsize) / 254 + ((de->realsize % 254) != 0);

      return (0);
    }
  default:
    {
      /* Ooh.. a bad thing (tm) */
      /* 74, DRIVE NOT READY,00,00 */
      set_error (74, 0, 0);
      return (-1);
    }
  }				/* end switch de.arctype */
}

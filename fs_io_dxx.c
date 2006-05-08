/*
   Disk Image File System Interface routine module for 64net/2
   (Say that with a mouth full of golf balls!)
   (C)Copyright Paul Gardner-Stephen 1996
 */

#include "config.h"
#include "fs.h"
#include "fs_func.h"
#include "datestamp.h"

int 
fs_dxx_getinfo (fs64_direntry * de)
{
  long i;
  uchar sectorbuffer[256];

  /* Read in the sector and sort the info there-in 
     if intcount =8 then link to next sector.
     if no next sector - return(-1); for no file.
   */

  if (!readts (&de->filesys, de->track, de->sector, sectorbuffer))
  {

    /* read sucessful - so lets go on */
    if (de->intcount > 7)
    {
      /* weve finished this sector */
      /* so check for next sector link.. */
      if (sectorbuffer[0])
      {
	/* link to next sector */
	de->track = sectorbuffer[0];
	de->sector = sectorbuffer[1];
	de->intcount = 0;
	/* now recurse into this function with a "good" parameter */

	return (fs64_getinfo (de));
      }
      else
      {
	/* no next sector, ergo - end of directory */
	/* not an error */
	return (-1);		/* no sir,.. i dont like it */
      }
    }

    /* OK... lets *really* get the files info :) */
    de->invisible = 0;		/* we want the file to be visible */
    de->filetype = sectorbuffer[2 + 32 * de->intcount];
    /* copy filename */
    for (i = 0; i < 16; i++)
    {
      de->fs64name[i] = sectorbuffer[de->intcount * 32 + 5 + i];
    }
    de->fs64name[16] = 0;
    /* blocks */
    de->blocks = sectorbuffer[30 + 32 * de->intcount] + 256 * sectorbuffer[31 + 32 * de->intcount];
    /* start track and sector of file */
    de->first_track = sectorbuffer[3 + 32 * de->intcount];
    de->first_sector = sectorbuffer[4 + 32 * de->intcount];
    return (0);
  }
  else
  {
    /* couldnt read sector :( */
    /* readts will have set the error */
    return (-1);
  }
}

int 
fs_dxx_findnext (fs64_direntry * de)
{
  /* all work for this file system is done in fs64_getinfo
     as no actual file name is fetched (the job of fs64_findnext
     itself).
   */

  int i = 0;

  /* read entries until a realone turns up,
     or the directory is emptied */

  de->filetype = 0;
  while ((!de->filetype) && (!i))
  {
    i = fs64_getinfo (de);
    de->intcount++;
  }
  if (i)
  {
    /* end of directory, so shut things down */
    de->active = 0;
    fclose (de->filesys.fsfile);
    de->filesys.fsfile=0;
  }
  return (i);
}

int 
fs_dxx_format (fs64_filesystem * fs, uchar *name, uchar *id)
{
  switch (fs->media)
  {
  case media_D64:
    return (fs_d64_format (fs, name, id));
  case media_D71:
    return (fs_d71_format (fs, name, id));
  case media_D81:
    return (fs_d81_format (fs, name, id));
  case media_DHD:
    return (fs_dhd_format (fs, name, id));
  default:
    set_error (38, 0, 0);
    return (-1);		/* unimplemented */
  }
}

int 
fs_dxx_validate (fs64_filesystem * fs, int purgeflag)
{
  /* Validate a file system */
  switch (fs->media)
  {
  case media_D64:
    return (fs_d64_validate (fs, purgeflag));
  case media_D71:
    return (fs_d71_validate (fs, purgeflag));
  case media_D81:
    return (fs_d81_validate (fs, purgeflag));
  case media_DHD:
    return (fs_dhd_validate (fs, purgeflag));
  default:
    set_error (38, 0, 0);
    return (-1);		/* unimplemented */
  }
}

int 
fs_dxx_createfile (uchar *fsfilename, uchar *name, int ft, int rel_len, fs64_file * f, int dirtrack, int dirsect, int media)
{

/* create a new file on a disk image */
  /* Methodology:

     0) convert path to a filesystem
     1) Check for atleast 2 free blocks
     2) Check for free dir entry (alloc new dir block if required
     using method appropriate to media)
     3) Create dir entry
     4) Alloc first block (?)
   */

  uchar sectbuff[256];
  int t, s;

  /* Step 0 - Convert fsfilename to a 64net/2 file system */
  strcpy ((char*)f->filesys.fspath, (char*)fsfilename);
  f->filesys.dirtrack = dirtrack;
  f->filesys.dirsector = dirsect;
  t = dirtrack;
  s = dirsect;
  f->filesys.fsfile = fopen ((char*)fsfilename, "r+");
  if (!f->filesys.fsfile)
  {
    set_error (74, 0, 0);
    return (-1);
  }
  f->filesys.media = media;

  /* Step 1 - Check for atleast 2 free blocks */
  if (fs64_blocksfree (&f->filesys) < 2)
  {
    /* Disk full */
    fclose (f->filesys.fsfile);
    f->filesys.fsfile=0;
    set_error (72, 0, 0);
    return (-1);
  }

  /* Step 2 - Find free direntry (ie file type = 0x00) */

  /* Step 2.1- get first *real* directory block (ie read link) */
  if (readts (&f->filesys, t, s, sectbuff))
  {
    /* readts will have set the error status */
    fclose (f->filesys.fsfile);
    f->filesys.fsfile=0;
    return (-1);
  }
  t = sectbuff[0];
  s = sectbuff[1];
  /* read this block in */
  if (readts (&f->filesys, t, s, sectbuff))
  {
    /* readts will have set the error status */
    return (-1);
  }

  /* Step 2.2 - start search for free directory entry */
  {
    int i = 0;
    while (1)
    {
      if (sectbuff[i * 32 + 2] == 0)
      {
	/* bingo! */
	break;
      }
      else
      {
	i++;
	if (i > 7)
	{
	  /* link to next block */
	  int ot, os;
	  i = 0;
	  ot = t;
	  os = s;
	  t = sectbuff[0];
	  s = sectbuff[1];
	  if (t == 0)
	  {
	    /* allocate a new directory block */
	    int tt, ts;
	    if (fs_dxx_findfreeblock (&f->filesys, &tt, &ts))
	    {
	      fclose (f->filesys.fsfile);
	      f->filesys.fsfile=0;
	      return (-1);
	    }
	    else
	    {
	      sectbuff[0] = tt;
	      sectbuff[1] = ts;
	      if (writets (&f->filesys, ot, os, sectbuff))
	      {
		fclose (f->filesys.fsfile);
		f->filesys.fsfile=0;
		return (-1);
	      }
	      if (fs_dxx_allocateblock (&f->filesys, tt, ts))
	      {
		fclose (f->filesys.fsfile);
		f->filesys.fsfile=0;
		return (-1);
	      }
	      t = tt;
	      s = ts;
	      /* prepare sector */
	      sectbuff[0] = 0;
	      sectbuff[1] = 0xff;
	      sectbuff[2] = 0;
	      sectbuff[34] = 0;
	      sectbuff[66] = 0;
	      sectbuff[98] = 0;
	      sectbuff[130] = 0;
	      sectbuff[162] = 0;
	      sectbuff[194] = 0;
	      sectbuff[226] = 0;
	      /* and write */
	      if (writets (&f->filesys, t, s, sectbuff))
	      {
		fclose (f->filesys.fsfile);
		f->filesys.fsfile=0;
		return (-1);
	      }
	    }
	  }
	  else
	  {
	    /* read next block */
	    readts (&f->filesys, t, s, sectbuff);
	  }
	}			/* if (i>7) */
      }				/* else - ie not empty directory entry */
    }				/* while (1) */
    /* find, and allocate a block */
    {
      int tt, ts;
      if (fs_dxx_findfreeblock (&f->filesys, &tt, &ts))
      {
	fclose (f->filesys.fsfile);
	f->filesys.fsfile=0;
	return (-1);
      }
      else
      {
	fs_dxx_allocateblock (&f->filesys, tt, ts);
      }
      /* now make dir entry */
      switch (ft)
      {
      case cbm_PRG:
      case cbm_SEQ:
      case cbm_USR:
	{
	  /* create PRG,USR, or SEQ file */
	  int j;
	  sectbuff[i * 32 + 2] = ft;	/* file type */
	  sectbuff[i * 32 + 3] = tt;	/* start track */
	  sectbuff[i * 32 + 4] = ts;	/* start sector */
	  sectbuff[i * 32 + 30] = 0;
	  sectbuff[i * 32 + 31] = 0;	/* blocks in file */
	  /* filename (0xa0 padded) */
	  for (j = 0; name[j]; j++)
	  {
	    sectbuff[i * 32 + 5 + j] = name[j];
	  }
	  for (; j < 16; j++)
	    sectbuff[i * 32 + 5 + j] = 0xa0;
	  /* time/date stamp */
	  {
	    int y, m, d, h, mi, s;
	    gettimestamp (&y, &m, &d, &h, &mi, &s);
	    sectbuff[i * 32 + 23] = y % 100;
	    sectbuff[i * 32 + 24] = m;
	    sectbuff[i * 32 + 25] = d;
	    sectbuff[i * 32 + 26] = h;
	    sectbuff[i * 32 + 27] = mi;
	  }
	  if (writets (&f->filesys, t, s, sectbuff))
	  {
	    fclose (f->filesys.fsfile);
	    f->filesys.fsfile=0;
	    return (-1);
	  }
	  /* prepare first sector etc */
	  f->buffer[0] = 0;
	  f->buffer[1] = 0;
	  /* set buffer and poss infomation etc.. */
	  f->open = 1;
	  strcpy ((char*)f->fs64name, (char*)name);
	  strcpy ((char*)f->realname, (char*)fsfilename);
	  f->first_track = tt;
	  f->first_sector = ts;
	  f->first_poss = 0xfe;
	  f->mode = mode_WRITE;
	  f->curr_track = tt;
	  f->curr_sector = ts;
	  f->blocks = 0;
	  f->realsize = 0;
	  f->filetype = 0x80 | t;
	  f->bp = 2;
	  f->be = 2;
	  /* directory entry stuff */
	  f->de.track = t;
	  f->de.sector = s;
	  f->de.intcount = i;
	  /* bingo! */
	  return (0);
	  break;
	}
      case cbm_REL:
	{
	  /* To create a rel:
	     1) Make dir entry
	     2) are we on a .DHD ?
	     Yes ->
	     2.1) Create super-side sector
	     2.2) Create side-sector
	     No ->
	     2.1) Create side sector
	   */
	  int j;
	  int ss, st, sst=0, sss=0;	/* side and super side sector blocks */
	  /* allocate block for [super] side sector */
	  if (fs_dxx_findfreeblock (&f->filesys, &st, &ss))
	  {
	    fclose (f->filesys.fsfile);
	    f->filesys.fsfile=0;
	    return (-1);
	  }
	  else
	  {
	    if (fs_dxx_allocateblock (&f->filesys, st, ss))
	    {
	      /* file system is inconsistant! */
	      set_error (71, st, ss);
	      fclose (f->filesys.fsfile);
	      f->filesys.fsfile=0;
	      return (-1);
	    }
	  }
	  if (f->filesys.media == media_DHD)
	  {
	    /* copy side block id to super side block id */
	    sst = st;
	    sss = ss;
	    /* allocate block for side sector */
	    if (fs_dxx_findfreeblock (&f->filesys, &st, &ss))
	    {
	      fclose (f->filesys.fsfile);
	      f->filesys.fsfile=0;
	      return (-1);
	    }
	    else
	    {
	      if (fs_dxx_allocateblock (&f->filesys, st, ss))
	      {
		/* file system is inconsistant! */
		set_error (71, st, ss);
		fclose (f->filesys.fsfile);
		return (-1);
	      }
	    }
	  }
	  /* Make dir entry */
	  sectbuff[i * 32 + 2] = ft | cbm_CLOSED;
	  sectbuff[i * 32 + 3] = tt;	/* start track */
	  sectbuff[i * 32 + 4] = ts;	/* start sector */
	  if (f->filesys.media == media_DHD)
	  {
	    /* super side sector */
	    sectbuff[i * 32 + 19] = sst;
	    sectbuff[i * 32 + 20] = sss;
	  }
	  else
	  {
	    /* side sector */
	    sectbuff[i * 32 + 19] = st;
	    sectbuff[i * 32 + 20] = ss;
	  }
	  sectbuff[i * 32 + 21] = rel_len;	/* record size */
	  sectbuff[i * 32 + 30] = 0;
	  sectbuff[i * 32 + 31] = 0;	/* blocks in file */
	  /* filename (0xa0 padded) */
	  for (j = 0; name[j]; j++)
	  {
	    sectbuff[i * 32 + 5 + j] = name[j];
	  }
	  for (; j < 16; j++)
	    sectbuff[i * 32 + 5 + j] = 0xa0;
	  /* time/date stamp */
	  {
	    int y, m, d, h, mi, s;
	    gettimestamp (&y, &m, &d, &h, &mi, &s);
	    sectbuff[i * 32 + 23] = y % 100;
	    sectbuff[i * 32 + 24] = m;
	    sectbuff[i * 32 + 25] = d;
	    sectbuff[i * 32 + 26] = h;
	    sectbuff[i * 32 + 27] = mi;
	  }
	  /* blocks initially allocated */
	  if (f->filesys.media == media_DHD)
	    sectbuff[i * 32 + 28] = 3;
	  else
	    sectbuff[i * 32 + 28] = 2;
	  sectbuff[i * 32 + 29] = 0;
	  /* write parent dir block back */
	  if (writets (&f->filesys, t, s, sectbuff))
	  {
	    fclose (f->filesys.fsfile);
	    f->filesys.fsfile=0;
	    return (-1);
	  }
	  /* create side sector */
	  for (j = 0; j < 256; j++)
	    sectbuff[j] = 0;
	  sectbuff[0] = 0;
	  sectbuff[1] = 0x11;	/* chars used (15 bytes header, one record) */
	  sectbuff[2] = 0;	/* side sector number */
	  sectbuff[3] = rel_len;	/* record length */
	  sectbuff[4] = st;	/* first side sector track */
	  sectbuff[5] = ss;	/* first side sector sector */
	  sectbuff[0x10] = tt;	/* first data fork track */
	  sectbuff[0x11] = ts;	/* first data fork sector */
	  if (writets (&f->filesys, st, ss, sectbuff))
	  {
	    fclose (f->filesys.fsfile);
	    f->filesys.fsfile=0;
	    return (-1);
	  }
	  /* prepare first data block */
	  for (j = 0; j < 256; j++)
	    sectbuff[j] = 0;
	  for (j = 0; j < 254; j += rel_len)
	    sectbuff[j + 2] = 0xff;
	  j--;
	  if (j >= 254)
	    j -= rel_len;
	  sectbuff[0] = 0;
	  sectbuff[1] = 2 + j;	/* one byte before last ff in block */
	  if (writets (&f->filesys, tt, ts, sectbuff))
	  {
	    fclose (f->filesys.fsfile);
	    f->filesys.fsfile=0;
	    return (-1);
	  }
	  if (f->filesys.media == media_DHD)
	  {
	    /* prepare first super side sector */
	    for (j = 0; j < 256; j++)
	      sectbuff[j] = 0;
	    sectbuff[0] = st;	/* first side sector in first group */
	    sectbuff[1] = ss;
	    sectbuff[2] = 0xfe;	/* super side sector id */
	    sectbuff[3] = st;	/* first side sector in first group */
	    sectbuff[4] = ss;
	    if (writets (&f->filesys, sst, sss, sectbuff))
	    {
	      fclose (f->filesys.fsfile);
	    f->filesys.fsfile=0;
	      return (-1);
	    }
	  }
	  debug_msg ("REL file created and open\n");
	  return (0);
	}
      case cbm_DIR:
	{
	  int j, dbt, dbs;
	  /* check media type */
	  if (f->filesys.media != media_DHD)
	  {
	    /* only DHD's may have DIR's */
	    set_error (76, 0, 0);
	    fclose (f->filesys.fsfile);
	    f->filesys.fsfile=0;
	    return (-1);
	  }
	  sectbuff[i * 32 + 2] = ft | cbm_CLOSED;
	  sectbuff[i * 32 + 3] = tt;	/* start track */
	  sectbuff[i * 32 + 4] = ts;	/* start sector */
	  sectbuff[i * 32 + 30] = 0;
	  sectbuff[i * 32 + 31] = 0;	/* blocks in file */
	  /* filename (0xa0 padded) */
	  for (j = 0; name[j]; j++)
	  {
	    sectbuff[i * 32 + 5 + j] = name[j];
	  }
	  for (; j < 16; j++)
	    sectbuff[i * 32 + 5 + j] = 0xa0;
	  /* time/date stamp */
	  {
	    int y, m, d, h, mi, s;
	    gettimestamp (&y, &m, &d, &h, &mi, &s);
	    sectbuff[i * 32 + 23] = y % 100;
	    sectbuff[i * 32 + 24] = m;
	    sectbuff[i * 32 + 25] = d;
	    sectbuff[i * 32 + 26] = h;
	    sectbuff[i * 32 + 27] = mi;
	  }
	  /* write parent dir block back */
	  if (writets (&f->filesys, t, s, sectbuff))
	  {
	    fclose (f->filesys.fsfile);
	    f->filesys.fsfile=0;
	    return (-1);
	  }
	  /* allocate block for dir */
	  if (fs_dxx_findfreeblock (&f->filesys, &dbt, &dbs))
	  {
	    fclose (f->filesys.fsfile);
	    f->filesys.fsfile=0;
	    return (-1);
	  }
	  else
	  {
	    if (fs_dxx_allocateblock (&f->filesys, dbt, dbs))
	    {
	      /* file system is inconsistant! */
	      set_error (71, dbt, dbs);
	      fclose (f->filesys.fsfile);
	    f->filesys.fsfile=0;
	      return (-1);
	    }
	  }
	  for (j = 0; j < 256; j++)
	    sectbuff[j] = 0;
	  if (writets (&f->filesys, dbt, dbs, sectbuff))
	  {
	    fclose (f->filesys.fsfile);
	    f->filesys.fsfile=0;
	    return (-1);
	  }
	  /* read T1 S1 to modify for dir header */
	  if (readts (&f->filesys, 1, 1, sectbuff))
	  {
	    fclose (f->filesys.fsfile);
	    f->filesys.fsfile=0;
	    return (-1);
	  }
	  /* filename (0xa0 padded) */
	  for (j = 0; name[j]; j++)
	  {
	    sectbuff[4 + j] = name[j];
	  }
	  for (; j < 16; j++)
	    sectbuff[4 + j] = 0xa0;
	  /* pointer to entry in parent dir */
	  sectbuff[36] = t;
	  sectbuff[37] = s;
	  sectbuff[38] = 2 + (i * 32);	/* index to byte in block */
	  /* pointer to parent dir header */
	  sectbuff[34] = dirtrack;
	  sectbuff[35] = dirsect;
	  /* pointer to dir block */
	  sectbuff[0] = dbt;
	  sectbuff[1] = dbs;
	  /* write new dir header */
	  if (writets (&f->filesys, tt, ts, sectbuff))
	  {
	    fclose (f->filesys.fsfile);
	    f->filesys.fsfile=0;
	    return (-1);
	  }
	  fclose (f->filesys.fsfile);
	    f->filesys.fsfile=0;
	  return (0);

	  /* then clean up, and close with some daft message */
	  break;
	}
      }				/* end of switch(ft) */
    }
  }

  fclose (f->filesys.fsfile);
	    f->filesys.fsfile=0;
  return (-1);
}

int 
fs_dxx_finddirblock (uchar *path, int *dirtrack, int *dirsector, uchar *fname)
{
  /* find directory start track and sector (for a filesystem and subdirectories) */

  /* Step 1 - find section of path which points to a file */
  int i;
#ifdef AMIGA
  BPTR filelock;
  struct FileInfoBlock myFIB;
#else
  struct stat sb;
#endif
  path[strlen (path) + 1] = 0;	/* for poorly inserted '/'s */
  for (i = 0; i < strlen (path); i++)
    if (path[i] == '/')
    {
      path[i] = 0;
#ifdef AMIGA
      if ((filelock = Lock (path, ACCESS_READ)) != 0)
      {
	if (Examine (filelock, &myFIB))
	{
	  if (myFIB.fib_DirEntryType < 0)
	  {
	    path[i] = '/';
	    UnLock (filelock);
	  }
	  else
	  {
	    path[i] = '/';
	    UnLock (filelock);
	    break;
	  }
	}
	else
	  return (-1);
      }
      else
	return (-1);
#else
      stat ((char*)path, &sb);
      if (sb.st_mode != S_IFREG)
	path[i] = '/';
      else
      {
	path[i] = '/';
	break;
      }
#endif
    }

  path[i] = 0;
  path[i] = '/';

  if (path[i + 1])
  {
    uchar fspath[1024], glob[256], slag[256];
    strcpy ((char*)fspath, (char*)path);
    strcpy ((char*)slag, (char*)&path[i + 1]);

    /* now, do fs64_findfirst_g's to dissolve slag and
       deposit to path, until slag is gone or a path segment
       is rejected */
    while (slag[0])
    {
      int i;
      fs64_direntry de;
      de.dir = 0;
      glob[0] = 0;
      for (i = 0; i <= strlen (slag); i++)
	if ((slag[i] == '/') || (slag[i] == 0))
	{
	  /* strip from slag */
	  uchar temp[1024];
	  if (slag[i] == '/')
	    sprintf ((char*)temp, "%s", &slag[i + 1]);
	  else
	    temp[0] = 0;
	  strcpy ((char*)slag, (char*)temp);
	  break;
	}
	else
	  sprintf ((char*)glob, "%s%c", glob, slag[i]);
      /* now fs64_findfirst it */
      if (fs64_findfirst_g (fspath, glob, &de, dirtrack, dirsector))
      {
	/* failed! */
	*dirtrack = -1;
	*dirsector = -1;
	fs64_closefind_g (&de);
	set_error (39, 0, 0);
	return (-1);
      }
      else
      {
	/* success */
	fs64_closefind_g (&de);
	if (slag[0] == 0)
	{
	  /* copy first_track, first_sector from de */
	  *dirtrack = de.first_track;
	  *dirsector = de.first_sector;
	  strcpy ((char*)fname, (char*)de.filesys.fspath);
	  return (0);
	}
      }
    }				/* while(slag[0]) */
    *dirtrack = -1;
    *dirsector = -1;
    return (-1);
  }
  else
  {
    /* no sub-dir's below disk image */
    if (path[i] == '/')
      path[i] = 0;
    strcpy ((char*)fname, (char*)path);
    switch (fs64_mediatype (path))
    {
    case media_D64:
      *dirtrack = 18;
      *dirsector = 0;
      return (0);
    case media_D71:
      *dirtrack = 18;
      *dirsector = 0;
      return (0);
    case media_D81:
      *dirtrack = 40;
      *dirsector = 0;
      return (0);
    case media_DHD:
      *dirtrack = 1;
      *dirsector = 1;
      return (0);
    default:
      *dirtrack = -1;
      *dirsector = -1;
      return (-1);
    }
  }
}

int 
fs_dxx_getopenablename (fs64_file * f, fs64_direntry * de)
{
  strcpy ((char*)f->realname, (char*)de->fs);
  return (0);
}

int 
fs_dxx_openfile (fs64_file * f)
{
  /* open for read coz this is the open file routine,
     not create file */
  strcpy ((char*)f->filesys.fspath, (char*)f->realname);
  if ((f->filesys.fsfile = fopen ((char*)f->realname, "r")) == NULL)
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
fs_dxx_findfreeblock (fs64_filesystem * fs, int *track, int *sector)
{
  /* find a free block in a disk image */

  switch (fs->media)
  {
  case media_D64:
    return (fs_d64_findfreeblock (fs, track, sector));
  case media_D71:
    return (fs_d71_findfreeblock (fs, track, sector));
  case media_D81:
    return (fs_d81_findfreeblock (fs, track, sector));
  case media_DHD:
    return (fs_dhd_findfreeblock (fs, track, sector));
  default:
    set_error (74, 0, 0);
    *track = -1;
    *sector = -1;
    return (-1);
  }
}

int 
fs_dxx_allocateblock (fs64_filesystem * fs, int track, int sector)
{
  /* allocate a block */

  switch (fs->media)
  {
  case media_D64:
    return (fs_d64_allocateblock (fs, track, sector));
  case media_D71:
    return (fs_d71_allocateblock (fs, track, sector));
  case media_D81:
    return (fs_d81_allocateblock (fs, track, sector));
  case media_DHD:
    return (fs_dhd_allocateblock (fs, track, sector));
  default:
    set_error (38, 0, 0);
    return (-1);
  }
}

int 
fs_dxx_openfind (fs64_direntry * de, uchar *path, int *dt, int *ds)
{
  /* Dxx filesystem block */
  de->filesys.media = fs64_mediatype (path);

  /* copy path into the filesystem source descriptor */
  strcpy ((char*)de->fs, (char*)path);
  strcpy ((char*)de->realname, (char*)path);
  /* now lets open it */
  if ((de->filesys.fsfile = fopen ((char*)de->fs, "r+")) == NULL)
  {
    /* try readonly open */
    if ((de->filesys.fsfile = fopen ((char*)de->fs, "r")) == NULL)
    {
      /* open failed */
      /* 74,DRIVE NOT READY,00,00 */
      set_error (74, 0, 0);
      return (-1);
    }
  }
  /*
     de->track=18;
     de->sector=1;
   */
  /* get dir block nicely */
  {
    uchar temp[256];
    /* get header block */
    if (*dt < 1)
    {
      if (fs_dxx_finddirblock (de->fs, &de->track, &de->sector, temp))
	return (-1);
    }
    else
    {
      de->track = *dt;
      de->sector = *ds;
    }
    /* link to dir block */
    if (readts (&de->filesys, de->track, de->sector, temp))
      return (-1);
    de->track = temp[0];
    de->sector = temp[1];
  }
  de->intcount = 0;
  de->active = 1;

  return (0);
}

int 
fs_d47_headername (uchar *path, uchar *header, uchar *id, int par, fs64_file * f)
{
  uchar buff[256];
  fs64_filesystem ff;
  int i;
  ff.fsfile = 0;

  /* T18 S0 */
  /* have to be general since we are using the same routine
     for 1541 and 1571 */
  f->filesys.media = fs64_mediatype (path);
  f->de.filesys.media = f->filesys.media;
  ff.media = f->filesys.media;
  strcpy ((char*)ff.fspath, (char*)path);
  if ((ff.fsfile = fopen ((char*)path, "r")) != NULL)
  {
    if (!readts (&ff, 18, 0, buff))
    {
      for (i = 0; i < 16; i++)
	if (buff[i + 144] != 0xa0)
	  header[i] = buff[i + 144];
	else
	  header[i] = 0x20;
      for (i = 0; i < 5; i++)
	if (buff[i + 162] != 0xa0)
	  id[i] = buff[162 + i];
	else
	  id[i] = 0x20;
    }
    else
    {
      /* cant read sector - so make it up :) */
      strcpy ((char*)header, "@@@@@@@@@@@@@@@@");
      strcpy ((char*)id, "@@@@@");
    }
    fclose (ff.fsfile);
    ff.fsfile=0;
    return (0);
  }
  else
  {
    /* cant open file system */
    strcpy ((char*)header, "@@@@@@@@@@@@@@@@");
    strcpy ((char*)id, "@@@@@");
    return (0);
  }
}

int 
fs_dxx_readblock (fs64_file * f)
{
  /* all disk images are treated the same here,
     as they all have a similar block structure
   */

  /* read the block at curr_track & curr_sector,
     recording the sector link */
  if (f->curr_track < 1)
  {
    /* no more sectors, ergo eof */
    /* 80,END OF FILE,00,00 */
    set_error (80, 0, 0);
    return (-1);
  }
  else
  {
    /* there are sectors left */
    if (readts (&f->filesys, f->curr_track, f->curr_sector,
		f->buffer))
    {
      /* some error occured in reading that sector */
      f->be = 0;
      f->bp = 0;
      /* dont set error as readts will have */
      return (-1);
    }
    else
    {
      /* sucessfully read next sector */
      if (f->buffer[0] == 0)
      {
	/* there is no sector after this one */
	f->curr_track = -1;
	f->curr_sector = -1;
	f->bp = 2;
	//XXX +2 is one too much, it means position of last byte, but +2 would
	//point to next free byte in block
	f->be = f->buffer[1] + 1; 
      }
      else
      {
	/* this sector is full, so set be=256 
	   and copy sector links */
	f->curr_track = f->buffer[0];
	f->curr_sector = f->buffer[1];
	f->bp = 2;
	f->be = 256;
      }
      return (0);
    }				/* end of read sector bits */
  }				/* end of eof or not eof if then else */
}

int 
fs_dxx_writeblock (fs64_file * f)
{
  /* all disk images are treated the same here,
     as they all have a similar block structure
   */

  /* write the block at curr_track & curr_sector,
     recording the sector link */

  /* BUGS: This is just a copy of fs64_readblock at present! */

  int track, sector;

  if (f->bp < 256)
  {
    /* partial block */
    /* SO.. Dont allocate another block */
    /* set end of file block pointers */
    f->buffer[0] = 0;
    //XXX it is the pointer to the current and not to the next free byte, 
    //so we have to subtract 1 only, not 2!
    f->buffer[1] = (f->bp - 1); 
    
    f->blocks++;
  }
  else
  {
    /* full block, so allocate another
       and store pointers into first two bytes of the block
     */
    /* BUGS: Should not attempt to do anything if there are
       <2 free blocks, not 0. Perhaps this will be ok, as we
       allocate the sector before it is filled 
     */
    if (!fs64_findfreeblock (&f->filesys, &track, &sector))
    {
      /* there is a free block */
      f->buffer[0] = track;
      f->buffer[1] = sector;
      if (fs_dxx_allocateblock (&f->filesys, track, sector))
      {
	return (-1);
      }
      f->blocks++;
    }
    else
    {
      /* no free block */
      /* 72,PARTITION FULL,00,00 */
      set_error (72, 0, 0);
      return (-1);
    }

  }				/* end of if curr_track<1 else */

  if (writets (&f->filesys, f->curr_track, f->curr_sector,
	       f->buffer))
  {
    /* some error occured in writing that sector */
    f->be = 0;
    f->bp = 0;
    /* dont set error as writets will */
    return (-1);
  }
  else
  {
    /* sucessfully wrote sector */
    if (f->buffer[0] == 0)
    {
      /* there is no sector after this one */
      f->curr_track = -1;
      f->curr_sector = -1;
      f->bp = 2;
      //XXX see comments above: +1 not +2. TB
      f->be = f->buffer[1] + 1;
    }
    else
    {
      /* this sector is full, so set curr_block 
         and clear sector links */
      f->curr_track = f->buffer[0];
      f->curr_sector = f->buffer[1];
      /* set buffer things (be is irrelevant here) */
      f->bp = 2;
    }
    return (0);
  }				/* end of write sector bits */
}

int 
fs_dxx_scratchfile (fs64_direntry * de)
{
  /* Disk image - so remove the file entry 
     and then read and de-allocate each sector */
  /* de->intcount, de->track & de->sector reference 
     the directory entry */

  uchar buffer[256];

  /* read in dir sector */
  debug_msg ("Scratching [%s]\n", de->fs64name);
  if (!readts (&de->filesys, de->track, de->sector, buffer))
  {
    /* read in fine */

    /* check delete-ability */
    if (buffer[de->intcount * 32 - 30] & cbm_LOCKED)
    {
      /* file is locked */
      return (-1);
    }
    if (((buffer[de->intcount * 32 - 30] & 0x0f) == cbm_DIR) ||
	((buffer[de->intcount * 32 - 30] & 0x0f) == cbm_CBM) ||
	((buffer[de->intcount * 32 - 30] & 0x0f) == cbm_REL))
    {
      /* we cant delete these file types (yet) */
      return (-1);
    }

    /* delete dir entry */
    buffer[de->intcount * 32 - 30] = 0;
    if (writets (&de->filesys, de->track, de->sector, buffer))
    {
      /* error writing back dir */
      return (-1);
    }

    /* now deallocate sector chain */
    while (de->first_track)
    {
      if (!readts (&de->filesys, de->first_track, de->first_sector, buffer))
      {
	/* read in ok */
	fs64_deallocateblock (&de->filesys, de->first_track, de->first_sector);
	de->first_track = buffer[0];
	de->first_sector = buffer[1];
      }
      else
      {
	/* BAM is clobbered (1/2 deleted file) */
	set_error (71, 0, 0);
	return (-1);
      }
    }
    /* scratched sucessfully */
    return (0);
  }
  else
  {
    /* cant read directory */
    return (-1);
  }

}

int 
fs_dxx_bamalloc (fs64_filesystem * fs, int t, int s, void *bam)
{
  /* allocate a block on a bam */

  switch (fs->media)
  {
  case media_D64:
    return (fs_d64_bamalloc (t, s, (uchar *) bam));
  case media_D71:
    return (fs_d71_bamalloc (t, s, bam));
  case media_D81:
    return (fs_d81_bamalloc (t, s, bam));
  case media_DHD:
    return (fs_dhd_bamalloc (t, s, bam));
  default:
    debug_msg ("fs_dxx_bamalloc: unsupported media\n");
    set_error (38, 0, 0);
    return (-1);
  }
}

int 
fs_dxx_validate_dir (fs64_filesystem * fs, int purgeflag, void *bam, int t, int s)
{
  /* validate a subdirectory on a disk image */

  uchar dirbuff[256];		/* directory buffer */
  uchar blockbuff[256];		/* block buffer (for files) */
  int i;
  int dt, ds, odt, ods;
  int dirtouch = 0;		/* dir block dirty bit */

  set_error (0, 0, 0);

  dt = t;
  ds = s;
  odt = -1;
  ods = -1;
  while (dt)
  {
    /* allocate next dir sector */
    if (fs_dxx_bamalloc (fs, dt, ds, bam))
    {
      /* 71,directory error,dt,ds */
      if (!purgeflag)
      {
	set_error (71, dt, ds);
	return (-1);
      }
      else
      {
	/* remove crosslink 
	   1) Read block odt,ods
	   2) Modify link
	   3) Write
	   4) set error to 03,DIR X-LINK FIXED,dt,ds
	   5) Return (on a 1541 this indicates end of dir)
	 */
	if (readts (fs, odt, ods, dirbuff))
	  /* readts will have set error */
	  return (-1);
	dirbuff[0] = 0;
	dirbuff[1] = 0xff;
	if (writets (fs, odt, ods, dirbuff))
	  /* writets will have set error */
	  return (-1);
	set_error (03, dt, ds);
	return (0);
      }
    }
    /* read next dir sector */
    if (readts (fs, dt, ds, dirbuff))
    {
      /* readts will have set the error */
      return (-1);
    }
    /* go through each of the entries */
    dirtouch = 0;
    for (i = 0; i < 8; i++)
    {
      if (dirbuff[i * 32 + 2] & cbm_CLOSED)
      {
	/* valid file to check */
	switch (dirbuff[i * 32 + 2] & 0x0f)
	{
	case cbm_DIR:
	  /* validate this directory too! */
	  /* (first allocating its header block and passing the link) */
	  if (readts (fs, dirbuff[i * 32 + 3], dirbuff[i * 32 + 4], blockbuff))
	    return (-1);
	  if (fs_dxx_bamalloc (fs, dirbuff[i * 32 + 3], dirbuff[i * 32 + 4], bam))
	  {
	    int ft = dirbuff[i * 32 + 3];
	    int fse = dirbuff[i * 32 + 4];
	    /* allready allocated :( */
	    if (!purgeflag)
	    {
	      /* 75, FILE SYSTEM INCONSISTANT,ft,fs */
	      set_error (75, ft, fse);
	      return (-1);
	    }
	    else
	    {
	      /* scratch file */
	      dirbuff[i * 32 + 2] = 0x00;
	      dirtouch = 1;
	      /* 03,DIR X-LINK REMOVED */
	      set_error (03, ft, fse);
	      break;
	    }
	  }
	  else
	  {
	    /* block allocated */
// XXX fixme
//#warning Is it OK here?
//#warning Why arnt the following two vars used???
//	    int ft = dirbuff[i * 32 + 3];
//	    int fse = dirbuff[i * 32 + 4];
	  }
	  /* okay, use link, and recur! */
	  if (fs_dxx_validate_dir (fs, purgeflag, bam, blockbuff[0], blockbuff[1]))
	    return (-1);
	  break;
	case cbm_DEL:
	case cbm_USR:
	case cbm_SEQ:
	case cbm_PRG:
	  {
	    /* sequential sector file */
	    int oft = -1, ofs = -1;
	    int ft = dirbuff[i * 32 + 3];
	    int fse = dirbuff[i * 32 + 4];
	    while (ft)
	    {
	      if (fs_resolve_ts (fs->media, ft, fse) < 0)
	      {
		if (!purgeflag)
		{
		  /* 66,ILLEGAL BLOCK,ft,fs */
		  set_error (66, ft, fse);
		  return (-1);
		}
		else
		{
		  /* truncate or delete file */
		  if (oft > -1)
		  {
		    /* read prev block, modify and write */
		    if (readts (fs, oft, ofs, blockbuff))
		      return (-1);
		    blockbuff[0] = 0;
		    blockbuff[1] = 0xff;
		    if (writets (fs, oft, ofs, blockbuff))
		      return (-1);
		    /* 04,FILE X-LINK REMOVED */
		    set_error (04, ft, fse);
		    break;
		  }
		  else
		  {
		    /* scratch file */
		    dirbuff[i * 32 + 2] = 0x00;
		    dirtouch = 1;
		    /* 04,FILE X-LINK REMOVED */
		    set_error (04, ft, fse);
		    break;
		  }
		}
	      }
	      else
	      {
		/* valid block */
		if (fs_dxx_bamalloc (fs, ft, fse, bam))
		{
		  /* allready allocated :( */
		  if (!purgeflag)
		  {
		    /* 75, FILE SYSTEM INCONSISTANT,ft,fs */
		    set_error (75, ft, fse);
		    return (-1);
		  }
		  else
		  {
		    /* truncate or delete file */
		    if (oft > -1)
		    {
		      /* read prev block, modify and write */
		      if (readts (fs, oft, ofs, blockbuff))
			return (-1);
		      blockbuff[0] = 0;
		      blockbuff[1] = 0xff;
		      if (writets (fs, oft, ofs, blockbuff))
			return (-1);
		      /* 04,FILE X-LINK REMOVED */
		      set_error (04, ft, fse);
		      break;
		    }
		    else
		    {
		      /* scratch file */
		      dirbuff[i * 32 + 2] = 0x00;
		      dirtouch = 1;
		      /* 04,FILE X-LINK REMOVED */
		      set_error (04, ft, fse);
		      break;
		    }
		  }
		}
		else
		{
		  /* block allocated */
		}
		/* link to next sector */
		if (readts (fs, ft, fse, blockbuff))
		{
		  return (-1);
		}
		oft = ft;
		ofs = fse;
		ft = blockbuff[0];
		fse = blockbuff[1];
	      }			/* else from if(ft_resolve_...  */
	    }			/* while (ft) */
	    break;
	  }
	case cbm_REL:
	  /* eeww... a REL file */
	  debug_msg ("Mangling some REL file\n");
	}

      }
      else
      {
	/* toast the dir entry */
	if (dirbuff[i * 32 + 2])
	{
	  dirbuff[i * 32 + 2] = cbm_DEL;
	  dirtouch = 1;
	}
      }
    }
    /* write dir entry back */
    if (dirtouch)
    {
      /* dir block is dirty so write back */
      if (writets (fs, dt, ds, dirbuff))
	/* writets will have set the error */
	return (-1);
    }
    /* link to next dir sector */
    dt = dirbuff[0];
    ds = dirbuff[1];
  }

  return (0);
}

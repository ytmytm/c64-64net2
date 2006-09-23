/*
   64NET/2 File-i/o module
   (C)Copyright Paul Gardner-Stephen 1995, All rights reserved
   File open,close,read and write routines for 64net/2
 */

#include "config.h"
#include "fs.h"
#include "fs_func.h"
#include "client-comm.h"
#include "comm-rrnet.h"

#include "fs_rawdir.h"

/* open file count */
static int of_count = 0;

int
fs_pathtofilesystem (fs64_filesystem * fs, uchar *path)
{
  /* construct filesystem (or abort) */
  int mt = fs64_mediatype (path);
  strcpy ((char*)fs->fspath, (char*)path);
  fs->media = mt;
  switch (mt)
  {
  case media_D64:
  case media_D71:
  case media_D81:
  case media_DHD:
    {
      /* disk image */
      switch (mt)
      {
      case media_D64:
	fs->dirtrack = 18;
	fs->dirsector = 0;
	break;
      case media_D71:
	fs->dirtrack = 18;
	fs->dirsector = 0;
	break;
      case media_D81:
	fs->dirtrack = 40;
	fs->dirsector = 0;
	break;
	/* BUG: The following will not handle sub-directories correctly */
      case media_DHD:
	fs->dirtrack = 1;
	fs->dirsector = 1;
	break;
      }
      if ((fs->fsfile = fopen ((char*)path, "r+")) == NULL)
      {
	/* Try readonly */
	if ((fs->fsfile = fopen ((char*)path, "r")) == NULL)
	  {
	    debug_msg("Error reason: Unable to open path (=%s) for mode r+. errno=%d\n",
		      path,errno);
	    set_error (74, 0, 0);
	    fs->fsfile = 0;
	    return (-1);
	  }
      }
      break;
    }
  case media_T64:
  case media_LNX:
    {
      /* Copy name, and open file  */
      if ((fs->fsfile = fopen ((char*)path, "r+")) == NULL)
      {
	/* Try readonly */
	if ((fs->fsfile = fopen ((char*)path, "r")) == NULL)
	  {
	    debug_msg("Error reason: Unable to open path (=%s) for mode r+. errno=%d\n",
		      path,errno);
	    set_error (74, 0, 0);
	    fs->fsfile = 0;
	    return (-1);
	  }
      }
      break;
    }
  case media_NET:
  case media_UFS:
    {
      /* copy name (already done!)  */
      break;
    }
  default:
    debug_msg("Error reason: Unknown media type\n");
    set_error (74, 00, 00);
    fs->fsfile = 0;
    return (-1);
  }
  return (0);
}

int
fs64_create_g (uchar *path, uchar *glob, fs64_file * f, int *dirtrack, int *dirsect)
{
  /* create a file for writing to */
  uchar globs[17][32] =
  {
    {0}};
  int t, dt, ds;

  /* prepare to extract info */
  parse_glob (globs, glob);

  for (t = 1; t < 15; t++)
    if (globs[0][t])
      break;

  if (t == 15)
  {
    /* type mismatch error - file with no type */
    debug_msg("Error reason: Type mismatch; file with no type\n");
    set_error (64, 0, 0);
    return (-1);
  }

  /* create the file */
  switch (fs64_mediatype (path))
  {
    /* globs[0][31] = rel length */
  case media_UFS:
    return (fs_ufs_createfile (path, globs[1], t, globs[0][31], f));
  case media_D64:		/* return(fs_dxx_createfile(path,globs[1],t,f,18,0,media_D64)); */
  case media_D71:		/* return(fs_dxx_createfile(path,globs[1],t,f,18,0,media_D71)); */
  case media_D81:		/* return(fs_dxx_createfile(path,globs[1],t,f,40,0,media_D81)); */
  case media_DHD:		/* return(fs_dxx_createfile(path,globs[1],t,f,1,1,media_DHD)); */
    {
      uchar fname[1024] =
      {0};
      /* find directory track/sector */
      if (*dirtrack < 1)
	fs_dxx_finddirblock (path, &dt, &ds, fname);
      else
      {
	dt = *dirtrack;
	ds = *dirsect;
	strcpy ((char*)fname, (char*)path);
      }
      /* create file (globs[0][31] = rel length */
      return (fs_dxx_createfile (fname, globs[1], t, globs[0][31], f, dt, ds, fs64_mediatype (path)));
    }
  case media_LNX:
    return (fs_lnx_createfile (path, globs[1], t, f));
  case media_NET:
    return (fs_net_createfile (path, globs[1], t, f));
  default:
    debug_msg("Error reason: Unknown or invalid media type\n");
    set_error (74, 0, 0);
    return (-1);
  }

}

int
fs64_scratchfile (fs64_direntry * de)
{
  /* delete the file currently pointed to in the de */

  /* FILESYSTEM_SPECIFIC */
  switch (de->filesys.media)
  {
  case media_UFS:
    return (fs_ufs_scratchfile (de));
    break;
  case media_D64:
  case media_D71:
  case media_D81:
  case media_DHD:
    return (fs_dxx_scratchfile (de));
    break;
  case media_LNX:
    return (fs_lnx_scratchfile (de));
    break;
  case media_NET:
    return (fs_net_scratchfile (de));
    break;
  default:
    /* unknown media */
    /* 74,DRIVE NOT READY,00,00 */
    debug_msg("Error reason: Unknown or invalid media type\n");
    set_error (74, 0, 0);
    return (-1);
  }
}

int fs64_scratchfile_g (uchar *filespec) {
	/* scratch a set of files */
	fs64_direntry de;
	uchar path[1024], glob[256];
	int dirflag, mode, replace, par;
	int nf = 0, flag = 0, dt = -1, ds = -1;
	de.dir = 0;

	/* parse things out */
	if (fs64_parse_filespec (filespec, path, glob, &dirflag, &mode, &replace, &par, &dt, &ds)) {
		/* error of some sort */
		return (-1);
	}

	if (!fs64_findfirst_g (path, glob, &de, &dt, &ds)) {

		/* okay.. scratch away! */
		while (!flag) {
			if (!fs64_scratchfile (&de)) {
				nf++;
			}
			else {
				/* couldnt scratch file */
				debug_msg("Error reason: Unable to scratch file\n");
				set_error (1, nf, 0);
				return (0);
			}
			flag = fs64_findnext_g (&de);
		}
		/* done */
		set_error (1, nf, 0);
		return (0);
	}
	else {
		/* no files or an error */
		/* file not found error will do */
		debug_msg("Error reason: No files to scratch, or mysterious error\n");
		set_error (62, 0, 0);
		return (-1);
	}
}

int
fs64_dirtail (fs64_file * f)
{
  /* do the BLOCKS FREE. line */
  long b;
  int i;

  /* line link */
  f->buffer[f->be++] = (f->curr_poss + 0x1e) & 0xff;
  f->buffer[f->be++] = (f->curr_poss + 0x1e) >> 8;
  f->curr_poss += 0x1e;

  /* blocks */
  b = f->blocksfree;
  if (b > 65535)
    b = 65535;
  f->buffer[f->be++] = b & 0xff;
  f->buffer[f->be++] = b >> 8;

  /* "BLOCKS FREE.             " */
  for (i = 0; i < (int)strlen ("BLOCKS FREE.             "); i++)
    f->buffer[f->be++] = "BLOCKS FREE.             "[i];

  /* terminating zeros */
  f->buffer[f->be++] = 0;
  f->buffer[f->be++] = 0;
  f->buffer[f->be++] = 0;

  /* this means we can close the find now! */
  fs64_closefind_g (&f->de);

  return (0);
}



int
fs64_dirheader (fs64_file * f, int par, uchar *label, uchar *id)
{
  int i;

  /* load link */
  f->buffer[f->be++] = (f->curr_poss) & 0xff;
  f->buffer[f->be++] = (f->curr_poss) >> 8;

  /* line link */
  f->buffer[f->be++] = (f->curr_poss + 0x1e) & 0xff;
  f->buffer[f->be++] = (f->curr_poss + 0x1e) >> 8;
  f->curr_poss += 0x1e;

  /* line number = partition */
  f->buffer[f->be++] = par & 0xff;
  f->buffer[f->be++] = par >> 8;

  /* reverse on */
  f->buffer[f->be++] = 0x12;

  /* quote */
  f->buffer[f->be++] = 0x22;

  /* 16 chars of par */
  for (i = 0; i < 16; i++)
    if (i < (int)strlen (label))
      f->buffer[f->be++] = label[i];
    else
      f->buffer[f->be++] = 0x20;

  /* quote */
  f->buffer[f->be++] = 0x22;

  /* space */
  f->buffer[f->be++] = 0x20;

  /* id */
  for (i = 0; i < 5; i++)
    if (i < (int)strlen (id))
      f->buffer[f->be++] = id[i];
    else
      f->buffer[f->be++] = 0x20;

  /* terminating zero */
  f->buffer[f->be++] = 0;

  return (0);
}				/* fs64_dirheader */

int
fs64_direntry2block (fs64_file * f)
{
  /* convert the current direntry into a basic programme fragment */
  int qf = 0, i;

  /* line link */
  f->buffer[f->be++] = (f->curr_poss + 0x20) & 0xff;
  f->buffer[f->be++] = (f->curr_poss + 0x20) >> 8;
  f->curr_poss += 0x20;

  /* blocks */
  if (f->de.blocks < 65536)
  {
    /* <65536 blocks */
    f->buffer[f->be++] = (f->de.blocks) & 0xff;
    f->buffer[f->be++] = (f->de.blocks) >> 8;
  }
  else
  {
    /* 65536+ blocks */
    f->buffer[f->be++] = 0xff;
    f->buffer[f->be++] = 0xff;
  }

  /* spaces */
  if (f->de.blocks < 1000)
    f->buffer[f->be++] = 0x20;
  if (f->de.blocks < 100)
    f->buffer[f->be++] = 0x20;
  if (f->de.blocks < 10)
    f->buffer[f->be++] = 0x20;

  /* "filename" */
  f->buffer[f->be++] = 0x22;
  qf = 0;
  for (i = 0; i < 16; i++)
    if (f->de.fs64name[i] != 0xa0)
      f->buffer[f->be++] = f->de.fs64name[i];
    else
    {
      /* do a quote and print the rest of the chars */
      f->buffer[f->be++] = 0x22;
      qf = 1;
      for (; i < 16; i++)
	if (f->de.fs64name[i] != 0xa0)
	  f->buffer[f->be++] = f->de.fs64name[i];
	else
	  f->buffer[f->be++] = 0x20;
    }
  if (qf == 0)
    f->buffer[f->be++] = 0x22;

  /* splat or closed */
  if (f->de.filetype & cbm_CLOSED)
    f->buffer[f->be++] = 0x20;
  else
    f->buffer[f->be++] = '*';

  /* file type */
  switch (f->de.filetype & 0x0f)
  {
  case cbm_DEL:
    f->buffer[f->be++] = 'D';
    f->buffer[f->be++] = 'E';
    f->buffer[f->be++] = 'L';
    break;
  case cbm_SEQ:
    f->buffer[f->be++] = 'S';
    f->buffer[f->be++] = 'E';
    f->buffer[f->be++] = 'Q';
    break;
  case cbm_PRG:
    if(f->de.filesys.arctype==arc_N64) {
      f->buffer[f->be++] = 'P';
      f->buffer[f->be++] = 'R';
      f->buffer[f->be++] = 'G';
    }
    else {
      f->buffer[f->be++] = 'P';
      f->buffer[f->be++] = 'R';
      f->buffer[f->be++] = 'G';
    }
    break;
  case cbm_USR:
    f->buffer[f->be++] = 'U';
    f->buffer[f->be++] = 'S';
    f->buffer[f->be++] = 'R';
    break;
  case cbm_REL:
    f->buffer[f->be++] = 'R';
    f->buffer[f->be++] = 'E';
    f->buffer[f->be++] = 'L';
    break;
  case cbm_CBM:
    f->buffer[f->be++] = 'C';
    f->buffer[f->be++] = 'B';
    f->buffer[f->be++] = 'M';
    break;
  case cbm_DIR:
    f->buffer[f->be++] = 'D';
    f->buffer[f->be++] = 'I';
    f->buffer[f->be++] = 'R';
    break;
  case cbm_UFS:
    f->buffer[f->be++] = 'U';
    f->buffer[f->be++] = 'F';
    f->buffer[f->be++] = 'S';
    break;
  case cbm_NET:
    f->buffer[f->be++] = 'N';
    f->buffer[f->be++] = 'E';
    f->buffer[f->be++] = 'T';
    break;
  default:
    f->buffer[f->be++] = '?';
    f->buffer[f->be++] = '?';
    f->buffer[f->be++] = '?';
    break;
  }
  if (f->de.filetype & cbm_LOCKED)
    f->buffer[f->be++] = '<';
  else
    f->buffer[f->be++] = ' ';

  /* last spaces */
  f->buffer[f->be++] = ' ';
  if (!(f->de.blocks < 1000))
    f->buffer[f->be++] = 0x20;
  if (!(f->de.blocks < 100))
    f->buffer[f->be++] = 0x20;
  if (!(f->de.blocks < 10))
    f->buffer[f->be++] = 0x20;

  /* terminating zero */
  f->buffer[f->be++] = 0;

  return (0);
}


/* 
   fs64_readchar, fs64_writechar - file character io 
 */

int fs64_unreadchar(fs64_file *f, uchar *c)
{
  /* push the character back onto the buffer.
     This is only guaranteed to work for one char */

  if (f->open != 1) {
	  set_error(61,0,0);
	  return -1;
  }
  if (f->char_queuedP) return -1;

  f->queued_char=*c;
  f->char_queuedP=1;

  return 0;
}

int fs64_readchar (fs64_file * f, uchar *c) {
	/* read a character from the file */


	if (f->open != 1) {
		/* if eof or otherwise, be 1541 bug compatible */
		*c = 199;
		set_error(61,0,0);
		return (-1);
	}

	/* Re-send queued character */
	if (f->char_queuedP) {
		f->char_queuedP=0;
		*c = f->queued_char;
		return 0;
	}

	if (f->isbuff) {
		/* buffer read */
		*c = f->buffer[f->bp];
		(f->bp) += 1;
		if (f->bp > 255) f->bp = 0;
		return (0);
	}

	/* FILESYSTEM_SPECIFIC */
	switch (f->filesys.media) {
		case media_NET:
		return (fs_net_readchar (f, c));
	}

	if (f->mode != mode_READ) {
		/* if it isnt a readable file, be 1541 bug compatible */
		*c = 199;
		return (-1);
	}

	if (f->bp >= f->be) {
		if(fs64_readblock(f)==0) {
			//printf("readblock-success\n");
		}
		else {
			//printf("readblock-failed\n");
			return -1;
		}
	}
	*c = f->buffer[f->bp++];
	return 0;
}

int
fs64_writechar (fs64_file * f, uchar c)
{
  /* write a character to the file */

  if (f->open != 1)
  {
    /* if eof or otherwise, be 1541 bug compatible */
    c = 199;
    return (-1);
  }

  if (f->isbuff)
  {
    /* buffer write */
    f->buffer[f->bp] = c;
    (f->bp) += 1;
    if (f->bp > 255)
      f->bp = 0;
    return (0);
  }

  /* special processing for non-block streams */
  /* FILESYSTEM_SPECIFIC */
  switch (f->filesys.media)
  {
  case media_NET:
    return (fs_net_writechar (f, c));
  }

  if (f->mode != mode_WRITE)
  {
    /* if it isnt a writeable file, be 1541 bug compatible */
    c = 199;
    return (-1);
  }

  /* check if there is room in the buffer */
  if (f->bp > 255)
  {
    /* buffer is empty - so read next block */
    if (fs64_writeblock (f))
    {
      /* file io error - error will be set */
      c = 199;
      return (-1);
    }
  }

  if (f->bp > 255)
  {
    /* still full - must be bad */
    return (-1);
  }

  /* make sure buffer is set right */
  if (f->bp < 2)
    f->bp = 2;
  /* add char */
  f->buffer[f->bp++] = c;
  return (0);

}

/*
   fs64_readblock, fs64_writeblock - file block movements
 */

int
fs64_writeblock (fs64_file * f)
{
  /* read the next block from a file, and return 0 if successful, or
     -1 if end of file 
   */

  if (f->mode != mode_WRITE)
  {
    /* ignore stupid/impossible accesses */
    debug_msg("Trying to write to read only file\n");
    return (-1);
  }

  /* FILESYSTEM_SPECIFIC */
  switch (f->filesys.media)
  {
  case media_UFS:
    return (fs_ufs_writeblock (f));
    break;
  case media_T64:
    return (fs_t64_writeblock (f));
    break;
  case media_D64:
    return (fs_dxx_writeblock (f));
    break;
  case media_D71:
    return (fs_dxx_writeblock (f));
    break;
  case media_D81:
    return (fs_dxx_writeblock (f));
    break;
  case media_DHD:
    return (fs_dxx_writeblock (f));
    break;
  case media_LNX:
    return (fs_lnx_writeblock (f));
    break;
  case media_NET:
    return (fs_net_writeblock (f));
    break;
  default:
    /* unknown media */
    /* 74,DRIVE NOT READY,00,00 */
    debug_msg("Error reason: Unknown or invalid media type\n");
    set_error (74, 0, 0);
    return (-1);
  }				/* end of media type switch */
}				/* end of fs64_writeblock() */


int
fs64_readblock (fs64_file * f)
{
  /* read the next block from a file, and return 0 if successful, or
     -1 if end of file 
   */

  if (f->mode != mode_READ)
  {
    /* ignore silly accesses! */
    return (-1);
  }

  if (f->isdir >= 1)
  {
    /* its a directory read - *easy* :) */
    if ((f->isdir == 2) && ((talklf[curr_client] & 0x0f)==0))
    {
      /* EOF */
      f->open = 0;
      return (-1);
    }
    if (!fs64_findnext_g (&f->de))
    {
      /* theres an entry */
      f->bp = 0;
      f->be = 0;
      if ((talklf[curr_client] & 0x0f)==0) {
        fs64_direntry2block (f); }
      else {
        fs64_direntry2rawentry(f);
	f->isdir++;
	if (f->isdir==9) {
	    f->isdir=1;
	 } else {
	    f->buffer[f->be++] = 0;
	    f->buffer[f->be++] = 0;
	    }
      }
    }
    else
    {
	if ((talklf[curr_client] & 0x0f)==0) {
          /* blocks free */
	    f->bp = 0;
	    f->be = 0;
	    fs64_dirtail (f);
	    f->isdir = 2; }
	else {
	    f->open=0;
	    return(-1);
	    }
    }
    return (0);
  }

  /* FILESYSTEM_SPECIFIC */
  switch (f->filesys.media)
  {
  case media_UFS:
    return (fs_ufs_readblock (f));
    break;
  case media_T64:
    return (fs_t64_readblock (f));
    break;
  case media_D64:
    return (fs_dxx_readblock (f));
    break;
  case media_D71:
    return (fs_dxx_readblock (f));
    break;
  case media_D81:
    return (fs_dxx_readblock (f));
    break;
  case media_DHD:
    return (fs_dxx_readblock (f));
    break;
  case media_LNX:
    return (fs_lnx_readblock (f));
    break;
  case media_NET:
    return (fs_net_readblock (f));
    break;
  default:
    /* unknown media */
    /* 74,DRIVE NOT READY,00,00 */
    return (-1);
  }				/* end of media type switch */
}				/* end of fs64_readblock() */


/*
   fs64_openfile_g, fs64_closefile_g - Open and close files from a direntry, but
   with globbing of the filename
 */

int
fs64_openfile_g (uchar *curdir, uchar *filespec, fs64_file * f)
{
  fs64_direntry de;
  int dirflag, mode, replace, par, i, dirtrack = -1, dirsect = -1;
  uchar path[1024], header[16];
  uchar glob[256], id[5];
  de.dir = 0;

  /* INCOMPLETE - In the end this must have all 8 possible
     open types implemented */

  /* silly place to set it, but oh well */
  f->isdir = 0;
  f->isbuff = 0;

  /* parse out some important info! */
  /* and return a path, glob , dirflag, and mode value for the open request */
  //debug_msg ("openfile_g: running fs64_parse_filespec\n");
  if (fs64_parse_filespec (filespec, path, glob, &dirflag, &mode, &replace, &par, &dirtrack, &dirsect))
  {
    /* an error occurred */
    /* parse_filespec will have set the error appropriately */
    return (-1);
  }
  //debug_msg ("openfile_g: fs64_parse_filespec completed (T%d S%d)\n", dirtrack, dirsect);

  client_activity (1);

  /* is if a buffer access? */
  if (glob[0] == '#')
  {
    /* Buffer Access */
    switch (fs64_mediatype (curr_dir[curr_client][curr_par[curr_client]]))
    {
    case media_D64:
    case media_D71:
    case media_D81:
    case media_DHD:
      f->filesys.media = fs64_mediatype (curr_dir[curr_client][curr_par[curr_client]]);
      strcpy ((char*)f->filesys.fspath, (char*)curr_dir[curr_client][curr_par[curr_client]]);
      if ((f->filesys.fsfile = fopen ((char*)path, "r+")) == NULL)
      {
	/* Open readonly if can't do so read write */
	if ((f->filesys.fsfile = fopen ((char*)path, "r")) == NULL)
	  {
	    debug_msg("Error reason: Unable to open path (=%s) for mode r+. Errno = %d\n",
		      path,errno);
	    set_error (74, 0, 0);
	    return (-1);
	  }
      }
      break;
    default:
      debug_msg("Error reason: Wrong media type for operation\n");
      set_error (76, 0, 0);
      return (-1);
    }
    debug_msg ("Buffer open\n");
    /* setup the fileentry for a directory list, not a "normal" open */
    /* its a dir! */
    f->isbuff = 1;
    /* set load address for converting dir entries */
    f->bp = 0;
    f->mode = mode_WRITE;
    /* open physical filesystem */
    f->open = 1;
    set_error (0, 0, 0);
    return (0);
  }

  /* READ from a normal file */
  if ((mode == mode_READ) && (dirflag == 0))
  {
    /* do a fs64_findfirst_g to locate the file, then a normal open */
    //debug_msg("Calling findfirst_g with: path=%s, glob=%s\n",path,glob);
    if (!fs64_findfirst_g (path, glob, &de, &dirtrack, &dirsect))
    {
      DIR *foo;
      /* ok, we got a file! */
      int rv;
      foo = de.dir;
      rv = fs64_openfile (&de, f);
      //debug_msg ("foo: %d, dir %d\n", (int) foo, (int) de.dir);
      fs64_closefind_g (&de);
      //debug_msg ("Just closed a dir in openfile_g\n");
      if (!rv)
      {
	/* notify client, if necessary */
	of_count++;
	if (of_count == 1)
	  client_activity (1);
      }
      else
	client_activity (of_count);
      return (rv);
    }
    else
    {
      /* no go.. presume not found */
      /* 62,FILE NOT FOUND,00,00 */
      fs64_closefind_g (&de);
      debug_msg("Error reason: fs64_findfirst_g (%s,%s,&de,%d,%d) failed\n",
		path, glob,dirtrack,dirsect);
  //    set_error (62, 0, 0);
      client_activity (of_count);
      return (-1);
    }
  }

  if ((mode == mode_WRITE) && (dirflag == 0))
  {
    /* WRITE to a normal file */

    /* check for * or ? in glob
       BUT: only upto a comma, so REL record sizes can still work */
    for (i = 0; i < (int)strlen (glob); i++)
    {
      if (glob[i] == ',')
	break;
      if ((glob[i] == '*') || (glob[i] == '?'))
      {
	/* 33, SYNTAX ERROR,00,00 */
	debug_msg("Error reason: wildcard in file open for write\n");
	set_error (33, 0, 0);
	return (-1);
      }
    }

    /* is it save with replace? */
    if (replace)
    {
      /* scratch file first */
      client_activity (1);
      if (fs64_scratchfile_g (filespec))
      {
	/* some sort of error - fs64_scratchfile_g will
	   have got the error set */
	client_activity (of_count);
	//return (-1);
	//XXX what if the file didn't exist like save"@:foo",8 
	//we still have to create it then, right?
      }
    }

    client_activity (1);

    /* does the file exist already? */
    if (!fs64_findfirst_g (path, glob, &de, &dirtrack, &dirsect))
    {
      /* the file already exists! */
      /* close the filefind  and bail out */
      while (!fs64_findnext_g (&de));
      /* 63, FILE EXISTS,00,00 */
      debug_msg("Error reason: File exists\n");
      set_error (63, 0, 0);
      client_activity (of_count);
      return (-1);
    }

    /* ok.. create the file! */
    debug_msg ("Path: [%s] glob [%s]\n", path, glob);
    if (fs64_create_g (path, glob, f, &dirtrack, &dirsect))
    {
      /* some evil error has occurred */
      return (-1);
    }

    of_count++;

    /* WOAH! finally the deed is done */
    return (0);
  }


  /* READ/WRITE from a DIRECTORY  (replace & mode irrelevant) */
  /* The 1541 will *always* read from a $ file openned */
  if (dirflag)
  {
    client_activity (1);
    /* setup the fileentry for a directory list, not a "normal" open */
    /* its a dir! */
    f->isdir = 1;
    /* set load address for converting dir entries */
    f->curr_poss = 0x0401;
    f->bp = 256;
    if (par == 0)
      par = curr_par[curr_client];
    /* setup de for search */
    //debug_msg ("openfile_g: running openfind_g\n");
    //debug_msg("Calling findfirst_g with: path=%s, glob=%s,$\n",path,glob);
    
    //no glob given vor dir? Bad, else we'll get zero entries, so we wildcard, Toby
    if(strlen(glob)==0) { glob[0]='*'; glob[1]=0; }
    if (fs64_openfind_g (path, (uchar*)strcat ((char*)glob, ",$"), &f->de, &dirtrack, &dirsect))
    {
      /* cant open directory */
      /* error will have been set by findfirst */
      /* dont return though.. just keep going and do an empty dir */
      client_activity (of_count);
      return (-1);
    }
    else
    {
      /* joy! - lets to it */
      fs64_filesystem fs;
      fs.fsfile = 0;
     // debug_msg ("openfile_g: openfind_g succeeded\n");
      if (fs_pathtofilesystem (&fs, path))
      {
	return (-1);
      }
      f->filesys.dirtrack = dirtrack;
      f->filesys.dirsector = dirsect;
      //debug_msg ("fs64_openfile_g() mt: %d\n", de.filesys.media);
      f->blocksfree = fs64_blocksfree (&fs);
      if (fs.fsfile)
      {
//	debug_msg ("Trying to close a file: %d\n", (int) fs.fsfile);
	fclose (fs.fsfile);
	fs.fsfile = 0;
      }
      f->bp = 0;
      f->be = 0;
      f->mode = mode_READ;
      f->open = 1;
      /* figure out partition #, label & id */
      /* FILESYSTEM_SPECIFIC */
      switch (fs64_mediatype (path))
      {
      case media_UFS:
	fs_ufs_headername (path, header, id, par);
	break;
      case media_D64:
	fs_d47_headername (path, header, id, par, f);
	break;
      case media_D71:
	fs_d47_headername (path, header, id, par, f);
	break;
      case media_D81:
	fs_d81_headername (path, header, id, par, f);
	break;
      case media_DHD:
	fs_dhd_headername (path, header, id, par, f);
	break;
      case media_LNX:
	fs_lnx_headername (path, header, id, par);
	break;
      case media_NET:
	fs_net_headername (path, header, id, par);
	break;
      default:
	strcpy ((char*)header, "-=-  64NET/2 -=-");
	strcpy ((char*)id, "64NET");
      }
      /* PGS - 20050516 - Why would we ever want to send T18S0 in raw
	 format?  Also, the talklf test here is wrong, as in PIEC the 
	 logical file gets set after, not before.  So just disable all
	 this and format the header always. */
      /* output header */
      fs64_dirheader (f, par, header, id);
      if (0) 
	{
	  if (((talklf[curr_client] & 0x0f)==0)||(talklf[curr_client]<0)) {
	    fs64_dirheader (f, par, header, id); }
	  else {
	    printf("Raw dir header: talklf = %d\n",talklf[curr_client]);
	    fs64_rawdirheader (f, par, header, id); }
	}
      /* all's well - so off we go */
      client_activity (++of_count);
      return (0);
    }
  }

  /* not implemented - bad filename / mode combo */
  /* 33,SYNTAX ERROR,00,00 */
  debug_msg("Error reason: Bad filename/mode combination (or an unimplemented one)\n");
  set_error (33, 0, 0);
  client_activity (of_count);
  return (-1);
}

int
fs64_closefile_g (fs64_file * f)
{
  /* fs64_closefile_g is just the normal close */

  if (of_count)
    of_count--;
  if (!of_count)
    client_activity (0);
  return (fs64_closefile (f));
}

/*
   fs64_openfile, fs64_closefile - Open and close files from a direntry
 */

/* open a file referenced by de */
int
fs64_openfile (fs64_direntry * de, fs64_file * f)
{
  /* BUGS: This routine hasnt been tested yet. */
  int i;

  /* file not open until all things done */
  f->open = 0;
  f->isdir = 0;
  f->char_queuedP=0;

  /* is de an active entry? */
  if (de->active != 1)
  {
    /* not really a DOS error, and shouldnt happen */
    return (-1);		/* de does not reference a file */
  }

  /* is it an openable file type? (ie not cbm_DIR or cbm_CBM) */
  if (((de->filetype & 0x0f) == cbm_DIR) || ((de->filetype & 0x0f) == cbm_CBM))
  {
    /* its a bad'un! */
    /* 64,FILE TYPE MISMATCH,00,00 */
	  //XXX DIR CHECK, TOBY
    debug_msg("Error reason: Tried to open a cbm_DIR or cbm_CBM file\n");
    set_error (64, 0, 0);
    return (-2);		/* references an `unopenable' */
  }

  /* OK, its a go-er! */
  /* copy relevant fields over */
  for (i = 0; i < 16; i++)
    f->fs64name[i] = de->fs64name[i];
  for (i = 0; i < 1024; i++)
    f->realname[i] = de->realname[i];
  f->filesys = de->filesys;
/*  f->arctype=de->arctype;
   f->filetype=de->filetype; */
  f->first_track = de->first_track;
  f->first_sector = de->first_sector;
  f->first_poss = de->binbase;
  f->blocks = de->blocks;	/* useless field coz it can be lying */
  f->realsize = de->realsize;

  /* initialize other relevant fields */
  f->curr_track = f->first_track;
  f->curr_sector = f->first_sector;
  f->curr_poss = f->first_poss;
  f->bp = 0;
  f->be = 0;
  f->mode = mode_READ;		/* fs64_createfile to open a write file */

  /* get name/structure for openable thing */
  switch (f->filesys.media)
  {
  case media_T64:
    fs_t64_getopenablename (f, de);
    break;
  case media_D64:
    fs_dxx_getopenablename (f, de);
    break;
  case media_D71:
    fs_dxx_getopenablename (f, de);
    break;
  case media_D81:
    fs_dxx_getopenablename (f, de);
    break;
  case media_DHD:
    fs_dxx_getopenablename (f, de);
    break;
  case media_UFS:
    fs_ufs_getopenablename (f, de);
    break;
  case media_LNX:
    fs_lnx_getopenablename (f, de);
    break;
  case media_NET:
    fs_net_getopenablename (f, de);
    break;
  default:
    /* oh dear! */
    return (-1);
  }

  /* open file */
  switch (f->filesys.media)
  {
  case media_T64:
    return (fs_t64_openfile (f));
    break;
  case media_D64:
    return (fs_dxx_openfile (f));
    break;
  case media_D71:
    return (fs_dxx_openfile (f));
    break;
  case media_D81:
    return (fs_dxx_openfile (f));
    break;
  case media_DHD:
    return (fs_dxx_openfile (f));
    break;
  case media_UFS:
    return (fs_ufs_openfile (f));
    break;
  case media_LNX:
    return (fs_lnx_openfile (f));
    break;
  case media_NET:
    return (fs_net_openfile (f));
    break;
  default:
    return (-1);
  }

}

int
fs64_closefile (fs64_file * f)
{
  int i;

  if (!f->open)
  {
    /* The file wasnt open to begin with! */
    return (0);
  }
  else
  {
    /* close fsfile */

    /* if it is a write file, write last block of data */
    if ((f->bp > 2) && (f->mode == mode_WRITE))
    {
      /* there is data to write */
      if (!f->isbuff)
      {
	fs64_writeblock (f);
	switch (f->filesys.media)
	{
	case media_D64:
	case media_D71:
	case media_D81:
	case media_DHD:
	  /* adjust directory entry */
	  if (readts (&f->filesys, f->de.track, f->de.sector, f->buffer))
	  {
	    return (-1);	/* doh! */
	  }
	  f->buffer[f->de.intcount * 32 + 2] |= cbm_CLOSED;
	  f->buffer[f->de.intcount * 32 + 30] = (f->blocks & 0xff);
	  f->buffer[f->de.intcount * 32 + 31] = (f->blocks / 0x100);
	  if (writets (&f->filesys, f->de.track, f->de.sector, f->buffer))
	  {
	    return (-1);	/* doh! */
	  }
	}
      }
    }

    /* set open=0, and clear some fields */
    if (f->open)
    {
      if ((f->filesys.fsfile)&&(f->realname[0]))
      {
	//debug_msg ("closing file (probably `%s')\n", f->realname);
	fclose (f->filesys.fsfile);
	f->filesys.fsfile = 0;
      }
      if (f->filesys.media == media_NET)
	fs_net_closefile (f);
      /* clear realname and fs64name */
      for (i = 0; i < 16; i++)
	f->fs64name[i] = 0;
      for (i = 0; i < 1024; i++)
	f->realname[i] = 0;
      f->open = 0;
    }


    return (0);
  }

}

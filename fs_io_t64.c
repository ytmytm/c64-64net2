/*
   T64 File system interface module for 64net/2
   (C)Copyright Paul Gardner-Stephen 1996, All rights reserved
 */

/* BUG: This module is incomplete and untested */

#include "fs.h"


int
fs_t64_getopenablename (fs64_file * f, fs64_direntry * de)
{

  /* BUG: should be de->realname below, but that variable is 
     the null string when referenced, so using the (psuedo) equivalent
     of de->fs) */

  strcpy (f->realname, de->fs);

  return (0);
}

int
fs_t64_openfile (fs64_file * f)
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

  /* shove load addr into buffer */
  /* BUG: This is a grotty hack to make up for a shortfall in the t64 format */
  f->buffer[2] = 0x01;
  f->buffer[3] = 0x08;
  f->bp = 2;
  f->be = 4;

  /* OKAY, all things are done */
  f->open = 1;
  return (0);
}

int
fs_t64_openfind (fs64_direntry * de, char *path)
{
  /* D64 filesystem block */
  de->filesys.media = media_T64;
  /* copy path into the filesystem source descriptor */
  strcpy (de->fs, path);
  strcpy (de->realname, path);
  /* now lets open it */
  if ((de->filesys.fsfile = fopen (de->fs, "r+")) == NULL)
  {
    /* try readonly open */
    if ((de->filesys.fsfile = fopen (de->fs, "r")) == NULL)
    {
      /* open failed */
      /* 74,DRIVE NOT READY,00,00 */
      set_error (74, 0, 0);
      return (-1);
    }
  }

  /* get file count */
  fseek (de->filesys.fsfile, 0x24, SEEK_SET);
  de->track = fgetc (de->filesys.fsfile);
  de->sector = 1;
  de->intcount = 0;
  de->active = 1;
  return (0);
}

int
fs_t64_getinfo (fs64_direntry * de)
{
  long i;
  unsigned char sectorbuffer[256];

  if (de->intcount >= de->track)
    return (-1);		/* end of directory */

  /* Read the file entry from the .T64
   */
  fseek (de->filesys.fsfile, 0x40 + (0x20 * de->intcount), SEEK_SET);
  for (i = 0; i < 0x20; i++)
    sectorbuffer[i] = fgetc (de->filesys.fsfile);

  de->invisible = 0;		/* we want the file to be visible */
  de->filetype = 0x82;		/* programme */
  /* copy filename */
  for (i = 0; i < 16; i++)
  {
    de->fs64name[i] = sectorbuffer[16 + i];
  }

  de->fs64name[16] = 0;

  /* blocks */
  de->realsize = sectorbuffer[4] + 256 * sectorbuffer[5];
  if (de->realsize % 254)
    de->blocks = (1 + (de->realsize) / 254);
  else
    de->blocks = ((de->realsize) / 254);

  /* start address of file */
  de->binbase = sectorbuffer[8] + sectorbuffer[9] * 256 + sectorbuffer[10] * 65536;

  return (0);

}

int
fs_t64_findnext (fs64_direntry * de)
{
  /* all work for this file system is done in fs64_getinfo
     as no actual file name is fetched (the job of fs64_findnext
     itself).
   */

  int i;

  i = fs64_getinfo (de);
  de->intcount++;
  if (i)
  {
    /* end of directory, so shut things down */
    de->active = 0;
    fclose (de->filesys.fsfile);
    de->filesys.fsfile = 0;
  }
  return (i);
}

int
fs_t64_readblock (fs64_file * f)
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
fs_t64_writeblock (fs64_file * f)
{
  /* BUGS: Doesnt check for free "blocks" aka diskspace */

  /* seek_set the file pointer right */
  fseek (f->filesys.fsfile, f->curr_poss, SEEK_SET);

  /* write the last 254 bytes of the block */
  if (f->bp == 256)
  {
    /* whole sector - allocate another */
    /* irrelevant for this file system! */
    /* just make sure there is some free space */
    if (fs64_blocksfree (&f->filesys) < 1)
    {
      /* partition full */
      set_error (72, 0, 0);
      return (-1);
    }
    /* ok */
  }
  else
  {
    /* partial sector */
    /* irrelevant for this file system */
    /* dont allocate another block as it is the end of the file */
    f->buffer[0] = 0;
    f->buffer[1] = (f->bp - 2);
  }
  for (f->be = 2; f->be < (f->bp); f->be++)
  {
    fputc (f->buffer[f->be], f->filesys.fsfile);
    f->curr_poss++;
    if (f->curr_poss >= (f->first_poss + f->realsize))
      /* eof reached */
      break;
  }
  /* add 254 to curr_poss */
  f->curr_poss += 254;
  f->realsize += 254;
  /* set buffer pointer */
  f->bp = 2;
  f->buffer[0] = 0;
  f->buffer[1] = (f->bp - 2);
  return (0);
}

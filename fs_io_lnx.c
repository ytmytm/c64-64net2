/*
   Lynx File System communication module for 64net/2
   (C)Copyright Paul Gardner-Stephen 1996

   Very hacked from UFS module

 */

#include "config.h"
#include "fs.h"

int
fs_lnx_createfile (uchar *path, uchar *name, int t, fs64_file * f)
{
  debug_msg ("lnx: Pretending to create file %s/%s [%d]\n",
	     path, name, t);
  return (-1);
}

int
fs_lnx_getopenablename (fs64_file * f, fs64_direntry * de)
{
  strcpy ((char*)f->realname, (char*)de->fs);
  return (0);
}

int
fs_lnx_openfile (fs64_file * f)
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
fs_lnx_allocateblock (fs64_filesystem * fs, int track, int sector)
{
  /* stub routine coz Lynx doesnt look blocky to 64net */
  return (0);
}


int
fs_lnx_blocksfree (fs64_filesystem * fs)
{
  /* lynx files are (currently) read only */
  return (0);
}


int
fs_lnx_findfreeblock (fs64_filesystem * fs, int *track, int *sector)
{
  /* read only file system */
  *track = -1;
  *sector = -1;
  return (0);
}

int
fs_lnx_deallocateblock (fs64_filesystem * fs, int track, int sector)
{
  return (-1);
}

int
fs_lnx_isblockfree (fs64_filesystem * fs, int track, int sector)
{
  /* read only file system (and non-blocky) */
  return (-1);
}

int
fs_lnx_create_g (uchar *path, uchar *glob, fs64_file * f)
{
  debug_msg ("Pretending to create (LNX) %s/%s\n", path, glob);
  return (-1);
}

int
fs_lnx_scratchfile (fs64_direntry * de)
{
  /* read only file system */
  return (-1);
}

int
fs_lnx_writeblock (fs64_file * f)
{
  /* read only file system */
  return (-1);
}

int
fs_lnx_readblock (fs64_file * f)
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
fs_lnx_headername (uchar *path, uchar *header, uchar *id, int par)
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
    header[j++] = path[i];
  /* end f the string */
  header[j] = 0;
  /* default */
  if ((!strcmp ((char*)path, "/")) || (header[0] == 0))
    sprintf ((char*)header, "LYNX ARCHIVE");

  strcpy ((char*)id, " LYNX");

  return (0);
}

int
fs_lnx_openfind (fs64_direntry * de, uchar *path)
{
  int i, j;
  uchar temp[16], c;

  /* LYNX filesystem file */
  de->filesys.media = media_LNX;

  /* copy path into the filesystem source descriptor */
  strcpy ((char*)de->fs, (char*)path);
  strcpy ((char*)de->realname, (char*)path);
  /* now lets open it (readonly) */
  if ((de->filesys.fsfile = fopen ((char*)de->fs, "r")) == NULL)
  {
    /* open failed */
    /* 74,DRIVE NOT READY,00,00 */
    set_error (74, 0, 0);
    return (-1);
  }

  /* All we intend to do here is:
     1) Get number of files in the archive   ( -> track    )
     2) Find start of file list in archive   ( -> fileposs )
     3) Find start of contents of first file ( -> binbase  )
   */

  /* skip BASIC header 
     (ie wait till 3 consecutive $00's */
  i = 0;
  j = 0;
  while (i < 3)
  {
    c = fgetc (de->filesys.fsfile);
    if (c)
      i = 0;
    else
      i++;
    j++;
    if (j > 250)
    {
      /* not a valid Lynx file */
      set_error (74, 0, 0);
      return (-1);
    }
  }
  de->fileposs = j;
  /* read $0d */
  c = fgetc (de->filesys.fsfile);
  /* read Lynx programme signature */
  c = fgetc (de->filesys.fsfile);
  while (c != 0xd)
  {
    /* debug_msg ("%c", c); */
    c = fgetc (de->filesys.fsfile);
  }
  /* debug_msg ("\n"); */

  /* read count of files in directory */
  i = 0;
  c = fgetc (de->filesys.fsfile);
  while (c != 0xd)
  {
    temp[i++] = c;
    c = fgetc (de->filesys.fsfile);
  }
  temp[i] = 0;
  de->track = atoi (temp);
  de->intcount = 0;
  /* debug_msg ("%d files in lnx\n", de->track); */

  /* store start of file info in archive */
  de->fileposs = ftell (de->filesys.fsfile);

  /* skip de->track*4 0x0d's to find the end of the file info,
     then skip to next whole sector to find start of binary storage */
  i = de->track * 4;
  while (i)
  {
    c = fgetc (de->filesys.fsfile);
    if (c == 0x0d)
      i--;
  }
  /* store start of file data in archive */
  de->binbase = ftell (de->filesys.fsfile);
  if (de->binbase % 254)
    de->binbase += (254 - (de->binbase % 254));

  /* seek back to fileposs for searching purposes */
  fseek (de->filesys.fsfile, de->fileposs, SEEK_SET);

  /* set blocks to zero, as the carry over blocks are for calcing 
     the start of the file data in the LNX archive */
  de->blocks = 0;

  /* mark directory entry active (usable) */
  de->active = 1;
  return (0);
}

int
fs_lnx_findnext (fs64_direntry * de)
{

  /* advance start of file binary by blocks of last file */
  de->binbase += 254 * de->blocks;

  if (de->intcount < de->track)
  {
    de->intcount++;
    fs64_getinfo (de);

    return (0);
  }
  else
  {
    /* no more entries */
    de->active = 0;
    fclose (de->filesys.fsfile);
    de->filesys.fsfile = 0;
    return (-1);
  }

}

int
fs_lnx_getinfo (fs64_direntry * de)
{
  int i;
  uchar c, temp[20];

  /* file will be visible */
  de->invisible = 0;
  /* clear old filename */
  for (i = 0; i < 16; i++)
    de->fs64name[i] = 0xa0;
  de->fs64name[16] = 0;
  /* read: name[CR]blocks[CR]type[CR]extra_bytes[CR] */
  /* filename */
  i = 0;
  c = fgetc (de->filesys.fsfile);
  while (c != 0x0d)
  {
    de->fs64name[i++] = c;
    c = fgetc (de->filesys.fsfile);
  }

  /* blocks */
  i = 0;
  c = fgetc (de->filesys.fsfile);
  while (c != 0x0d)
  {
    temp[i++] = c;
    c = fgetc (de->filesys.fsfile);
  }
  de->blocks = atoi (temp);
  /* filetype */
  c = fgetc (de->filesys.fsfile);
  while (c != 0x0d)
  {
    switch (c)
    {
    case 0x20:
    case 0x0d:
      break;
    case 'P':
      de->filetype = cbm_CLOSED | cbm_LOCKED | cbm_PRG;
      break;
    case 'U':
      de->filetype = cbm_CLOSED | cbm_LOCKED | cbm_USR;
      break;
    case 'S':
      de->filetype = cbm_CLOSED | cbm_LOCKED | cbm_SEQ;
      break;
    case 'R':
      de->filetype = cbm_CLOSED | cbm_LOCKED | cbm_REL;
      break;
    default:
      de->filetype = cbm_CLOSED | cbm_LOCKED | cbm_invalid;
      break;
    }
    c = fgetc (de->filesys.fsfile);
  }
  /* debug_msg ("filetype: %02x\n", de->filetype); */
  /* bytes in last block */
  i = 0;
  c = fgetc (de->filesys.fsfile);
  while (c != 0x0d)
  {
    temp[i++] = c;
    c = fgetc (de->filesys.fsfile);
  }
  /* calc actual size of file */
  de->realsize = (de->blocks - 1) * 254 + atoi (temp);

  /* debug_msg ("Offset $%04x, size: $%04x\n", (int) de->binbase, (int) de->realsize); */
  return (0);
}

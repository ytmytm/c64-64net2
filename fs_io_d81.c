/*
   64NET/2 D81 file specific routines
   (C) Copyright Paul Gardner-Stephen 1996, All rights reserved 
 */

#include "config.h"
#include "fs.h"
#include "fs_func.h"
#include "misc_func.h"

int
fs_d81_validate (fs64_filesystem * fs, int purgeflag)
{
  /* Validate a .D64 file */
  /* Algorithm:
     1) Read old T18 S0
     2) Clear old BAM (to all free)
     3) Alloc T18 S0
     4) Parse through each dir sector doing:
     4.1) Allocating directory sector
     4.2) Parsing through each file in the directory allocating used blocks,
     and clearing splat entries.
     5) Write back BAM
   */
  /*
     NOTE: If at anytime a block is attempted to be allocated, which already is, 
     a 71,DIRECTORY ERROR,tt,ss or a 75,FILESYSTEM INCONSISTENT,tt,ss 
     will be generated, with tt and ss being the cross-linked sector.
     *BUT*: If the purgeflag variable is set then the following will happen;
     Crosslinked files will be truncated.
     Crosslinked directories will be truncated.
     Bad blocks will be allocated (no error return). */

  uchar t18s0[3][256];		/* BAM buffer */
  int dt, ds;

  set_error (0, 0, 0);

  if (fs_d81_readbam (fs, t18s0))
    return (-1);

  /* Step 2 & 3 - Free all blocks *except* T18 S0 */
  fs_d81_makebam (t18s0);

  /* Step 4 - Parse through directory */
  dt = t18s0[0][0];
  ds = t18s0[0][1];
  if (fs_dxx_validate_dir (fs, purgeflag, (void *) t18s0, dt, ds))
  {
    /* error */
    return (-1);
  }
  else
  {
    if (fs_d81_writebam (fs, t18s0))
      return (-1);
    return (0);
  }

}


int
fs_d81_bamblocksfree (uchar blocks[3][256])
{
  int i, bc = 0;
  for (i = 1; i < 40; i++)
    bc += blocks[1][10 + i * 6];
  for (i = 41; i < 81; i++)
    bc += blocks[2][10 + (i - 40) * 6];

  debug_msg ("d81: %d bf\n", bc);
  return (bc);
}

int
fs_d81_bamfindfreeblock (uchar blocks[3][256], int *track, int *sector)
{
  /* find a free block on a D81 */

  int i;

  /* first 1/2 of the disk */
  for (*track = 39; *track > 0; (*track)--)
    if (blocks[1][10 + (*track) * 6])
    {
      /* free block here */
      for (i = 1; i < 6; i++)
	if (blocks[1][10 + (*track) * 6 + i])
	{
	  /* free sector here */
	  *sector = (i - 1) * 8 + firsthighbit (blocks[1][10 + (*track) * 6 + i]);
	  return (0);
	}
      /* directory error */
      set_error (71, 0, 0);
      return (-1);
    }

  /* second 1/2 of the disk */
  for (*track = 41; *track < 81; (*track)++)
    if (blocks[2][10 + (*track - 40) * 6])
    {
      /* free block here */
      for (i = 1; i < 6; i++)
	if (blocks[2][10 + (*track - 40) * 6 + i])
	{
	  /* free sector here */
	  *sector = (i - 1) * 8 + firsthighbit (blocks[2][10 + (*track - 40) * 6 + i]);
	  return (0);
	}
      /* directory error */
      set_error (71, 0, 0);
      return (-1);
    }

  *track = -1;
  *sector = -1;
  return (-1);
}

int
fs_d81_bamalloc (int t, int s, uchar blocks[3][256])
{
  int lt = t, b = 1;
  int po2[8] =
  {1, 2, 4, 8, 16, 32, 64, 128};
  int ofs = (s / 8), bit = po2[s & 7];

  if (lt > 40)
  {
    lt -= 40;
    b = 2;
  }
  debug_msg ("Bit: %02x Ofs: %d\n", bit, ofs);
  debug_msg ("blocks[%d][%d] = %d\n", b, 11 + lt * 6 + ofs, blocks[b][10 + lt * 6 + ofs] & bit);
  if (blocks[b][11 + lt * 6 + ofs] & bit)
  {
    /* free */
    debug_msg ("d81: allocing T%d S%d\n", t, s);
    blocks[b][11 + lt * 6 + ofs] &= (0xff - bit);
    blocks[b][10 + lt * 6]--;
    return (0);
  }
  else
  {
    /* already alloc'd */
    debug_msg ("d81: T%d S%d already in use\n", t, s);
    return (-1);
  }
}

int
fs_d81_bamdealloc (uchar blocks[3][256], int t, int s)
{
  int lt = t, b = 1;
  int po2[8] =
  {1, 2, 4, 8, 16, 32, 64, 128};
  int ofs = (s / 8), bit = po2[s & 7];

  if (lt > 40)
  {
    lt -= 40;
    b = 2;
  }
  if (!blocks[b][11 + lt * 6 + ofs] & bit)
  {
    /* free */
    blocks[b][10 + lt * 6]++;
    blocks[b][11 + lt * 6 + ofs] |= bit;
    return (0);
  }
  else
    /* already alloc'd */
    return (-1);
}

int
fs_d81_blocksfree (fs64_filesystem * fs)
{
  uchar blocks[3][256];

  if (fs_d81_readbam (fs, blocks))
    return (-1);
  return (fs_d81_bamblocksfree (blocks));

}

int
fs_d81_findfreeblock (fs64_filesystem * fs, int *track, int *sector)
{
  uchar blocks[3][256];

  if (fs_d81_readbam (fs, blocks))
    return (-1);
  if (fs_d81_bamfindfreeblock (blocks, track, sector))
  {
    *track = -1;
    *sector = -1;
    return (-1);
  }
  else
    return (0);
}

int
fs_d81_allocateblock (fs64_filesystem * fs, int track, int sector)
{
  uchar blocks[3][256];
  int t, s;

  if (fs_d81_readbam (fs, blocks))
    return (-1);
  if (fs_d81_bamalloc (track, sector, blocks))
  {
    /* already allocated, so tell em wht a free one is */
    if (fs_d81_bamfindfreeblock (blocks, &t, &s))
      set_error (65, 0, 0);
    else
      set_error (65, t, s);
    return (-1);
  }
  if (fs_d81_writebam (fs, blocks))
    return (-1);

  return (0);
}

int
fs_d81_deallocateblock (fs64_filesystem * fs, int track, int sector)
{
  uchar blocks[3][256];

  if (fs_d81_readbam (fs, blocks))
    return (-1);
  if (fs_d81_bamdealloc (blocks, track, sector))
    return (-1);
  if (fs_d81_writebam (fs, blocks))
    return (-1);

  return (0);
}

int
fs_d81_format (fs64_filesystem * fs, uchar *name, uchar *id)
{
  /* format a D64 file */
  int t, s;
  uchar blocks[3][256] =
  {
    {0}};

  /* If id is non null then format each sector, otherwise
     only clear dir and BAM */
  if (id[0])
  {
    /* toast *everything* */
    for (t = 1; t < 81; t++)
    {
      for (s = 0; s < 40; s++)
      {
	if (writets (fs, t, s, blocks[0]))
	  return (-1);
      }
    }
    /* clear disk name */
    for (t = 24; t < 28; t++)
      blocks[0][t] = 0xa0;
    /* copy new id in */
    blocks[0][22] = id[0];
    blocks[0][23] = id[1];
  }
  else
  {
    /* toast first dir sector */
    if (writets (fs, 40, 3, blocks[0]))
      return (-1);
  }

  /* make shiny new bam */
  fs_d81_makebam (blocks);
  /* set DOS information */
  blocks[0][0] = 40;
  blocks[0][1] = 3;
  blocks[0][25] = '3';
  blocks[0][26] = 'D';
  blocks[0][2] = 'D';
  blocks[0][3] = 0;

  /* copy header in */
  for (t = 4; t < 22; t++)
    blocks[0][t] = 0xa0;
  for (t = 4; name[t - 4]; t++)
    blocks[0][t] = name[t - 4];

  /* write back */
  /* BUG: Needs to be written twice! */
  fs_d81_writebam (fs, blocks);

  set_error (0, 0, 0);
  return (0);

}

int
fs_d81_readbam (fs64_filesystem * fs, uchar blocks[3][256])
{
  /* read a D81 bam */
  int i;

  for (i = 0; i < 3; i++)
    if (readts (fs, 40, i, blocks[i]))
      return (-1);
  return (0);
}

int
fs_d81_writebam (fs64_filesystem * fs, uchar blocks[3][256])
{
  /* write a D81 bam */
  int i;

  for (i = 0; i < 3; i++)
  {
    if (writets (fs, 40, i, blocks[i]))
      return (-1);
    if (writets (fs, 40, i, blocks[i]))
      return (-1);
  }
  return (0);
}

int
fs_d81_makebam (uchar blocks[3][256])
{
  /* create a new D81 bam */
  int t = 0, i, j;

  blocks[1][0] = 40;
  blocks[1][1] = 2;
  blocks[1][2] = 'D';
  blocks[1][3] = 187;
  blocks[1][4] = blocks[0][22];
  blocks[1][5] = blocks[0][23];
  blocks[1][6] = 0xc0;
  blocks[1][7] = 0x00;

  blocks[2][0] = 0;
  blocks[2][1] = 0;
  blocks[2][2] = 'D';
  blocks[2][3] = 187;
  blocks[2][4] = blocks[0][22];
  blocks[2][5] = blocks[0][23];
  blocks[2][6] = 0xc0;
  blocks[2][7] = 0x00;

  /* BAM data */
  for (i = 16; i < 256; i += 6)
  {
    t++;
    blocks[1][i] = 40;
    for (j = 1; j < 6; j++)
      blocks[1][i + j] = 0xff;
    blocks[2][i] = 40;
    for (j = 1; j < 6; j++)
      blocks[2][i + j] = 0xff;
    /* allocate dir blocks */
    if (t == 40)
    {
      blocks[1][i] = 37;
      blocks[1][i + 1] = 0xf8;
    }
  }

  return (0);
}

int
fs_d81_headername (uchar *path, uchar *header, uchar *id, int par, fs64_file * f)
{
  uchar buff[256];
  fs64_filesystem ff;
  int i;
  ff.fsfile = 0;

  f->filesys.media = fs64_mediatype (path);
  f->de.filesys.media = f->filesys.media;
  ff.media = f->filesys.media;
  strcpy (ff.fspath, path);
  if ((ff.fsfile = fopen (path, "r")) != NULL)
  {
    if (!readts (&ff, 40, 0, buff))
    {
      for (i = 0; i < 16; i++)
	if (buff[i + 4] != 0xa0)
	  header[i] = buff[i + 4];
	else
	  header[i] = 0x20;
      for (i = 0; i < 5; i++)
	if (buff[i + 22] != 0xa0)
	  id[i] = buff[22 + i];
	else
	  id[i] = 0x20;
    }
    else
    {
      /* cant read sector - so make it up :) */
      strcpy (header, "@@@@@@@@@@@@@@@@");
      strcpy (id, "@@@@@");
    }
    fclose (ff.fsfile);
    ff.fsfile = 0;
    return (0);
  }
  else
  {
    /* cant open file system */
    strcpy (header, "@@@@@@@@@@@@@@@@");
    strcpy (id, "@@@@@");
    return (0);
  }
}

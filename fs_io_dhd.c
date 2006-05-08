/*
   64NET/2 .DHD specific routines 
   (C) Copyright Paul Gardner-Stephen 1996, All rights reserved
 */

#include "config.h"
#include "fs.h"
#include "fs_func.h"
#include "misc_func.h"

static int bitsset[256] =
{
  0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,	/* 0x */
  1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,	/* 1x */
  1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,	/* 2x */
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,	/* 3x */
  1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,	/* 4x */
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,	/* 5x */
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,	/* 6x */
  3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,	/* 7x */
  1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,	/* 8x */
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,	/* 9x */
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,	/* ax */
  3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,	/* bx */
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,	/* cx */
  3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,	/* dx */
  3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,	/* ex */
  4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8	/* fx */
};

int
fs_dhd_validate (fs64_filesystem * fs, int purgeflag)
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

  uchar t18s0[34][256];		/* BAM buffer */
  int dt, ds, num_tracks;

  set_error (0, 0, 0);

  /* get number of tracks */
  fseek (fs->fsfile, 54, SEEK_SET);
  num_tracks = fgetc (fs->fsfile);
  if ((num_tracks < 1) || (num_tracks > 255))
  {
    set_error (74, 0, 0);
    return (-1);
  }

  if (fs_dhd_readbam (fs, t18s0))
    return (-1);

  /* Step 2 & 3 - Free all blocks *except* T18 S0 */
  fs_dhd_makebam (t18s0, num_tracks);

  /* Step 4 - Parse through directory */
  dt = t18s0[1][0];
  ds = t18s0[1][1];
  if (fs_dxx_validate_dir (fs, purgeflag, (void *) t18s0, dt, ds))
  {
    /* error */
    return (-1);
  }
  else
  {
    if (fs_dhd_writebam (fs, t18s0))
      return (-1);
    return (0);
  }

}

int
fs_dhd_bamblocksfree (uchar blocks[34][256])
{
  int i, bc = 0, bn, ofs, j;
  for (i = 1; i <= blocks[2][8]; i++)
  {
    bn = 2 + ((i - 8) / 8);
    ofs = 32 * (i & 7);
    for (j = 0; j < 32; j++)
      bc += bitsset[blocks[bn][ofs + j]];
  }

  return (bc);
}

int
fs_dhd_bamfindfreeblock (uchar blocks[34][256], int *track, int *sector)
{
  /* find a free block on a DHD */

  int i, bn, ofs;

  for (*track = 1; *track <= blocks[2][8]; (*track)++)
  {
    bn = 2 + (*track / 8);
    ofs = 32 * (*track & 7);
    for (i = 0; i < 32; i++)
      if (blocks[bn][ofs + i])
      {
	/* free block here */
	*sector = i * 8 + 7 - firsthighbit (blocks[bn][ofs + i]);
	return (0);
      }
  }
  *track = -1;
  *sector = -1;
  return (-1);
}

int
fs_dhd_bamalloc (int t, int s, uchar blocks[34][256])
{
  int po2[8] =
  {128, 64, 32, 16, 8, 4, 2, 1};
  int so = (s / 8), bit = po2[s & 7];
  int ofs, bn;

  bn = 2 + (t / 8);
  ofs = 32 * (t & 7);

  if (blocks[bn][ofs + so] & bit)
  {
    /* free */
    blocks[bn][ofs + so] -= bit;
    return (0);
  }
  else
  {
    /* already alloc'd */
    return (-1);
  }
}

int
fs_dhd_bamdealloc (uchar blocks[34][256], int t, int s)
{
  int po2[8] =
  {128, 64, 32, 16, 8, 4, 2, 1};
  int so = (s / 8), bit = po2[s & 7];
  int ofs, bn;

  bn = 2 + (t / 8);
  ofs = 32 * (t & 7);

  if (!blocks[bn][ofs + so] & bit)
  {
    /* alloc'd */
    blocks[bn][ofs + so] |= bit;
    return (0);
  }
  else
  {
    /* already free */
    return (-1);
  }
}

int
fs_dhd_blocksfree (fs64_filesystem * fs)
{
  uchar blocks[34][256];

  if (fs_dhd_readbam (fs, blocks))
    return (-1);
  return (fs_dhd_bamblocksfree (blocks));

}

int
fs_dhd_findfreeblock (fs64_filesystem * fs, int *track, int *sector)
{
  uchar blocks[34][256];

  if (fs_dhd_readbam (fs, blocks))
    return (-1);
  if (fs_dhd_bamfindfreeblock (blocks, track, sector))
  {
    *track = -1;
    *sector = -1;
    return (-1);
  }
  else
    return (0);
}

int
fs_dhd_allocateblock (fs64_filesystem * fs, int track, int sector)
{
  uchar blocks[34][256];
  int t, s;

  if (fs_dhd_readbam (fs, blocks))
    return (-1);
  if (fs_dhd_bamalloc (track, sector, blocks))
  {
    /* already allocated, so tell em wht a free one is */
    if (fs_dhd_bamfindfreeblock (blocks, &t, &s))
      set_error (65, 0, 0);
    else
      set_error (65, t, s);
    return (-1);
  }
  if (fs_dhd_writebam (fs, blocks))
    return (-1);

  return (0);
}

int
fs_dhd_deallocateblock (fs64_filesystem * fs, int track, int sector)
{
  uchar blocks[34][256];

  if (fs_dhd_readbam (fs, blocks))
    return (-1);
  if (fs_dhd_bamdealloc (blocks, track, sector))
    return (-1);
  if (fs_dhd_writebam (fs, blocks))
    return (-1);

  return (0);
}

int
fs_dhd_format (fs64_filesystem * fs, uchar *name, uchar *id)
{
  /* format a D64 file */
  int t, s, num_tracks;
  uchar blocks[34][256] =
  {
    {0}};

  errno = 0;
  fseek (fs->fsfile, 54, SEEK_SET);
  num_tracks = fgetc (fs->fsfile);
  if ((num_tracks < 1) || (num_tracks > 255))
  {
    set_error (74, 0, 0);
    return (-1);
  }

  /* If id is non null then format each sector, otherwise
     only clear dir and BAM */
  if (id[0])
  {
    /* toast *everything* */
    for (t = 1; t <= num_tracks; t++)
    {
      for (s = 0; s < 256; s++)
      {
	if (writets (fs, t, s, blocks[0]))
	  return (-1);
      }
    }
    /* place padding */
    for (t = 24; t < 28; t++)
      blocks[1][t] = 0xa0;
    /* copy new id in */
    blocks[1][22] = id[0];
    blocks[1][23] = id[1];
  }
  else
  {
    /* toast first dir sector */
    if (writets (fs, 1, (num_tracks / 8) + 3, blocks[0]))
      return (-1);
  }

  /* make shiny new bam */
  fs_dhd_makebam (blocks, num_tracks);
  /* set DOS information */
  blocks[1][0] = 1;
  blocks[1][1] = (num_tracks / 8) + 3;
  blocks[1][25] = '1';
  blocks[1][26] = 'H';
  blocks[1][2] = 'H';
  blocks[1][3] = 0;
  /* root dir header pointer */
  blocks[1][32] = 1;
  blocks[1][33] = 1;
  /* parent header pointer */
  blocks[1][34] = 0x00;
  blocks[1][35] = 0x00;
  /* reference to location in parent */
  blocks[1][36] = 0x00;
  blocks[1][37] = 0x00;
  blocks[1][38] = 0x00;

  /* copy header in */
  for (t = 4; t < 22; t++)
    blocks[1][t] = 0xa0;
  for (t = 4; name[t - 4]; t++)
    blocks[1][t] = name[t - 4];

  /* write back */
  fs_dhd_writebam (fs, blocks);

  set_error (0, 0, 0);
  return (0);

}

int fs_dhd_readbam (fs64_filesystem * fs, uchar blocks[34][256])
{
  /* read a D81 bam */
  int i;
  int num_tracks;

  fseek (fs->fsfile, 54, SEEK_SET);
  num_tracks = fgetc (fs->fsfile);
  if ((num_tracks < 1) || (num_tracks > 255))
  {
    set_error (74, 0, 0);
    return (-1);
  }

  for (i = 0; i < ((num_tracks / 8) + 3); i++)
    if (readts (fs, 1, i, blocks[i]))
      return (-1);
  return (0);
}

int
fs_dhd_writebam (fs64_filesystem * fs, uchar blocks[3][256])
{
  /* write a D81 bam */
  int i;
  int num_tracks;

  fseek (fs->fsfile, 54, SEEK_SET);
  num_tracks = fgetc (fs->fsfile);
  if ((num_tracks < 1) || (num_tracks > 255))
  {
    set_error (74, 0, 0);
    return (-1);
  }

  for (i = 0; i < ((num_tracks / 8) + 3); i++)
  {
    if (writets (fs, 1, i, blocks[i]))
      return (-1);
    if (writets (fs, 1, i, blocks[i]))
      return (-1);
  }

  return (0);
}

int
fs_dhd_makebam (uchar blocks[34][256], int num_tracks)
{
  /* create a new D81 bam */
  int i, j;
  int bn, ofs;

  blocks[1][0] = 1;
  blocks[1][1] = (num_tracks / 8) + 3;

  blocks[2][0] = 0;
  blocks[2][1] = 0;
  blocks[2][2] = 'H';
  blocks[2][3] = 183;
  blocks[2][4] = blocks[1][22];
  blocks[2][5] = blocks[1][23];
  blocks[2][6] = 0xc0;
  blocks[2][7] = 0x00;
  blocks[2][8] = num_tracks;

  /* BAM data */
  for (i = 1; i <= num_tracks; i++)
  {
    bn = 2 + (i / 8);
    ofs = (i & 7) * 32;
    for (j = 0; j < 32; j++)
      blocks[bn][ofs + j] = 0xff;
  }

  /* allocate T1 S0 to S(2+(num_tracks/8)) */
  for (j = 0; j <= (2 + (num_tracks / 8)); j++)
    fs_dhd_bamalloc (1, j, blocks);

  return (0);
}

int
fs_dhd_headername (uchar *path, uchar *header, uchar *id, int par, fs64_file * f)
{
  uchar buff[256];
  fs64_filesystem ff;
  int i;
  ff.fsfile = 0;

  f->filesys.media = fs64_mediatype (path);
  f->de.filesys.media = f->filesys.media;
  if (f->filesys.dirtrack < 1)
  {
    f->filesys.dirtrack = 1;
    f->filesys.dirsector = 1;
  }
  ff.media = f->filesys.media;
  strcpy ((char*)ff.fspath, (char*)path);
  if ((ff.fsfile = fopen ((char*)path, "r")) != NULL)
  {
    if (!readts (&ff, f->filesys.dirtrack, f->filesys.dirsector, buff))
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
      strcpy ((char*)header, "@@@@@@@@@@@@@@@@");
      strcpy ((char*)id, "@@@@@");
    }
    fclose (ff.fsfile);
    ff.fsfile = 0;
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

/* 
   64NET/2 .D64 Specific routines
   (C)Copyright Paul Gardner-Stephen 1996, All rights reserved
 */

#include "fs.h"
#include "fs_func.h"

int
fs_d64_allocateblock (fs64_filesystem * fs, int track, int sector)
{
  unsigned char sectorbuffer[256];
  int t, s;

  printf ("fs_d64_allocateblock\n");

  /* ensure its a valid track & sector */
  if (fs_resolve_ts (fs->media, track, sector) < 0)
  {
    /* bad track/sector */
    /* 66,ILLEGAL BLOCK,00,00 */
    set_error (66, track, sector);
    return (-1);
  }
  else
  {
    /* valid track & sector */
    /* so... read it in! */
    if (!readts (fs, 18, 0, sectorbuffer))
    {
      /* read BAM ok */
      /* calc offset */
      if (!fs_d64_bamalloc (track, sector, sectorbuffer))
      {
	/* write sector */
	if (!writets (fs, 18, 0, sectorbuffer))
	  /* all's well */
	  return (0);
	else
	  /* couldnt write */
	  /* writets will have set the error */
	  return (-2);
      }
      else
      {
	/* block in use */
	/* return next free block, if there is one */
	if (!fs64_findfreeblock (fs, &t, &s))
	{
	  /* there is one */
	  /* 65,NO BLOCK,TT,SS */
	  set_error (65, t, s);
	}
	else
	  /* nope, disk full */
	  /* 65,NO BLOCK,00,00 */
	  set_error (65, 0, 0);

	return (-1);
      }
    }
    else
    {
      /* couldnt read t18 s0 ! */
      /* readts will have set error */
      return (-2);
    }
  }
}

int
fs_d64_deallocateblock (fs64_filesystem * fs, int track, int sector)
{
  unsigned char sectorbuffer[256];
  int ofs = 0, bit = 0;


  /* ensure its a valid track & sector */
  if (fs_resolve_ts (fs->media, track, sector) < 0)
  {
    /* bad track/sector */
    /* 66,ILLEGAL BLOCK,00,00 */
    set_error (66, track, sector);
    return (-1);
  }
  else
  {
    /* valid track & sector */
    /* so... read it in! */
    if (!readts (fs, 18, 0, sectorbuffer))
    {
      /* read BAM ok */
      /* calc offset */
      ofs = 1 + (track * 4) + (sector >> 3);
      bit = sector & 7;
      /* check bit */
      if (!(sectorbuffer[ofs] & (1 << bit)))
      {
	/* its in use, but not for much longer.. */
	sectorbuffer[ofs] |= (1 << bit);
	/* increase free sector count on that track */
	sectorbuffer[ofs & 0xfc]++;
	/* write sector */
	if (!writets (fs, 18, 0, sectorbuffer))
	  /* all's well */
	  return (0);
	else
	  /* couldnt write */
	  /* writets will have set the error */
	  return (-2);
      }
      else
      {
	/* block already free */
	/* no error is given to this */
	return (-1);
      }
    }
    else
    {
      /* couldnt read t18 s0 ! */
      /* error will have been set by readts */
      return (-2);
    }
  }
}

int
fs_d64_findfreeblock (fs64_filesystem * fs, int *track, int *sector)
{
  unsigned char sectorbuffer[256];
  int i, t = 0, ms=21, j;
  unsigned long trackbuf;

  /* .D64 file */
  /* read track 18 sector 0 and work it out */
  if (!readts (fs, 18, 0, sectorbuffer))
  {
    /* read it ok */
    int k;
    int tl[35] =
    {17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35};
    for (k = 0; k < 35; k++)
    {
      t = tl[k];
      i = t * 4;
      trackbuf = sectorbuffer[i + 1];
      trackbuf += sectorbuffer[i + 2] * 256;
      trackbuf += sectorbuffer[i + 3] * 65536;
      if (t < 36)
	ms = 17;
      if (t < 31)
	ms = 18;
      if (t < 25)
	ms = 19;
      if (t < 18)
	ms = 21;
      if (t == 18)
	ms = 0;			/* dont allow allocation of sectors on track 18 */
      for (j = 0; j < ms; j++)
      {
	if (trackbuf & 1)
	{
	  *track = t;
	  *sector = j;
	  printf ("Nominating T%d S%d as free block\n", *track, *sector);
	  return (0);
	}
	else
	  trackbuf = trackbuf >> 1;
      }
    }
  }
  else
  {
    /* an error occured while reading */
    /* readst will have set the error */
    *track = -1;
    *sector = -1;
    return (-1);
  }

  *track = -1;
  *sector = -1;
  return (-1);

}

int
fs_d64_blocksfree (fs64_filesystem * fs)
{
  unsigned char sectorbuffer[256];
  int i, t = 0, blocks = 0;

  /* .D64 file */
  /* read track 18 sector 0 and work it out */
  if (!readts (fs, 18, 0, sectorbuffer))
  {
    /* read it ok */
    for (i = 4; i < 144; i += 4)
    {
      t++;
      if (t != 18)
      {
	blocks += sectorbuffer[i];
      }
    }
    return (blocks);
  }
  else
  {
    /* an error occured */
    /* readst will have set the error */
    return (0);
  }
}

int
fs_d64_validate (fs64_filesystem * fs, int purgeflag)
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

  uchar t18s0[256];		/* BAM buffer */
  int dt, ds;

  set_error (0, 0, 0);

  if (fs_d64_readbam (fs, t18s0))
    return (-1);

  /* Step 2 & 3 - Free all blocks *except* T18 S0 */
  fs_d64_makebam (t18s0);

  /* Step 4 - Parse through directory */
  dt = t18s0[0];
  ds = t18s0[1];
  if (fs_dxx_validate_dir (fs, purgeflag, (void *) t18s0, dt, ds))
  {
    /* error */
    return (-1);
  }
  else
  {
    if (fs_d64_writebam (fs, t18s0))
      return (-1);
    return (0);
  }

}

int
fs_d64_readbam (fs64_filesystem * fs, uchar t18s0[256])
{
  /* Step 1 - Read old T18 S0 */
  if (readts (fs, 18, 0, t18s0))
  {
    /* readts will have set the error */
    return (-1);
  }
  return (0);
}

int
fs_d64_writebam (fs64_filesystem * fs, uchar t18s0[256])
{
  /* i dont know why this needs to be twice, but it does! */
  if (writets (fs, 18, 0, t18s0))
    return (-1);
  if (writets (fs, 18, 0, t18s0))
    return (-1);
  return (0);
}

int
fs_d64_bamalloc (int track, int sector, uchar sectorbuffer[256])
{
  int ofs, bit;

  /* calc offset */
  ofs = 1 + (track * 4) + (sector >> 3);
  bit = sector & 7;
  if (sectorbuffer[ofs] & (1 << bit))
  {
    printf ("Allocing block T%d s%d\n", track, sector);
    /* its free, but not for much longer.. */
    sectorbuffer[ofs] &= (0xff - (1 << bit));
    /* reduce free block count */
    sectorbuffer[ofs & 0xfc]--;
    return (0);
  }
  else
    /* already alloced */
    return (-1);
}

int
fs_d64_format (fs64_filesystem * fs, char *name, char *id)
{
  /* format a D64 file */
  int t, s=21;
  uchar block[256] =
  {0};

  /* If id is non null then format each sector, otherwise
     only clear dir and BAM */
  if (id[0])
  {
    /* toast *everything* */
    for (t = 1; t < 36; t++)
    {
      if (t < 18)
	s = 21;
      else if (t < 25)
	s = 19;
      else if (t < 31)
	s = 18;
      else if (t < 36)
	s = 17;
      for (; s; s--)
      {
	if (writets (fs, t, s - 1, block))
	  return (-1);
      }
    }
    /* build T18 S0 */
    for (t = 162; t < 171; t++)
      block[t] = 0xa0;
    /* copy new id in */
    block[162] = id[0];
    block[163] = id[1];
  }
  else
  {
    /* toast T18 S1 */
    if (writets (fs, 18, 1, block))
      return (-1);
  }

  /* make shiny new bam */
  fs_d64_makebam (block);
  block[0] = 18;
  block[1] = 1;
  block[165] = '2';
  block[166] = 'A';
  block[2] = 'A';
  block[3] = 0;

  /* copy header in */
  for (t = 144; t < 161; t++)
    block[t] = 0xa0;
  for (t = 144; name[t - 144]; t++)
    block[t] = name[t - 144];

  /* write back */
  /* BUG: Needs to be written twice! */
  if (writets (fs, 18, 0, block))
    return (-1);
  if (writets (fs, 18, 0, block))
    return (-1);

  set_error (0, 0, 0);
  return (0);
}

int
fs_d64_makebam (uchar t18s0[256])
{
  /* create a new BAM with only T18 S0 allocated */
  int t;

  for (t = 1; t < 36; t++)
  {
    if (t == 18)
    {
      /* alloc only S0 */
      t18s0[4 * t] = 18;
      t18s0[4 * t + 1] = 0xfe;
      t18s0[4 * t + 2] = 0xff;
      t18s0[4 * t + 3] = 0x07;
    }
    else if (t < 18)
    {
      t18s0[4 * t] = 21;
      t18s0[4 * t + 1] = 0xff;
      t18s0[4 * t + 2] = 0xff;
      t18s0[4 * t + 3] = 0x1f;
    }
    else if (t < 25)
    {
      t18s0[4 * t] = 19;
      t18s0[4 * t + 1] = 0xff;
      t18s0[4 * t + 2] = 0xff;
      t18s0[4 * t + 3] = 0x07;
    }
    else if (t < 31)
    {
      t18s0[4 * t] = 18;
      t18s0[4 * t + 1] = 0xff;
      t18s0[4 * t + 2] = 0xff;
      t18s0[4 * t + 3] = 0x03;
    }
    else
    {
      t18s0[4 * t] = 17;
      t18s0[4 * t + 1] = 0xff;
      t18s0[4 * t + 2] = 0xff;
      t18s0[4 * t + 3] = 0x01;
    }

  }				/* for (t=1;t<36;t++) */
  return (0);
}

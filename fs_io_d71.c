/* 
   64NET/2 .D71 Specific routines
   (C)Copyright Paul Gardner-Stephen 1996, All rights reserved
 */

#include "fs.h"
#include "fs_func.h"
#include "misc_func.h"

int
fs_d71_validate (fs64_filesystem * fs, int purgeflag)
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

  uchar t18s0[2][256];		/* BAM buffer */
  int dt, ds;

  set_error (0, 0, 0);

  if (fs_d71_readbam (fs, t18s0))
    return (-1);

  /* Step 2 & 3 - Free all blocks *except* T18 S0 */
  fs_d71_makebam (t18s0);

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
    if (fs_d71_writebam (fs, t18s0))
      return (-1);
    return (0);
  }

}

int
fs_d71_blocksfree (fs64_filesystem * fs)
{
  unsigned char sectorbuffer[256];
  int i, t = 0, blocks = 0;

  /* .D71 file */
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
    t = 0;
    for (i = 221; i <= 255; i++)
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
fs_d71_makebam (uchar blocks[2][256])
{
  /* create a new BAM with only T18 S0 allocated */
  int t;

  /* do first side */
  fs_d64_makebam (blocks[0]);
  /* correct for 1571 */
  /* 1571 flag */
  blocks[0][3] = 0x80;
  /* t36-71 block counts */
  for (t = 0; t < 17; t++)
    blocks[0][221 + t] = 21;
  blocks[0][221 + 17] = 20;	/* t53 s0 is allocated */
  for (t = 18; t < 24; t++)
    blocks[0][221 + t] = 19;
  for (t = 24; t < 30; t++)
    blocks[0][221 + t] = 18;
  for (t = 30; t < 35; t++)
    blocks[0][221 + t] = 17;

  /* now flip side */
  for (t = 0; t < 35; t++)
  {
    if (t == 17)
    {
      /* alloc only S0 */
      blocks[1][3 * t + 0] = 0xfe;
      blocks[1][3 * t + 1] = 0xff;
      blocks[1][3 * t + 2] = 0x07;
    }
    else if (t < 17)
    {
      blocks[1][3 * t + 0] = 0xff;
      blocks[1][3 * t + 1] = 0xff;
      blocks[1][3 * t + 2] = 0x1f;
    }
    else if (t < 24)
    {
      blocks[1][3 * t] = 0xff;
      blocks[1][3 * t + 1] = 0xff;
      blocks[1][3 * t + 2] = 0x07;
    }
    else if (t < 30)
    {
      blocks[1][3 * t + 0] = 0xff;
      blocks[1][3 * t + 1] = 0xff;
      blocks[1][3 * t + 2] = 0x03;
    }
    else
    {
      blocks[1][3 * t + 0] = 0xff;
      blocks[1][3 * t + 1] = 0xff;
      blocks[1][3 * t + 2] = 0x01;
    }

  }				/* for (t=1;t<36;t++) */
  return (0);
}

int
fs_d71_format (fs64_filesystem * fs, char *name, char *id)
{
  /* format a D71 file */
  int t=1, s=21;
  uchar blocks[2][256] =
  {
    {0}};

  printf ("fs_d71_format\n()");

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
	/* this may be a little slow due to seeking everywhere! */
	if (writets (fs, t, s - 1, blocks[0]))
	  return (-1);
	if (writets (fs, t + 35, s - 1, blocks[0]))
	  return (-1);
      }
    }
    /* build T18 S0 */
    for (t = 162; t < 171; t++)
      blocks[0][t] = 0xa0;
    /* copy new id in */
    blocks[0][162] = id[0];
    blocks[0][163] = id[1];
  }
  else
  {
    /* toast T18 S1 */
    if (writets (fs, 18, 1, blocks[0]))
      return (-1);
  }

  /* make shiny new bam */
  fs_d71_makebam (blocks);
  blocks[0][0] = 18;
  blocks[0][1] = 1;
  blocks[0][165] = '2';
  blocks[0][166] = 'A';
  blocks[0][2] = 'A';
  blocks[0][3] = 0x80;

  /* copy header in */
  for (t = 144; t < 161; t++)
    blocks[0][t] = 0xa0;
  for (t = 144; name[t - 144]; t++)
    blocks[0][t] = name[t - 144];

  /* write back */
  /* BUG: Needs to be written twice! */
  if (writets (fs, 18, 0, blocks[0]))
    return (-1);
  if (writets (fs, 18, 0, blocks[0]))
    return (-1);
  if (writets (fs, 53, 0, blocks[1]))
    return (-1);
  if (writets (fs, 53, 0, blocks[1]))
    return (-1);

  set_error (0, 0, 0);
  return (0);
}

int
fs_d71_deallocateblock (fs64_filesystem * fs, int track, int sector)
{
  unsigned char sectorbuffers[2][256];
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
    if (readts (fs, 53, 0, sectorbuffers[1]))
      return (-1);
    if (!readts (fs, 18, 0, sectorbuffers[0]))
    {
      /* read BAM ok */
      /* calc offset */
      if (track < 36)
      {
	ofs = 1 + (track * 4) + (sector >> 3);
	bit = sector & 7;
	/* check bit */
	if (!(sectorbuffers[0][ofs] & (1 << bit)))
	{
	  /* its in use, but not for much longer.. */
	  sectorbuffers[0][ofs] |= (1 << bit);
	  /* increase free sector count on that track */
	  sectorbuffers[0][ofs & 0xfc]++;
	  /* write sector */
	  if (!writets (fs, 18, 0, sectorbuffers[0]))
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
      }				/* if (track<36) */
      else
      {
	ofs = ((track - 36) * 3) + (sector >> 3);
	bit = sector & 7;
	/* check bit */
	if (!(sectorbuffers[1][ofs] & (1 << bit)))
	{
	  /* its in use, but not for much longer.. */
	  sectorbuffers[1][ofs] |= (1 << bit);
	  /* increase free sector count on that track */
	  sectorbuffers[0][(track - 36) + 221]++;
	  /* write sector */
	  if (writets (fs, 53, 0, sectorbuffers[1]))
	    return (-1);
	  if (!writets (fs, 18, 0, sectorbuffers[0]))
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
fs_d71_allocateblock (fs64_filesystem * fs, int track, int sector)
{
  uchar sectorbuffers[2][256];
  int t, s;

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
    if (!fs_d71_readbam (fs, sectorbuffers))
    {
      if (fs_d71_bamalloc (track, sector, sectorbuffers))
      {
	/* block already in use */
	/* no error is given to this */
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
      else
      {
	if (!fs_d71_writebam (fs, sectorbuffers))
	  return (0);
	else
	  /* couldnt write */
	  /* writets will have set the error */
	  return (-2);
      }
    }
    else
    {
      /* couldnt read bam ! */
      /* error will have been set by readts */
      return (-2);
    }
  }
}

int
fs_d71_bamalloc (int track, int sector, uchar sectorbuffers[2][256])
{
  /* read BAM ok */
  /* calc offset */
  int ofs, bit;

  if (track < 36)
  {
    ofs = 1 + (track * 4) + (sector >> 3);
    bit = sector & 7;
    /* check bit */
    if ((sectorbuffers[0][ofs] & (1 << bit)))
    {
      /* free, but not for much longer.. */
      sectorbuffers[0][ofs] &= (0xff - (1 << bit));
      /* decrease free sector count on that track */
      sectorbuffers[0][ofs & 0xfc]--;
      /* write sector */
      return (0);
    }
    else
      /* block in use */
      return (-1);

  }				/* if (track<36) */
  else
  {
    ofs = ((track - 36) * 3) + (sector >> 3);
    bit = sector & 7;
    /* check bit */
    if ((sectorbuffers[1][ofs] & (1 << bit)))
    {
      /* free, but not for much longer.. */
      sectorbuffers[1][ofs] &= (0xff - (1 << bit));
      /* increase free sector count on that track */
      sectorbuffers[0][(track - 36) + 221]--;
      /* write sector */
      return (0);
    }
    else
    {
      return (-1);
    }
  }

}

int
fs_d71_findfreeblock (fs64_filesystem * fs, int *track, int *sector)
{
  /* find a free block on a D71 */
  uchar blocks[2][256];
  int tl[35] =
  {17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35};
  int i;

  printf ("d71: findfreeblock\n");

  /* read bam */
  if (fs_d71_readbam (fs, blocks))
    return (-1);

  /* check on first side */
  for (i = 0; i < 35; i++)
    if (i != 18)
      if (blocks[0][4 * tl[i]])
      {
	*track = tl[i];
	/* There is one on this track */
	if (blocks[0][4 * tl[i] + 1])
	{
	  *sector = firsthighbit (blocks[0][4 * tl[i] + 1]);
	  printf ("Free block is T%d S%d\n", *track, *sector);
	  return (0);
	}
	if (blocks[0][4 * tl[i] + 2])
	{
	  *sector = 8 + firsthighbit (blocks[0][4 * tl[i] + 2]);
	  printf ("Free block is T%d S%d\n", *track, *sector);
	  return (0);
	}
	if (blocks[0][4 * tl[i] + 3])
	{
	  *sector = 16 + firsthighbit (blocks[0][4 * tl[i] + 3]);
	  printf ("Free block is T%d S%d\n", *track, *sector);
	  return (0);
	}
      }

  /* check second side */
  for (i = 0; i < 35; i++)
    if (i != 18)
      if (blocks[0][220 + tl[i]])
      {
	*track = tl[i] + 35;
	/* There is one on this track */
	if (blocks[1][4 * tl[i] + 1])
	{
	  *sector = firsthighbit (blocks[1][4 * tl[i] + 1]);
	  printf ("Free block is T%d S%d\n", *track, *sector);
	  return (0);
	}
	if (blocks[1][4 * tl[i] + 2])
	{
	  *sector = 8 + firsthighbit (blocks[1][4 * tl[i] + 1]);
	  printf ("Free block is T%d S%d\n", *track, *sector);
	  return (0);
	}
	if (blocks[1][4 * tl[i] + 3])
	{
	  *sector = 16 + firsthighbit (blocks[1][4 * tl[i] + 1]);
	  printf ("Free block is T%d S%d\n", *track, *sector);
	  return (0);
	}
      }

  /* no free block */
  printf ("No free block found\n");
  *track = -1;
  *sector = -1;
  return (-1);

}

int
fs_d71_readbam (fs64_filesystem * fs, uchar blocks[2][256])
{
  /* read the two bam blocks */
  if (readts (fs, 18, 0, blocks[0]))
    return (-1);

  if (readts (fs, 53, 0, blocks[1]))
    return (-1);

  return (0);
}

int
fs_d71_writebam (fs64_filesystem * fs, uchar blocks[2][256])
{
  /* write the two bam blocks (twice in case of daft bug!) */
  if (writets (fs, 18, 0, blocks[0]))
    return (-1);
  if (writets (fs, 18, 0, blocks[0]))
    return (-1);

  if (writets (fs, 53, 0, blocks[1]))
    return (-1);
  if (writets (fs, 53, 0, blocks[1]))
    return (-1);

  return (0);
}

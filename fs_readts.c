/*
   Track/Sector read and write routines for 64net/2
   (C)Copyright Paul Gardner-Stephen 1995, All rights reserved.
   This module is inteded solely for use within the 64net/2 system
   (including 64ls,64cp,64rm and the 64net programmes)
 */

#include "config.h"
#include "fs.h"

static int d64tracks[36] =
{-1,
		   /* Tracks 1-18 */
 0, 0x1500, 0x2a00, 0x3f00, 0x5400, 0x6900, 0x7e00, 0x9e00, 0xa800,
 0xbd00, 0xd200, 0xe700, 0xfc00, 0x11100, 0x12600, 0x13b00, 0x15000, 0x16500,
		   /* Tracks 19-25 */
 0x17800, 0x18b00, 0x19e00, 0x1b100, 0x1c400, 0x1d700, 0x1ea00,
		   /* Tracks 26-31 */
 0x1fc00, 0x20e00, 0x22000, 0x23200, 0x24400, 0x25600,
		   /* Tracks 32-35 */
 0x26700, 0x27800, 0x28900, 0x29a00};

int
readts (fs64_filesystem * fs, int track, int sector, uchar *buffer)
{
  /* Read a sector into a buffer (if possible) */
  long foo;

  /* printf("readts - about to call fs_resolve_ts(1)\n"); */

  if ((foo=fs_resolve_ts (fs->media, track, sector)) < 0)
  {
    /* bad track/sector */
    /* 66,ILLEGAL BLOCK,TT,SS */
    set_error (66, track, sector);
    return (-1);
  }

  /* printf("readts - about to call fseek\n"); */

  fseek (fs->fsfile, foo, SEEK_SET);	/* seek set */
  fread(buffer,256,1,fs->fsfile);
  /* for (i = 0; i < 256; i++)
     buffer[i] = fgetc (fs->fsfile); */

  /* success! */
  return (0);
}

int
writets (fs64_filesystem * fs, int track, int sector, uchar *buffer)
{
  /* Read a sector from a buffer (if possible) */
  int i;
  long foo;

  if (fs_resolve_ts (fs->media, track, sector) < 0)
  {
    /* bad track/sector */
    /* 66,ILLEGAL BLOCK,TT,SS */
    set_error (66, track, sector);
    return (-1);
  }

  foo = fs_resolve_ts (fs->media, track, sector);
/*   printf("Writing T:%d S:%d at %lx\n",track,sector,foo);  */

  fseek (fs->fsfile, foo, SEEK_SET);	/* seek set */
  errno = 0;
  for (i=0; i < 256; i++)
    if (fputc (buffer[i], fs->fsfile) == EOF)
    {
      /* write error */
      set_error (25, track, sector);
      return (-1);
    }

  /* success! */
  return (errno);
}


long
fs_resolve_ts (int media, int track, int sector)
{
  /* calculate address in file for a given track and sector */

  long addr;

  if ((track < 1) || (track > 255) || (sector < 0) || (sector > 255))
  {
    /* track or sector is illegal */
    return (-1);
  }

  switch (media)
  {
  case media_UFS:
  case media_NET:
  case media_T64:
  case media_LNX:
    {
      /* no sectors here! */
      return (-1);
    }
  case media_D64:
    {
      /* good ol' 1541 file system */
      addr = d64tracks[track] + sector * 256;
      if (((track > 18) && (sector > 18)) || ((track > 24) && (sector > 17)) || ((track > 30) && (sector > 16)))
      {
	/* sector is illegal */
	return (-1);
      }
      if ((track > 35) || (sector > 20))
      {
	/* track or sector is too large */
	return (-1);
      }
      return (addr);
    }
  case media_D71:
    if (track < 36)
    {
      addr = d64tracks[track] + sector * 256;
      if (((track > 18) && (sector > 18)) || ((track > 24) && (sector > 17)) || ((track > 30) && (sector > 16)))
      {
	/* sector is illegal */
	return (-1);
      }
      if ((track > 35) || (sector > 20))
      {
	/* track or sector is too large */
	return (-1);
      }
    }
    else
    {
      addr = 174848 + d64tracks[track - 35] + sector * 256;
      track -= 35;
      if (((track > 18) && (sector > 18)) || ((track > 24) && (sector > 17)) || ((track > 30) && (sector > 16)))
      {
	/* sector is illegal */
	track += 35;
	return (-1);
      }
      if ((track > 35) || (sector > 20))
      {
	/* track or sector is too large */
	track += 35;
	return (-1);
      }
      track += 36;
    }
    return (addr);
  case media_D81:
    addr = (track - 1) * 10240 + (sector * 256);
    return (addr);
  case media_DHD:
    /* CMD HD/RL/RD disk image */
    /* remember to skip info block */
    addr = 256 + (track - 1) * 65536 + (sector * 256);
    return (addr);
  default:
    /* if we dont know what it is, how can we resolve the sector address */
    return (-1);
  }
}

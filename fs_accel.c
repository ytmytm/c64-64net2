/*
   64NET/2 File System Accelerator module.
   This module contains the interface to read and write sectors
   to/from the 64net/2 server *really* fast (~20KB/sec on a bland
   C64).  This is primarily designed for GEOS and similar packages,
   as well as for fast whole disk copying 
 */

#include "fs.h"
#include "machdep.h"

/* simulated RAM expander */
int georam_size=0;
uchar *georam;

int
fs_accel (void)
{
  int p=0, t, s;
  unsigned char buff[256];
  unsigned char *page;
  fs64_filesystem filesys;
  filesys.fsfile = 0;

  last_unit = 0;

  switch (charget ())
  {
  case 'I':
    /* read memory page */
    p = charget ();
    p += charget () * 256;
    if (p < georam_size)
      /* valid page */
      page = &georam[p * 256];
    else
      /* dud page */
      page = &buff[0];
    /* send it! */
    fishsendblock (256, buff);
    return (0);
  case 'O':
    /* write memory page */
    p = charget ();
    p += charget () * 256;
    if (p < georam_size)
      /* valid page */
      page = &georam[p * 256];
    else
      /* dud page */
      page = &buff[0];
    /* send it! */
    fishgetblock (256, buff);
    return (0);
  case 'S':
    /* size memory */
    sendchar (georam_size & 0xff);
    sendchar (georam_size / 256);
    return (0);
  case 'R':
    /* read sector */
    /* get parameters */
    p = charget ();
    t = charget ();
    s = charget ();
    /* open file system */
    if (!curr_dir[last_unit][p][0])
    {
      sendchar (77);		/* illegal partition */
      return (-1);
    }
    strcpy (filesys.fspath, curr_dir[last_unit][p]);
    /* get media type */
    filesys.media = fs64_mediatype (filesys.fspath);
    switch (filesys.media)
    {
    case media_D64:
    case media_D71:
    case media_D81:
    case media_DHD:
      break;
    case media_BAD:
    case media_NOTFS:
    case media_NET:
    case media_UFS:
    default:
      /* media type mismatch */
      sendchar (76);
      return (-1);
    }
    if ((filesys.fsfile = fopen (filesys.fspath, "r")) == NULL)
    {
      /* couldn't open it */
      /* 74,DRIVE NOT READY,00,00 */
      set_error (74, 0, 0);
      sendchar (74);
      return (-1);
    }
    /* read it */
    if (readts (&filesys, t, s, buff))
    {
      /* barf! (read error) */
      sendchar (21);
      fclose (filesys.fsfile);
      filesys.fsfile = 0;
      return (-1);
    }
    fclose (filesys.fsfile);
    filesys.fsfile = 0;
    fishsendblock (256, buff);
    return (0);
  case 'W':
    /* write sector */
    /* get parameters */
    p = charget ();
    t = charget ();
    s = charget ();
    /* open file system */
    if (!curr_dir[last_unit][p][0])
    {
      sendchar (77);		/* illegal partition */
      return (-1);
    }
    strcpy (filesys.fspath, curr_dir[last_unit][p]);
    /* get media type */
    filesys.media = fs64_mediatype (filesys.fspath);
    switch (filesys.media)
    {
    case media_D64:
    case media_D71:
    case media_D81:
    case media_DHD:
      break;
    case media_BAD:
    case media_NOTFS:
    case media_NET:
    case media_UFS:
    default:
      /* media type mismatch */
      sendchar (76);
      return (-1);
    }
    if ((filesys.fsfile = fopen (filesys.fspath, "r")) == NULL)
    {
      /* couldn't open it */
      /* 74,DRIVE NOT READY,00,00 */
      set_error (74, 0, 0);
      sendchar (74);
      return (-1);
    }
    fishgetblock (256, buff);
    /* write it */
    if (writets (&filesys, t, s, buff))
    {
      /* barf! (write error) */
      /* BUG: how can GEOS tell??? */
      fclose (filesys.fsfile);
      filesys.fsfile = 0;
      return (-1);
    }
    fclose (filesys.fsfile);
    filesys.fsfile = 0;
    return (0);
  case 'P':
    /* get partition info (ie media type) */
    if (!curr_dir[last_unit][p][0])
    {
      /* undefined partition */
      sendchar (0xff);
    }
    else
    {
      t = fs64_mediatype (curr_dir[last_unit][p]);
      switch (t)
      {
      case media_BAD:
      case media_NOTFS:
	sendchar (0xff);
      default:
	sendchar (t);
      }
    }
    return (0);
  }

  /* invalid request made */
  return(0);
}

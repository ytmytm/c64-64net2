/*
   64NET/2 File System Accelerator module.
   This module contains the interface to read and write sectors
   to/from the 64net/2 server *really* fast (~20KB/sec on a bland
   C64).  This is primarily designed for GEOS and similar packages,
   as well as for fast whole disk copying 
 */

#include "config.h"
#include "fs.h"
#include "machdep.h"
#include "comm-work.h"
#include "fs_accel.h"

/* simulated RAM expander */
int georam_size=0;
uchar *georam;

int
fs_accel (uchar command)
{
  int p=0, t, s, i, n;
  uchar buff[256];
  uchar *page;
  fs64_filesystem filesys;
  filesys.fsfile = 0;

  last_unit = 0;

  switch (command) {

  case FSA_READ_PAGE:
  case FSA_READ_MEMORY:
    if (command==FSA_READ_MEMORY) n=charget()+1; else n=256;
    p = charget ();
    p += charget () * 256;
    if (p < georam_size)
      /* valid page */
      page = &georam[p * 256];
    else
      /* dud page */
      page = &buff[0];
    /* send it! */
    if (allowFishLoad) fishsendblock (n, buff);
	else
	for (i=0;i<n;i++) sendchar(buff[i]);
    return (0);

  case FSA_WRITE_PAGE:
  case FSA_WRITE_MEMORY:
    if (command==FSA_WRITE_MEMORY) n=charget()+1; else n=256;
    p = charget ();
    p += charget () * 256;
    if (p < georam_size)
      /* valid page */
      page = &georam[p * 256];
    else
      /* dud page */
      page = &buff[0];
    /* send it! */
    if (allowFishSave) fishgetblock (n, buff);
	else
	for (i=0;i<n;i++) buff[i]=charget();
    return (0);

  case FSA_SIZE_MEMORY:
    sendchar (georam_size & 0xff);
    sendchar (georam_size / 256);
    return (0);

  case FSA_READ_SECTOR:
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
    sendchar(0);	/* no error */
    if (allowFishLoad) fishsendblock (256, buff);
	else
	for (i=0;i<256;i++) sendchar(buff[i]);
    return (0);

  case FSA_WRITE_SECTOR:
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
    sendchar(0);	/* no error so far */
    if (allowFishSave) fishgetblock (256, buff);
	else
	for (i=0;i<256;i++) buff[i]=charget();
    /* write it */
    if (writets (&filesys, t, s, buff))
    {
      /* barf! (write error) */
      sendchar(28);
      fclose (filesys.fsfile);
      filesys.fsfile = 0;
      return (-1);
    }
    sendchar(0);
    fclose (filesys.fsfile);
    filesys.fsfile = 0;
    return (0);

  case FSA_PAR_INFO:
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

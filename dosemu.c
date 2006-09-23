/*
   CBM/CMD DOS Emulation module for 64NET/2
   (C)Copyright Paul Gardner-Stephen 1995, 1996, All rights reserved.

   NOTE: This code is, in part, based upon reading manuals and documentation
   by both Commodore Business Machines (CBM), and Creative Micro Designs (CMD)
   and as such is a legal reverse engineering project.  It is hereby 
   testified by myself, Paul Gardner-Stephen, that the above statement is true
   and that this module contains only original work by myself, and no work by
   either CBM or CMD.
 */

#include "config.h"
#include "fs.h"
#include "fs_func.h"
#include "dosemu.h"
#include "misc_func.h"
#include "comm-rrnet.h"

int which_unit (int);

/* virtual drive memory */
uchar drive_memory[MAX_CLIENTS][0x10000];

void
set_current_dir (uchar *foo)
{
  /* set current directory on current partition of current drive */

  /* de-allocate current dir */
  if (curr_dir[curr_client][curr_par[curr_client]])
    free (curr_dir[curr_client][curr_par[curr_client]]);
  /* set new one */
  if (!(curr_dir[curr_client][curr_par[curr_client]] = (uchar *)
	malloc (strlen (foo) + 1)))
    /* couldnt malloc */
    fatal_error ("Cannot allocate memory.");
  else
    strcpy ((char*)curr_dir[curr_client][curr_par[curr_client]],(char*) foo);
}

void
reset_drive (void)
{
  /* reset last_drive
     This routine is called whenever a UJ disk command is sent, or similar
     reset means are employed */

  int i;

  /* cutesy startup message */
  set_error (73, 0, 0);
  /* clear incoming command channel */
  if (curr_client > -1)
    dos_comm_len[curr_client] = 0;

  /* reset subdirectories */
  for (i = 1; i < 255; i++)
  {
    curr_par[curr_client] = i;
    if (partn_dirs[curr_client][curr_par[curr_client]])
      set_current_dir (partn_dirs[curr_client][curr_par[curr_client]]);
  }				/* for(i=1;i<255;i++) */

  /* set default partition to 1 */
  curr_par[curr_client] = 1;

}

void
init_dos (void)
{
  /* clear all things which need be cleared, and set default DOS messages.
     This routine is called whenever the DOS system is completely reset.
   */

  int i;

  /* BUGS: This module does not have access to the MAX_NET_DRVS #define
     so we employ this mucky work around */

  for (i = 1; i < MAX_CLIENTS; i++)
  {
    curr_client = which_unit (i);
    /* reset last_drive */
    if (curr_client != -1)
      reset_drive ();
  }
  memset(drive_memory,0,MAX_CLIENTS*0x10000);
  for (i=0;i<MAX_CLIENTS;i++) {
    drive_memory[i][0x69]=0x08; 	/* interleave set */
    drive_memory[i][0xe5c6]='4';	/* end of UJ message DOS VERSION, DRIVE - mimic 1541 */
    }
}

int
do_dos_command (void)
{
	//XXX missing the rename command
  /* execute the DOS command in the buffer */
  int i = curr_client, dt = -1, ds = -1;
  /* scratch vars */
  int j, k, par, rv;
  uchar path[1024], partition[8];
  uchar temp[1024];
  uchar name[80], id[80];
  debug_msg ("DOS Command: [%s]\n", dos_command[i]);
  if (dos_comm_len[i] == 0)
    /* null command needs null action */
    return (0);

    /* strip "0:" off the DOS command, leave only ':' */

//  for (j=0;j<=dos_comm_len[i];j++) {
//    if ((dos_command[i][j]=='0')&&(dos_command[i][j+1]==':')) {
//	for (k=j+1;k<=dos_comm_len[i];k++)
//	    dos_command[i][k-1]=dos_command[i][k];
//	dos_comm_len[i] = dos_comm_len[i]-1;
//    }
//  }
// debug_msg ("DOS Command: [%s]\n", dos_command[i]);


  /* parse out */
  switch (dos_command[i][0])
  {
  case 'M':
    {
      /* MkDir */
      switch (dos_command[i][1])
      {
      case '-':
        /* M-R and M-W */
        switch (dos_command[i][2])
        {
	case 'R':
	    { int addr, k;
	      addr = dos_command[i][3]+256*dos_command[i][4];
	      debug_msg("M-R on $%04x for $%02x\n",addr,dos_command[i][5]);
	      for (k=0;k<dos_command[i][5];k++) {
	        dos_status[i][k]=drive_memory[i][addr+k];
		}
	      dos_stat_len[i]=dos_command[i][5];
	      dos_comm_len[i] = 0;
	      return 0;
	    }
	    break;
	case 'W':
	    { int addr, k;
	      addr = dos_command[i][3]+256*dos_command[i][4];
	      debug_msg("M-W on $%04x for $%02x\n",addr,dos_command[i][5]);
	      for (k=0;k<dos_command[i][5];k++)
	        drive_memory[i][addr+k]=dos_status[i][k+6];
	      dos_comm_len[i] = 0;
	      set_error (0, 0, 0);
	      return 0;
	    }
	    break;
	}
	break;
      case 'D':
	/* make conventional dir */
	dos_comm_len[i] = 0;
	set_error (38, 0, 0);
	return (-1);
	break;
      case 'd':
      case 196:
	{
	  /* make special dir */
	  int dt = dos_command[i][2];
	  int media = 0, par, blocks = 0xffffff, size = 0;
	  uchar path[1024], sname[256], lname[256], ext[8];
	  fs64_filesystem fs;
	  fs.fsfile = 0;
	  switch (dt)
	  {
	  case '4':
	  case '6':		/* 1541 (D64) */
	    media = media_D64;
	    blocks = 683 + 1;
	    break;
	  case '7':		/* 1571 (D71) */
	    media = media_D71;
	    blocks = 683 * 2 + 1;
	    break;
	  case '8':		/* 1581 (D81) */
	    media = media_D81;
	    blocks = 80 * 40 + 1;
	    break;
	  case 'N':
	  case 'H':		/* CMD "Native" (DHD) */
	    media = media_DHD;
	    if (!dos_command[i][3])
	      dos_command[i][3] = 0xff;
	    blocks = 256 * dos_command[i][3] + 2;
	    size = dos_command[i][3];
	    break;
	  case 'T':		/* T64 */
	  case 'L':		/* LNX */
	    set_error (38, 0, 0);
	    return (-1);
	  default:
	    /* 79,INVALID MEDIA,00,00 */
	    set_error (79, 0, 0);
	  }
	  /* find where `:' is (if at all) */
	  lname[0] = 0;
	  for (j = 4; j < dos_comm_len[i]; j++)
	    if (dos_command[i][j] == ':')
	    {
	      strcpy ((char*)lname, (char*)&dos_command[i][j + 1]);
	      if (!lname[0])
	      {
		/* missing filename */
		set_error (34, 0, 0);
		dos_comm_len[i] = 0;
		return (-1);
	      }
	      dos_command[i][j + 1] = 0;
	      dos_comm_len[i] = j;
	      break;
	    }
	  if (!lname[0])
	  {
	    /* no colon, so its all filename */
	    strcpy ((char*)lname, (char*)&dos_command[i][4]);
	    dos_command[i][4] = ':';
	    dos_command[i][5] = 0;
	  }
	  /* find out about host filesystem */
	  if (fs64_parse_path (&dos_command[i][4], path, &par, &dt, &ds))
	  {
	    /* fs64_parse_filespec will have set the error */
	    dos_comm_len[i] = 0;
	    return (-1);
	  }
	  debug_msg ("Md: create [%s] in [%s]\n", lname, path);
	  /* create a fs64_filesystem */
	  if (fs_pathtofilesystem (&fs, path))
	  {
	    /* fs_pathtofilesystem() will have set the error */
	    debug_msg ("Could not make file system\n");
	    return (-1);
	  }
	  /* ensure host media is suitable */
	  switch (fs.media)
	  {
	  case media_D64:
	  case media_D71:
	  case media_D81:
	  case media_DHD:
	    /* 78,recursive filesystem,00,00 */
	    set_error (78, 0, 0);
	    return (-1);
	    break;
	  case media_NET:
	  case media_LNX:
	  case media_T64:
	    /* 76,MEDIA TYPE MISMATCH,00,00 */
	    set_error (76, 0, 0);
	    return (-1);
	    break;
	  case media_UFS:
	    /* bingo! */
	    break;
	  default:
	    /* 76,MEDIA TYPE MISMATCH,00,00 */
	    set_error (76, 0, 0);
	    return (-1);
	  }
	  /* ensure sufficient space */
	  if (fs64_blocksfree (&fs) < blocks)
	  {
	    /* 52,file too large,00,00 */
	    set_error (52, 0, 0);
	    return (-1);
	  }
	  /* okey.. create a new disk image, with the given name
	     (use shortname() to resolve) */
	  switch (media)
	  {
	  case media_D64:
	    strcpy ((char*)ext, ".d64");
	    break;
	  case media_D71:
	    strcpy ((char*)ext, ".d71");
	    break;
	  case media_D81:
	    strcpy ((char*)ext, ".d81");
	    break;
	  case media_DHD:
	    strcpy ((char*)ext, ".dhd");
	    break;
	  default:
	    set_error (38, 0, 0);
	    return (-1);
	  }
	  /* filter crusty chars */
	  for (k = 0; k < (int)strlen (lname); k++)
	    switch (lname[k])
	    {
	    case ',':
	    //case '.':
	    //case '+':
	    case '/':
	    case '\\':
	    case '*':
	    case '?':
	    case ':':
	    case ';':
	    case '~':
	      set_error (33, 0, 0);
	      dos_comm_len[i] = 0;
	      return (-1);
	    default:
		  break;
	    }
	  strcpy ((char*)sname, (char*)lname);
	  sprintf ((char*)path, "%s%s%s", path, sname, ext);
	  debug_msg ("Md: create [%s] of %d blocks\n", path, blocks);
	  /* create physical file */
	  fs.media = media;
	  if ((fs.fsfile = fopen ((char*)path, "r")) != NULL)
	  {
	    /* already exists */
	    fclose (fs.fsfile);
	    fs.fsfile = 0;
	    debug_msg ("Dir exists\n");
	    set_error (63, 0, 0);
	    dos_comm_len[i] = 0;
	    return (-1);
	  }
	  if ((fs.fsfile = fopen ((char*)path, "w+")) == NULL)
	  {
	    /* could not open new file system */
	    debug_msg ("Could not open new file system\n");
	    set_error (74, 0, 0);
	    dos_comm_len[i] = 0;
	    return (-1);
	  }
	  else
	  {
	    if (media == media_DHD)
	    {
	      /* write DHD info block */
	      uchar temp[256] =
	      {0};
	      int i;
	      sprintf ((char*)temp, "DHD %02x TRACKS\nPRG [64NET/2 v00.01 ALPHA            ]\n%c%c",
		       size, 4, size);
	      for (i = 0; i < 256; i++)
		fputc (temp[i], fs.fsfile);
	    }
	  }
	  fclose (fs.fsfile);
	  fs.fsfile = 0;

	  switch (fs.media)
	  {
	  case media_D64:
	  case media_D71:
	  case media_D81:
	  case media_DHD:
	    {
	      /* create file system */
	      if (fs_pathtofilesystem (&fs, path))
	      {
		dos_comm_len[i] = 0;
		return (-1);
	      }
	      /* format it! */
	      debug_msg ("Format please!\n");
	      if (fs_dxx_format (&fs, lname, (uchar*)"01"))
	      {
		dos_comm_len[i] = 0;
		fclose (fs.fsfile);
		fs.fsfile = 0;
		return (-1);
	      }
	      break;
	    }
	  default:
	    {
	      set_error (38, 0, 0);
	      dos_comm_len[i] = 0;
	      fclose (fs.fsfile);
	      fs.fsfile = 0;
	      return (-1);
	    }
	  }
	  dos_comm_len[i] = 0;
	  fclose (fs.fsfile);
	  fs.fsfile = 0;
	  return (0);
	  break;
	}
      };
      break;
    };
  case 'N':
    {
      /* Format disk image */
      uchar valcmd[4] = "NEW";
      int par, k, l = 0;
      uchar path[1024];
      /* get partition number (or `alidate' then partition #) */
      /* (j will contain the offset to the partition) */
      for (j = 1; dos_command[i][j] == valcmd[j]; j++);
      debug_msg ("Parsing [%s]\n", &dos_command[i][j]);
      /* Format a disk image or filesystem in general */
      for (k = j; k < dos_comm_len[i]; k++)
	if (dos_command[i][k] == ':')
	{
	  l = dos_command[i][k + 1];
	  dos_command[i][k + 1] = 0;
	  break;
	}
      if (fs64_parse_path (&dos_command[i][j], path, &par, &dt, &ds))
      {
	/* fs64_parse_filespec will have set the error */
	dos_comm_len[i] = 0;
	return (-1);
      }
      dos_command[i][k + 1] = l;
      debug_msg ("Format: Path [%s] par:%d\n", path, par);
      id[0] = 0;
      name[0] = 0;
      for (j = k + 1; j < dos_comm_len[i]; j++)
	if (dos_command[i][j] != ',')
	  sprintf ((char*)name, "%s%c", name, dos_command[i][j]);
	else
	{
	  dos_command[i][j + 3] = 0;
	  strcpy ((char*)id, (char*)&dos_command[i][j + 1]);
	  if (!id[1])
	    id[1] = 0xa0;
	  break;
	}
      name[16] = 0;
      debug_msg ("Name/id: [%s][%s]\n", name, id);

      {
	/* construct filesystem (or abort) */
	int mt = fs64_mediatype (path);
	switch (mt)
	{
	case media_D64:
	case media_D71:
	case media_D81:
	case media_DHD:
	  {
	    /* disk image */
	    fs64_filesystem fs;
	    fs.fsfile = 0;
	    strcpy ((char*)fs.fspath, (char*)path);
	    fs.media = mt;
	    switch (mt)
	    {
	    case media_D64:
	      fs.dirtrack = 18;
	      fs.dirsector = 0;
	      break;
	    case media_D71:
	      fs.dirtrack = 18;
	      fs.dirsector = 0;
	      break;
	    case media_D81:
	      fs.dirtrack = 40;
	      fs.dirsector = 0;
	      break;
	    case media_DHD:
	      fs.dirtrack = 1;
	      fs.dirsector = 1;
	      break;
	    }
	    if ((fs.fsfile = fopen ((char*)path, "r+")) == NULL)
	    {
	      set_error (74, 0, 0);
	      dos_comm_len[i] = 0;
	      return (-1);
	    }
	    /* format */
	    if (fs_dxx_format (&fs, name, id))
	    {
	      /* gark! */
	      dos_comm_len[i] = 0;
	      return (-1);
	    }
	    else
	    {
	      /* all done */
	      dos_comm_len[i] = 0;
	      set_error (0, 0, 0);
	      return (0);
	    }
	    break;
	  }			/* case media_D?? */
	default:
	  /* 76,MEDIA TYPE MISMATCH,00,00 */
	  set_error (76, 0, 0);
	  dos_comm_len[i] = 0;
	  return (-1);
	}			/* switch(mt) */
      }
      break;
    }
  case 118:
  case 214:			/* shift-V (both c64 codes) */
    {
      /* validate with purge */
      uchar valcmd[9] = "VALIDATE";
      int par;
      uchar path[1024];
      /* get partition number (or `alidate' then partition #) */
      /* (j will contain the offset to the partition) */
      for (j = 1; dos_command[i][j] == valcmd[j]; j++);
      debug_msg ("Parsing [%s]\n", &dos_command[i][j]);
      if (fs64_parse_path (&dos_command[i][j], path, &par, &dt, &ds))
      {
	/* fs64_parse_filespec will have set the error */
	dos_comm_len[i] = 0;
	return (-1);
      }

      debug_msg ("Validate: Path [%s] par:%d\n", path, par);

      {
	/* construct filesystem (or abort) */
	int mt = fs64_mediatype (path);
	switch (mt)
	{
	case media_D64:
	case media_D71:
	case media_D81:
	case media_DHD:
	  {
	    /* disk image */
	    fs64_filesystem fs;
	    fs.fsfile = 0;
	    strcpy ((char*)fs.fspath, (char*)path);
	    fs.media = mt;
	    switch (mt)
	    {
	    case media_D64:
	      fs.dirtrack = 18;
	      fs.dirsector = 0;
	      break;
	    case media_D71:
	      fs.dirtrack = 18;
	      fs.dirsector = 0;
	      break;
	    case media_D81:
	      fs.dirtrack = 40;
	      fs.dirsector = 0;
	      break;
	    case media_DHD:
	      fs.dirtrack = 1;
	      fs.dirsector = 1;
	      break;
	    }
	    if ((fs.fsfile = fopen ((char*)path, "r+")) == NULL)
	    {
	      set_error (74, 0, 0);
	      dos_comm_len[i] = 0;
	      return (-1);
	    }
	    /* validate (plus error correction) */
	    if (fs_dxx_validate (&fs, 1))
	    {
	      /* gark! */
	      dos_comm_len[i] = 0;
	      return (-1);
	    }
	    else
	    {
	      /* all done */
	      dos_comm_len[i] = 0;
	      return (0);
	    }
	    break;
	  }			/* case media_D?? */
	default:
	  /* 76,MEDIA TYPE MISMATCH,00,00 */
	  set_error (76, 0, 0);
	  dos_comm_len[i] = 0;
	  return (-1);
	}			/* switch(mt) */
      }
      break;
    }
  case 'V':
    {
      /* validate */
      uchar valcmd[9] = "VALIDATE";
      int par;
      uchar path[1024];
      /* get partition number (or `alidate' then partition #) */
      /* (j will contain the offset to the partition) */
      for (j = 1; dos_command[i][j] == valcmd[j]; j++);
      debug_msg ("Parsing [%s]\n", &dos_command[i][j]);
      if (fs64_parse_path (&dos_command[i][j], path, &par, &dt, &ds))
      {
	/* fs64_parse_filespec will have set the error */
	dos_comm_len[i] = 0;
	return (-1);
      }

      debug_msg ("Validate: Path [%s] par:%d\n", path, par);

      {
	/* construct filesystem (or abort) */
	int mt = fs64_mediatype (path);
	switch (mt)
	{
	case media_D64:
	case media_D71:
	case media_D81:
	case media_DHD:
	  {
	    /* disk image */
	    fs64_filesystem fs;
	    fs.fsfile = 0;
	    strcpy ((char*)fs.fspath, (char*)path);
	    fs.media = mt;
	    switch (mt)
	    {
	    case media_D64:
	      fs.dirtrack = 18;
	      fs.dirsector = 0;
	      break;
	    case media_D71:
	      fs.dirtrack = 18;
	      fs.dirsector = 0;
	      break;
	    case media_D81:
	      fs.dirtrack = 40;
	      fs.dirsector = 0;
	      break;
	    case media_DHD:
	      fs.dirtrack = 1;
	      fs.dirsector = 1;
	      break;
	    }
	    if ((fs.fsfile = fopen ((char*)path, "r+")) == NULL)
	    {
	      set_error (74, 0, 0);
	      dos_comm_len[i] = 0;
	      return (-1);
	    }
	    /* validate (no error correction) */
	    if (fs_dxx_validate (&fs, 0))
	    {
	      /* gark! */
	      dos_comm_len[i] = 0;
	      return (-1);
	    }
	    else
	    {
	      /* all done */
	      dos_comm_len[i] = 0;
	      return (0);
	    }
	    break;
	  }			/* case media_D?? */
	default:
	  /* 76,MEDIA TYPE MISMATCH,00,00 */
	  set_error (76, 0, 0);
	  dos_comm_len[i] = 0;
	  return (-1);
	}			/* switch(mt) */
      }
      break;
    }
  case 'I':
    /* soft reset drive */
    set_error (0, 0, 0);
    dos_comm_len[i] = 0;
    return (0);
    break;
  case 'U':
    {
      /* Ux disk commands, eg reset, read sector, write sector */
      switch (dos_command[i][1])
      {
      case '1':
      case 'A':
	rv = dos_u1 (dos_command[i], dos_comm_len[i], i);
	dos_comm_len[i] = 0;
	dos_command[i][0] = 0;
	return (rv);
      case '2':
      case 'B':
	/* sector-write */
	/* unimplemented */
	set_error (38, 0, 0);
	dos_comm_len[i] = 0;
	return (0);
      case '3':
      case 'C':
	/* JMP $0500 */
	/* unimplemented */
	set_error (38, 0, 0);
	dos_comm_len[i] = 0;
	return (0);
      case '4':
      case 'D':
	/* JMP $0503 */
	/* unimplemented */
	set_error (38, 0, 0);
	dos_comm_len[i] = 0;
	return (0);
      case '5':
      case 'E':
	/* JMP $0506 */
	/* unimplemented */
	set_error (38, 0, 0);
	dos_comm_len[i] = 0;
	return (0);
      case '6':
      case 'F':
	/* JMP $0509 */
	/* unimplemented */
	set_error (38, 0, 0);
	dos_comm_len[i] = 0;
	return (0);
      case '7':
      case 'G':
	/* JMP $050C */
	/* unimplemented */
	set_error (38, 0, 0);
	dos_comm_len[i] = 0;
	return (0);
      case '8':
      case 'H':
	/* JMP $050F */
	/* unimplemented */
	set_error (38, 0, 0);
	dos_comm_len[i] = 0;
	return (0);
      case '9':
      case 'I':
	/* soft-reset (does nothing) */
	set_error (0, 0, 0);
	dos_comm_len[i] = 0;
	return (0);
      case 'J':
      case ':':
	/* hard-reset */
	reset_drive ();
	dos_comm_len[i] = 0;
	return (0);
      default:
	set_error (30, 0, 0);
	dos_comm_len[i] = 0;
	return (0);
      }				/* end of Ux commands */
    }
  case 'C':
    {
      /* CD,CP etc */
      switch (dos_command[i][1])
      {
      case 'D':
	{
	  /* CD - change subdirectories */
	  /* ALGORITHM (revised):

	     1. Is it a "pwd" ("CD")
	     2. Change : to / and post-pend a : 
	     3. fs64_parse_partition it (like for open etc)
	   */

	  /* get rid of trailing CR's */
	  j = dos_comm_len[i] - 1;
	  while (dos_command[i][j] == 0x0d)
	  {
	    dos_command[i][j] = 0;
	    dos_comm_len[i] -= 1;
	    j--;
	  }

	  if (dos_comm_len[i] == 2)
	  {
	    /* return current dir in status */
	    sprintf ((char*)temp, "%02d,%s,%02d,%02d\r",
		     0, (char *) curr_dir[i][curr_par[i]], 0, 0);
	    set_drive_status (temp, strlen (temp));
	    dos_comm_len[i] = 0;
	    return (0);
	  }

	  par = 0;
	  j = dos_comm_len[i] - 1;
	  for (k = 2; k <= j; k++)
	    switch (dos_command[i][k])
	    {
	    case ':':
	      /* change : to / */
	      dos_command[i][k] = '/';
	      k += 0;
	      j = 0;
	      break;
	    case '/':
	      /* partition before / 
	         (ie no :, so same as for :, but no need to alter */
	      k += 0;
	      j = 0;
	      break;
	    case '_':
	      /* partition before <- 
	         Dont advance k, as <- is the entire path */
	      k--;
	      j = 0;
	      break;
	    case '1':
	    case '2':
	    case '3':
	    case '4':
	    case '5':
	    case '6':
	    case '7':
	    case '8':
	    case '9':
	    case '0':
	      /* digit for partition */
	      par *= 10;
	      par += dos_command[i][k] - 0x30;
	      break;
	    default:
	      /* bad char in CD */
	      set_error (33, 0, 0);
	      dos_comm_len[i] = 0;
	      return (-1);
	    }
	  j = dos_comm_len[i];
	  dos_command[i][j] = 0;

	  debug_msg ("Par: %d, Path [%s]\n", par, &dos_command[i][k]);

	  /* step 2 - fs64_parse_partitions */
	  strcpy ((char*)path, (char*)&dos_command[i][k]);
	  sprintf ((char*)partition, "%d", par);
	  if (fs64_resolve_partition (partition, path, &dt, &ds))
	    /* parse_filespec will have set the error */
	  {
	    dos_comm_len[i] = 0;
	    return (-1);
	  }
	  if (!par) par = curr_par[i];
	  debug_msg ("Parsed path: [%d][%s]\n", par, path);
	  if (curr_dir[i][par]) free (curr_dir[i][par]);
	  curr_dir[i][par] = (uchar *) malloc (strlen (path) + 1);
	  strcpy ((char*)curr_dir[i][par], (char*)path);
	  /* and update dir block */
	  curr_dirtracks[i][par] = dt;
	  curr_dirsectors[i][par] = ds;
	  set_error (0, 0, 0);
	  dos_comm_len[i] = 0;
	  return (0);
	}			/* end of CD */
      case 'P':
      case 'p':
	/* change current partition */
	if (dos_command[i][1] == 'p')
	{
	  /* chr$ version of CP */
	  j = dos_command[i][2];
	}
	else
	{
	  if (dos_command[i][2] == ':')
	    /* skip colon */
	    j = (int)(atof (&dos_command[i][3]));
	  else
	    j = (int)(atof (&dos_command[i][2]));
	}
	if (j == 0)
	{
	  /* only show current partition */
	  set_error (2, curr_par[i], 0);
	}
	else
	{
	  if (partn_dirs[i][j])
	  {
	    /* partition exists */
	    curr_par[i] = j;
	    set_error (2, j, 0);
	  }
	  else
	    /* no such partition */
	    set_error (77, j, 0);
	}			/* if (j==0) (are we getting or setting partition) */
	dos_comm_len[i] = 0;
	return (0);
	break;
      case 'O':
	/* COPY ?? */
	/* fallthough to 'C:' */
      case ':':
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
	/* C: or Cn: means copy */
	set_error (38, 0, 0);	/* unimplemented */
	break;
      default:
	/* unknown Cx command */
	set_error (30, 0, 0);
	dos_comm_len[i] = 0;
	return (0);
	break;
      }				/* end of C..... command dispatch */
    }				/* end case 'c' */
    break;
  case 'S':
	{ int k;
	    for (k=0;(k<dos_comm_len[i])&&(dos_command[i][k]!=':');k++);
	    if (k==dos_comm_len[i]) {
		set_error (30, 0, 0);
		dos_comm_len[i] = 0;
		return (1);
	    }
	    fs64_scratchfile_g(&dos_command[i][k+1]);
	    dos_comm_len[i] = 0;
	    return (0);
	}
  default:
    /* unknown command */
    set_error (31, 0, 0);
    dos_comm_len[i] = 0;
    return (0);
    break;
  }				/* end of master command dispatch switch */

  /* generic disk command error */
  set_error (30, 0, 0);
  dos_comm_len[i] = 0;
  return (0);

}


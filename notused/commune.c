/* 
   64NET/2 Commune module
   (C)Copyright Paul Gardner-Stephen 1995, All Rights Reserved.

   This module does all the C64 <--> Server communications
 */

/*
   BUGS:
   This programme will have to be greatly generalised as time progresses
   At present it will hopefully support the full drive redirection scope
   intended.  Communication device generalisation will be next on the 
   list.
 */

#include "commune.h"

int
commune ()
{
  /* The actual programme loop which does the work of talking to the C64/C128
     or C65.
     This should be sufficiently general to work over serial, parallel or 
     otherwise (but not real IEC buss).
   */

  int a, i, j;
  unsigned char c;

  if ((f = fopen ("/dev/tty00", "r+")) == NULL)
  {
    fatal_error ("Cannot open TTY\n");
    exit (1);
  }

  while (1)
  {
    a = iecin ();
    if (a == COMMCHAR2)
    {
      /* we are being asked to send a character */
      if (talklf < 0)
      {
	debug_msg ("Being asked to send a char with no talker specified.\n");
	fputc (COMMCHAR, f);
	fputc (4, f);
	set_error (70, 0, 0);
      }
      else
      {
	i = which_unit (talkdev);
	last_drive = talkdev & 0x1f;
	last_unit = which_unit (last_drive);
	if (!(i < 0))
	{
	  /* no lf, no go */
	  if (talklf < 0)
	  {
	    fputc (COMMCHAR, f);
	    fputc (255, f);	/* load error */
	    set_error (70, 0, 0);
	  }
	  else if (talklf == 0x0f)
	  {
	    /* command channel */
	    if (!dos_stat_len[i])
	      set_error (0, 0, 0);	/* default to OK message */

	    /* send char */
	    switch (dos_status[i][0])
	    {
	    case COMMCHAR:
	      fputc (COMMCHAR, f);
	      fputc (0x00, f);
	      break;
	    case COMMCHAR2:
	      fputc (COMMCHAR, f);
	      fputc (0x01, f);
	      break;
	    default:
	      fputc (dos_status[i][0], f);
	    }

	    /* update dos status */
	    for (j = 0; j <= dos_stat_len[i]; j++)
	      dos_status[i][j] = dos_status[i][j + 1];

	    /* reduce length remainin */
	    dos_stat_len[i]--;
	  }			/* end of command channel read */
	  else if (fs64_readchar (&logical_files[i][talklf], &c))
	  {
	    /* couldnt read a char */
	    debug_msg ("EOF on lf#%d, IEC $%02x\n", talklf, talkdev);
	    fputc (COMMCHAR, f);
	    /* set status to 4 (time out) */
	    /* returns 66 on not found, and 64 on eof */
	    /* not found is defined by inability to read even a
	       single char from the file */
	    fputc (64 + lf_flags[talklf], f);
	  }			/* end of failed to read from channel */
	  else
	  {
	    lf_flags[talklf] = 0;	/* file is found */
	    fputc (c, f);
	    /* if we just sent the command char, send a $00
	       to indicate we meant the char, and not a command */
	    if (c == COMMCHAR)
	      fputc (0x00, f);
	  }
	}			/* end of device exists */
      }
    }
    else
    {
      /* not a command char */
      if (a < 0)
      {
	/* only attn was altered */
	if (attn == 0)
	{
	  /* attn has been de-asserted, so process atn command */
	  actonattncmd ();
	}
	else
	{
	  /* attn has been asserted - do nothing, but chars received
	     now may have special meaning */
	}
      }
      else
      {
	/* received a real char */
	received (a);
      }
    }
  }
}

int
actonattncmd ()
{
  /* act on any attention command received */

  int a, i, j, listenlf;

  if (attndev)
  {
    if (attndev == 0x3f)
    {
      /* global shut up (unlisten ?) */
      attndev = 0;
      return (0);
    }
    if (attndev == 0x5f)
    {
      /* global shut up (untalk) */
      attndev = 0;
      return (0);
    }
    last_drive = attndev & 0x1f;
    last_unit = which_unit (last_drive);

    if (attncmd == 0)
    {
      /* if no attention command, read next byte and use that */
      a = -1;
      debug_msg ("Obnoxious error pending (source unknown)\n");
      while (a < 0)
	a = iecin ();
      attncmd = a;
    }

    switch (attncmd & 0xf0)
    {
    case OPEN:
      /* open a file */
      /* receive chars until attn is asserted */
      fnlen = 0;
      while (!attn)
      {
	a = iecin ();
	if (a >= 0)
	  filename[fnlen++] = a;
	/* c64 SHIFT -> IBM lower case
	   (only if not command channel */
	if ((attncmd & 0x0f) != 0x0f)
	  if ((a > 192) && (a < (192 + 27)))
	    filename[fnlen - 1] = a - 0x60;
      }
      filename[fnlen] = 0;
      /* add trailers as needed for "special" channels */
      if (attncmd & 0xf == 0)
	strcat (filename, ",R");
      if (attncmd & 0xf == 1)
	strcat (filename, ",W");
      if (filename[0] < 0x20)
	strcpy (filename, filename + 1);
      /* find the unit number */
      for (i = 0; i < MAX_NET_DEVS; i++)
	if (devices[i] == (attndev & 0x1f))
	{
	  /* now *really* open the file */
	  debug_msg ("Filename: \"%s\"\n", filename);
	  if ((attncmd & 0x0f) < 0x0f)
	    set_error (0, 0, 0);
	  /* if the channel is in use, then close it */
	  /* BUGS: Doesnt handle the command channel, or current
	     partition directories */
	  debug_msg ("Opening logical file $%02x on IEC drive %d\n",
		     attncmd & 0x0f, attndev & 0x1f);
	  if (attncmd & 0xf == 0xf)
	  {
	    /* command channel!
	       We dont have to explicitly open the command channel,
	       only yell at it :) */
	    strcpy (dos_command[last_unit], filename);
	    dos_comm_len[last_unit] = fnlen;
	    do_dos_command ();
	    break;
	  }
	  if (logical_files[i][attncmd & 0xf].open == 1)
	    fs64_closefile_g (&logical_files[i][attncmd & 0xf]);
	  fs64_openfile_g (curr_dir[last_unit][curr_par[last_unit]],
			   filename, &logical_files[i][attncmd & 0xf]);
	  lf_flags[attncmd & 0xf] = 2;
	  break;
	}
      break;
    case CLOSE:
      /* close a file */
      for (i = 0; i < MAX_NET_DEVS; i++)
	if (devices[i] == (attndev & 0x1f))
	{
	  debug_msg ("Closing logical file $%02x on IEC $%02x\n",
		     attncmd & 0x0f, attndev & 0x1f);
	  fs64_closefile_g (&logical_files[i][attncmd & 0xf]);
	  if (attncmd & 0xf == 0xf)
	  {
	    /* Closing command channel forces all files shut on the
	       drive */
	    for (j = 0; j < 15; j++)
	      fs64_closefile_g (&logical_files[i][j]);
	  }
	  break;
	}
      break;
    case COMMUNE:
      /* talk or listen */
      if (attndev & 0x20)
      {
	debug_msg ("Listening to logical file $%02x\n", attncmd & 0x0f);
	listenlf = attncmd & 0x0f;
	/* ignore all the chars if it is not for us, but we must
	   still watch them. */
	i = which_unit (attndev & 0x1f);
	if (i > -1)
	{
	  last_drive = attndev & 0x1f;
	  last_unit = which_unit (last_drive);
	}

	/* clear dos command */
	if (listenlf == 0x0f)
	  for (j = 0; j < 255; j++)
	    dos_command[i][j] = 0;

	while (!attn)
	{
	  a = iecin ();
	  if (a > -1)
	  {
	    if (i > -1)
	      if (listenlf < 0x0f)
		fs64_writechar (&logical_files[i][listenlf], a);
	      else
	      {
		/* dos command channel */
		if (dos_comm_len[i] < 255)
		  dos_command[i][dos_comm_len[i]++] = a;
		else
		  /* command too long - shouldn't happen often! */
		  set_error (32, 0, 0);
	      }
	    /* else not_64net_drive */
	  }			/* if (a>-1) */
	}			/* while(!attn) */
	/* attn has ben asserted */
	if (listenlf == 0x0f)
	{
	  /* dispatch dos command */
	  do_dos_command ();
	}
      }
      else if (attndev & 0x40)
      {
	talklf = attncmd & 0x0f;
	talkdev = attndev & 0x1f;
	if (which_unit (talkdev) != -1)
	  debug_msg ("Talking from logical file $%02x, dev $%02x\n",
		     talklf, talkdev);
      }
    case NONE:
      break;
    }
    attndev = 0;
    attncmd = 0;
    return (0);
  }
  else
  {
    /* We just had an Attn pulse */
    debug_msg ("Attn Pulsed\n");
    return (0);
  }
}


int
received (int c)
{
  if (attn)
  {
    /* char received under attention */
    if (attndev == 0)
    {
      attndev = c;
      if (attndev == 0x3f)
      {
	debug_msg ("buss-wide unlisten.\n");
	/* listenlf=-1; */
      }
      else if (attndev == 0x5f)
      {
	/* global shut-up from the C64 */
	debug_msg ("buss-wide untalk.\n");
	talklf = -1;
      }
      else
      {
	last_drive = attndev & 0x1f;
	last_unit = which_unit (last_drive);
      }
    }
    else
    {
      /* its the attention command */
      attncmd = c;
      /* dont act until attn is deasserted */
    }
  }
  else
  {
    /* char received normally */
    debug_msg ("Char $%02x received outside of packet.\n", c);
  }
  return (0);
}

int
iecin ()
{
  int a, b, i;

  a = fgetc (f);
  if (a == COMMCHAR)
  {
    b = a;
    a = fgetc (f);
    if (a == 0)
      return (b);
    else if (a == 0x41)
    {
      attn = 1;
      return (-1);
    }
    else if (a == 0x49)
    {
      attn = 0;
      return (-1);
    }
    else if (a == 0x01)
      return (COMMCHAR2);
    else if (a > 0x7f)
    {
      for (i = 0; i < MAX_NET_DEVS; i++)
	if ((a & 0x7f) == devices[i])
	{
	  debug_msg ("Device $%02x Acknowledged\n", a & 0x7f);
	  fputc (0x01, f);
	  return (-1);
	}
      fputc (0x03, f);
      return (-1);		/* no char received */
    }
    else
      return (-1);		/* no char received */
  }
  else
    return (a);
}

/* These configuration reading routines really should be in another module,
   but that would entail global variables which i want to avoid.
 */

int
read_config (char *file)
{
  /* Read in the 64net.ini file */

  FILE *cf = 0;
  unsigned char temp[256];

  if ((cf = fopen (file, "r")) == NULL)
    fatal_error ("Cannot read configuration file.");

  while (!feof (cf))
  {
    fgets (temp, 256, cf);
    if ((temp[0] == '#') || (temp[0] == 0))
    {
      /* its a comment or blank line */
    }
    else
    {
      /* its a real line of stuff */
      if (!strncmp ("port ", temp, 4))
      {
	/* its a port line */
	strcpy (port, &temp[5]);
	printf ("INIT: Communication port set to %s\n", port);
      }
      else if (!strncmp ("device ", temp, 6))
      {
	/* its a device line */
	read_device (cf);
      }
      else if (!strncmp ("debug mode", temp, 9))
      {
	/* debug mode */
	debug_mode = 1;
	printf ("INIT: Debug mode messages will be displayed.\n");
      }
      else
      {
	/* Unknown line type */
	fatal_error ("Bad configuration file.  Unknown line types.");
      }
    }
  }
  fclose (cf);
  cf = 0;

  /* all done! */

  /* Check for required bits */
  if (!devices[0])
  {
    /* no devices defined */
    fatal_error ("Configuration file contains no device lines.");
  }
  if (port[0] == 0)
  {
    /* no port lines */
    fatal_error ("Configuration file contains no port line.");
  }

  return (0);
}

int
read_device (FILE * cf)
{
  /* read a device section */
  int dev_num, i, pn;
  char temp[256];

  for (dev_num = 0; dev_num < MAX_NET_DEVS; dev_num++)
    if (!devices[dev_num])
      break;

  if (dev_num >= MAX_NET_DEVS)
  {
    /* too many network devices */
    fatal_error ("Too many network devices defined.");
  }
  else
  {
    /* yup, we can cope */

    while (!feof (cf))
    {
      fgets (temp, 256, cf);
      if ((temp[0] == '#') || (temp[0] == 0))
      {
	/* comment line */
      }
      else
      {
	/* real line */
	/* Acceptables are:
	   NUMBER <dev_num>            - Sets the device # for this dev
	   PARTITION <dev_num>,<path>  - Define a partition
	   END DEVICE                  - End definition of device
	   PRINTER                     - Define device to be a printer,
	   not a drive.
	 */
	if (!strncmp ("number", temp, 6))
	{
	  /* device number */
	  devices[dev_num] = (unsigned char) atol (&temp[6]);
	  printf ("Networked device $%02x assigned to IEC device $%02x\n",
		  dev_num, devices[dev_num]);
	}
	else if (!strncmp ("partition", temp, 9))
	{
	  /* partition for drive */
	  /* find first comma */
	  for (i = 9; i < strlen (temp); i++)
	    if (temp[i] == ',')
	      break;
	  if (i >= strlen (temp))
	    fatal_error ("Bad partition line (no commas).");
	  pn = atol (&temp[9]);
	  if ((pn < 1) || (pn > 250))
	  {
	    /* bad partition # */
	    fatal_error ("Bad partition number. Must be between 1 and 250.");
	  }
	  else
	  {
	    /* okey */
	    if (!(partn_dirs[dev_num][pn] =
		  (unsigned char *) malloc (strlen (&temp[i + 1]))))
	      /* couldnt malloc */
	      fatal_error ("Cannot allocate memory.");
	    else
	    {
	      /* strip newline */
	      temp[strlen (temp) - 1] = 0;
	      strcpy (partn_dirs[dev_num][pn], &temp[i + 1]);
	      printf ("  %s added as partition %d on unit $%02x\n",
		      partn_dirs[dev_num][pn], pn, dev_num);
	    }
	  }
	}
	else if (!(strncmp ("end ", temp, 3)))
	{
	  /* Done definition */
	  return (0);
	}
      }				/* end of not a comment */
    }				/* end of while !feof */
    fatal_error ("Unexpected end of configuration file.");
  }				/* end of we can handle another drive */

  return (0);
}

int
fatal_error (unsigned char *message)
{
  /* fatal error */

  printf ("64NET: A fatal error has occured:\n");
  printf ("       %s\n", message);
  exit (2);
}

int
which_unit (int dev)
{
  /* return the network unit number for dev */
  int i;

  for (i = 0; i < MAX_NET_DEVS; i++)
  {
    if ((dev & 0x1f) == devices[i])
      return (i);
  }

  /* no luck */
  return (-1);
}

int
set_drive_status (unsigned char *string, int len)
{
  /* set the drive message */
  int i, d;

  d = which_unit (last_drive);
  if (d < 0)
    return (1);			/* no drive polled */

  /* copy string */
  for (i = 0; i <= len; i++)
    dos_status[d][i] = string[i];

  /* and set length */
  dos_stat_len[d] = len;
  return (0);
}

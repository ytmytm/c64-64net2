/* 
   64NET/2 Commune module
   (C)Copyright Paul Gardner-Stephen 1995, 1996, All Rights Reserved.

   This module does all the C64 <--> Server communications
   Printer port version
 */

/*
   BUGS:
   This programme will have to be greatly generalised as time progresses.
   At present it will hopefully support the full drive redirection scope
   intended.  Communication device generalisation will be next on the 
   list.
 */

#include "config.h"

#include "fs.h"
#include "machdep.h"
#include "comm-lpt.h"
#include "client-comm.h"
#include "fs_accel.h"
#include "misc_func.h"
#include "comm-work.h"

#define PADDING 1

/* ports */
int portin  = DEF_INPORT;
int portout = DEF_OUTPORT;

/* Current talk & listen logical files */
int talklf=-1;
int listenlf=-1;
int lastlf=-1;

/* ** DRIVE and PARTITION resolution: */
/* Partition base directories for each drive */
uchar *partn_dirs[MAX_NET_DEVS][256];

/* ** CBM DOS emulation stuff: */
/* DOS Statii */
uchar dos_status[MAX_NET_DEVS][256];
/* length of DOS statii */
uchar dos_stat_len[MAX_NET_DEVS];
/* DOS commands */
uchar dos_command[MAX_NET_DEVS][256];
/* length of DOS command */
uchar dos_comm_len[MAX_NET_DEVS]={0};

/* ** current directories & partitions: */
/* current partition on each drive */
int curr_par[MAX_NET_DEVS];
/* current subdirectory (absolute) for each partition on each
   drive */
uchar *curr_dir[MAX_NET_DEVS][256];

/* dir blocks for partitions */
int partn_dirtracks[MAX_NET_DEVS][256];
int partn_dirsectors[MAX_NET_DEVS][256];
int curr_dirtracks[MAX_NET_DEVS][256];
int curr_dirsectors[MAX_NET_DEVS][256];

/* Logical files */
fs64_file logical_files[MAX_NET_DEVS][16];

/* Device number as given in config file and passed to client upon boot */
int devnum=1;

/* client system type */
int client_type=0;

/* Last accessed drive, for the purposes of error channels etc */
int last_drive=-1; /* none */
int last_unit=-1; /* none */

/* number of times we should try to poll before snoozing */
int max_tries=synctolerance;

/* structures for redirected devices */
int devices[MAX_NET_DEVS]={0};

/* file that points to our communications device */
fs64_file file;

/* partition # that will be searched for programs, whatever this means, not used */
int pathdir;

int commune (void)
{
  /* The actual programme loop which does the work of talking to the C64/C128
     or C65.
     This should be sufficiently general to work over serial, parallel or 
     otherwise (including real IEC buss).
   */

  int a;

  init_hw ();

  /* Announce registration details */
  printf ("64NET/2 is covered by the 64net/2 BSD license\n");

  starthw();

  while (1)
  {

    /* the following line may effect stuff, so dont delete */
/*      debug_msg("Waiting. . .\n");     */
#ifdef USE_LINUX_KERNEL_MODULE
    a = charget();
#else /* !USE_LINUX_KERNEL_MODULE */
    a = syncchar();
#endif
    switch (a)
    {
    case FSACCEL:
	/* File System Accelerator */
	fs_accel (charget());
	goto next;
    case OPENDISABLE:
	dont_open=1;
	goto next;
    case SAVE:
      do_save();
      goto next;
    case LOAD:
      do_load();
      goto next;
    case DEVCHECK:
      {
	/* check device, is it one we being maintained by 64net.
	   This is used by the client to determine which buss to
	   talk to */
	debug_msg ("Device check\n");
	if (which_unit (charget ()) > -1)
	{
	  /* 64net device */
	  sendchar (1);
	}
	else
	{
	  /* non-64net device */
	  sendchar (0);
	}
      }
      goto next;
    case BOOT:
      {
	do_boot();
	/* return to (almost) startup state */
	listenlf = -1;
	talklf = -1;
      }
      goto next;
    case OPEN:
       do_open_small();
       goto next;
    case CLOSE:
       do_close_small();
       goto next;
    case CHKIN:
	do_chkinout(0);
	goto next;
    case CHKOUT:
	do_chkinout(1);
	goto next;
    case SECOND:
	{
	    /* second system call, get secondary address after LISTEN, that is
	       filenumber (channel) and mode (6x=data flow, ex=name, fx=close) */
	    listenlf = charget();
	    debug_msg ("Second call, sa=$%02x, channel=%i\n",listenlf & 0xf0,listenlf & 0x0f);
	    switch (listenlf & 0xf0) {
		case 0xf0:	/* name incoming */
		    fnlen = 0;
		    filename[fnlen]=0;
		    break;
		case 0xe0:	/* close a file */
		case 0x60:	/* get some data - open a file */
		    break;
	    }
	}
	goto next;
    case TKSA:
	{
	    /* tksa system call, get secondary address after TALK, that is
	       filenumber and mode, mode can be only 0x6x or 0xfx (data or close) */
	    talklf = charget();
	    debug_msg ("Tksa call, sa=$%02x, channel=%i\n",talklf & 0xf0,talklf & 0x0f);
	    switch (talklf & 0xf0) {
		case 0xf0:	/* name??? - shouldn't happen */
		case 0xe0:	/* close */
		case 0x60:	/* send some data - open a file */
	    	break;
		}
		
	}
	goto next;
    case LISTEN:
    case TALK:
	{
	    int i;
	    /* listen/talk system call, get device number and merely ignore it (AFAIK 64net/2 has
	       only one device but more units) */
	    i = charget();
	    if (a==LISTEN) debug_msg ("Listen call on device %i\n",i);
	    if (a==TALK) debug_msg ("Talk call on device %i\n",i);
	}
	goto next;
    case CIOUT:
	{
    	    /* ciout system call, receive something and store it */
	    if (listenlf < 0)
	    {
		/* listening without a listener */
		/* this one's for the bit bucket */
		debug_msg ("Listen without listener: %i\n",charget());
		/* charget (); */
	    } else {
		switch (listenlf & 0xf0) {
		    case 0xf0:		/* fill in name buffer */
			if ((listenlf & 0x0f) == 0x0f) {
			    do_ciout(listenlf); }
			else {
			    filename[fnlen++]=charget();
			    filename[fnlen]=0; }
			break;
		    case 0xe0:
			charget();	/* illegal - drop it */
			debug_msg ("Unneeded CIOUT call (on close)\n");
			break;
		    case 0x60:		/* do file stuff */
			do_ciout(listenlf);
			break;
		}
	    }
	}
	goto next;
    case ACPTR:
	{
	    /* acptr system call, get something and send it */
	    if (talklf < 0)
	    {
		debug_msg ("talking without a talker\n");
		/* talking without a talker */
		/* send reply */
		sendchar (128);
		sendchar (199);
		/* and set error */
		set_error (70, 0, 0);
	    } else {

		switch (talklf & 0xf0) {
		    case 0xf0:
		    case 0xe0:
			sendchar (128);	/* send kinda error */
			sendchar (199);
			break;
		    case 0x60:		/* send file data */
			do_acptr(talklf);
			break;
		}
	    }
	}
	goto next;
    case UNLISTEN:
    case UNTALK:
	{
	    if (a==UNLISTEN) debug_msg ("Unlisten call on channel %i\n",listenlf & 0x0f);
	    if (a==UNTALK) debug_msg ("Untalk call on channel %i\n",talklf & 0x0f);
	    /* untalk/unlisten system call, lower all talk flags, close files, (send error?) */
	    /* ATN+0x5f */
	    lastlf = (a==UNLISTEN) ? listenlf : talklf;
//	    if (a==UNLISTEN) devlf=listenlf; else devlf=talklf; 
	/* do_dos_command if after listen and channel 15 */
	    if (a==UNLISTEN) listenlf = -1; else talklf = -1;

//^	    if ((a==UNLISTEN) && ((lastlf & 0x0f)==15) && (dos_comm_len[last_unit]!=0)) {
	    if ((a==UNLISTEN) && ((lastlf & 0x0f)==15)) {
    		debug_msg ("Processing dos command\n");
		if (dos_comm_len[last_unit]!=0) do_command();
		sendchar (0);	/* UN{TALK,LISTEN} always return status code OK, read dos_status for more */
	    } else {
		switch(lastlf & 0xf0) {
		    case 0xf0:	/* got full name - open it */
			if ((lastlf & 0x0f)!=0x0f) do_open(lastlf);
			break;
		    case 0xe0:	/* close that file */
			if ((lastlf & 0x0f)!=0x0f) do_close(lastlf);
			break;
		    case 0x60:	/* err, what was that? flush buffers? anyway return no errors */
			sendchar (128);	/* XXX EOF here? or 0 as OK? */
		}		/* end switch(devlf...	*/
	    }			/* end else...		*/
	}			/* end UNTALK:		*/
    }				/* end switch(a)	*/
  next:
  	;
  }				/* while(1)		*/
/* will be never reached */
	return 0;
}				/* int commune()	*/

/* These configuration reading routines really should be in another module,
   but that would entail global variables which i want to avoid.
 */

int 
read_config (uchar *file)
{
  /* Read in the 64netrc file */

  FILE *cf = 0;
  uchar temp[256];
  uchar port[256]={0};

  if ((cf = fopen (file, "r")) == NULL)
    fatal_error ("Cannot read configuration file.");

  while (!feof (cf))
  {
    fgets ((char*)temp, 256, cf);
    if ((temp[0] == '#') || (temp[0] == 0) || (temp[0] == '\n') || (temp[0] == '\r'))
    {
      /* its a comment or blank line */
    }
    else
    {
      /* its a real line of stuff */
      if (!strncmp ("ram ", temp, 3))
      {
	/* create `geoRAM' */
	if (georam_size != 0)
	{
	  printf ("INIT: GEOS-RAM already specified - ignoring redeclaration\n");
	}
	else
	{
	  georam_size = atol (&temp[4]);
	  if (georam_size > 0)
	  {
	    if (!(georam =
		  (uchar *) malloc (georam_size * 256 + PADDING)))
	      /* couldnt malloc */
	      fatal_error ("Cannot allocate memory for GEOS-RAM (try smaller size!).");
	    printf ("INIT: GEOS-RAM created (%d pages = %dKB)\n", georam_size, georam_size / 4);
	  }
	  else
	  {
	    fatal_error ("INIT: Bad value for GEOS-RAM size - Barf!\n");
	  }
	}
      }
      else if (!strncmp ("port ", temp, 4))
      {
	/* its a port line */
#ifdef AMIGA
	printf ("INIT: Using internal parallel port\n");
	strcpy (port, &temp[5]);
#else
	strcpy (port, &temp[5]);
	printf ("INIT: Communication port set to %s\n", port);
	printf ("      (Port interpretted as hex addr for lpt)\n");
	portout = strtol ((char*)port, NULL, 16);
	portin = portout + 1;
	printf ("INIT: Port now $%04x\n", portout);
#endif
      }
      else if (!strncmp ("path ", temp, 4))
      {
	/* path partition */
	pathdir = atol (&temp[5]);
	printf ("64NET/2 will look in partition %d for programmes\n", pathdir);
      }
      else if (!strncmp ("devnum ", temp, 6))
      {
	/* 64net2 emulated device number */
	devnum = atol (&temp[7]);
	printf ("64NET/2 will be seen from client as device %d\n", devnum);
      }
      else if (!strncmp ("device ", temp, 6))
      {
	/* its a device line */
	read_device (cf);
      }
      else if (!strncmp ("ledfile", temp, 6))
      {
	/* its a device line */
	if (temp[strlen (temp) - 1] < ' ')
	  temp[strlen (temp) - 1] = 0;
	if (temp[7] < ' ')
	  sprintf ((char*)&temp[7], " %s/.64net2.leds", getenv ("HOME"));
	client_init (&temp[8]);
	client_activity (0);
	client_error (0);
      }
      else if (!strncmp ("timeout ", temp, 7))
      {
	/* set max_tries */
	max_tries = atoi (&temp[8]);
	printf ("INIT: Timeout set to %d attempts\n", max_tries);
      }
      else if (!strncmp ("debug mode", temp, 9))
      {
	/* debug mode */
	debug_mode = 1;
	printf ("INIT: Debug mode messages will be displayed.\n");
      }
      else if (!strncmp ("fishload", temp, 7))
      {
        /* enable fishload */
        allowFishLoad = 1;
	printf("INIT: enabled fishload routine.\n");
      }
      else if (!strncmp ("fishsave", temp, 7))
      {
        /* enable fishsave */
	allowFishSave = 1;
	printf("INIT: enabled fishsave routine.\n");
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
	  devices[dev_num] = (uchar) atol (&temp[6]);
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
	  if ((pn < 1) || (pn > 255))
	  {
	    /* bad partition # */
	    fatal_error ("Bad partition number. Must be between 1 and 255.");
	  }
	  else
	  {
	    /* okey */
	    partn_dirs[dev_num][pn] =
	      (uchar *) malloc (strlen (&temp[i + 1]) + PADDING);
	    if (!partn_dirs[dev_num][pn])
	      /* couldnt malloc */
	      fatal_error ("Cannot allocate memory.");
	    else
	    {
	      /* strip newline */
	      uchar partition[8], path[1024];
	      partition[0] = 'n';
	      temp[strlen (temp) - 1] = 0;
	      strcpy (partn_dirs[dev_num][pn], &temp[i + 1]);
	      printf ("  %s added as partition %d on unit $%02x\n",
		      partn_dirs[dev_num][pn], pn, dev_num);
	      /* parse for .DHD sub-directories */
	      partn_dirtracks[dev_num][pn] = -1;
	      partn_dirsectors[dev_num][pn] = -1;
	      curr_dirtracks[dev_num][pn] = -1;
	      curr_dirsectors[dev_num][pn] = -1;
	      strcpy (path, partn_dirs[dev_num][pn]);
	      if (path[0] != '@')
	      {
		if (fs64_resolve_partition (partition, path,
					    &partn_dirtracks[dev_num][pn], &partn_dirsectors[dev_num][pn]))
		{
		  /* failed */
		  printf ("Invalid partition path for %d\n", pn);
		  fatal_error ("Invalid partition table\n");
		}
		curr_dirtracks[dev_num][pn] = partn_dirtracks[dev_num][pn];
		curr_dirsectors[dev_num][pn] = partn_dirsectors[dev_num][pn];
		debug_msg ("  (%s T%d S%d)\n", path,
		partn_dirtracks[dev_num][pn], partn_dirsectors[dev_num][pn]);
		free (partn_dirs[dev_num][pn]);
		if (!(partn_dirs[dev_num][pn] =
		    (uchar *) malloc (strlen (path) + 1 + PADDING)))
		  /* couldnt malloc */
		  fatal_error ("Cannot allocate memory.");
		strcpy (partn_dirs[dev_num][pn], path);
	      }
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
set_drive_status (uchar *string, int len)
{
  /* set the drive message */
  int d;

  /* Only one drive at present */
  /*   d=which_unit(last_drive); */
  d = 0;

  if (d < 0)
    return (1);			/* no drive polled */

  /* copy string */
  memcpy (dos_status[d], string, len+1);

  /* and set length */
  dos_stat_len[d] = len;
  return (0);
}


/* do a peek from the C64's memory */
int 
c64peek (long location)
{

  sendchar (0xff);
  sendchar (0x02);
  sendchar (0x02);
  sendchar (location & 0xff);
  sendchar (location >> 8);

  return (charget ());

}

int 
c64poke (long location, int value)
{

  sendchar (0xff);
  sendchar (0x01);
  sendchar (0x02);
  sendchar (location & 0xff);
  sendchar (location >> 8);
  sendchar (value);

  return (0);

}

int 
c64print (uchar *text)
{
#ifdef USE_LINUX_KERNEL_MODULE
    sendchar (0xfd);
    fishsendblock(strlen(text)+1,text);
#else
  int i;

  asciitopetscii(text);

  sendchar (0xfd);
  for (i = 0; i < strlen (text); i++)
    sendchar (text[i]);
  sendchar (0);
#endif
  return (0);
}

int 
c64jsr (int addr, int a, int x, int y)
{
  sendchar (0xf8);
  sendchar (addr & 0xff);
  sendchar (addr >> 8);
  sendchar (y & 0xff);
  sendchar (x & 0xff);
  sendchar (a & 0xff);

  return (0);
}

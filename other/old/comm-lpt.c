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

#include <unistd.h>

#include "fs.h"
#include "machdep.h"
#include "comm-lpt.h"
#include "clientdep.h"
#include "client-comm.h"
#include "fs_accel.h"
#include "version.h"
#include "datestamp.h"
#include "dosemu.h"
#include "misc_func.h"

/* 
   XXX - Linux doesnt seem to like fish save (tho fishload is fine)
*/
int allowFishLoad=0;	/* 1 = on */
int allowFishSave=0;

/* debug messages flag */
int debug_mode=0;

/* ports */
int inport  = 0x3bd;
int outport = 0x3bc;

/* Current talk & listen logical files */
int talklf=-1;
int listenlf=-1;
int secaddr=0;

/* ** DRIVE and PARTITION resolution: */
/* Partition base directories for each drive */
unsigned char *partn_dirs[MAX_NET_DEVS][256];

/* ** CBM DOS emulation stuff: */
/* DOS Statii */
unsigned char dos_status[MAX_NET_DEVS][256];
/* length of DOS statii */
unsigned char dos_stat_len[MAX_NET_DEVS];
/* DOS commands */
unsigned char dos_command[MAX_NET_DEVS][256];
/* length of DOS command */
unsigned char dos_comm_len[MAX_NET_DEVS]={0};

/* ** current directories & partitions: */
/* current partition on each drive */
int curr_par[MAX_NET_DEVS];
/* current subdirectory (absolute) for each partition on each
   drive */
unsigned char *curr_dir[MAX_NET_DEVS][256];

/* dir blocks for partitions */
int partn_dirtracks[MAX_NET_DEVS][256];
int partn_dirsectors[MAX_NET_DEVS][256];
int curr_dirtracks[MAX_NET_DEVS][256];
int curr_dirsectors[MAX_NET_DEVS][256];

/* Logical files */
fs64_file logical_files[MAX_NET_DEVS][16];

/* device to communicate over */
unsigned char port[256]={0};

/* structures for redirected devices */
int devices[MAX_NET_DEVS]={0};

/* file readable flags (set when first character is read) */
int lf_flags[MAX_NET_DEVS][16];

/* Last accessed drive, for the purposes of error channels etc */
int last_drive=-1; /* none */
int last_unit=-1; /* none */

/* number of times we should try to poll before snoozing */
int max_tries=10;
int tries;

/* simulated RAM expander */
int georam_size=0;
uchar *georam;

/* Temprary variables used to store filenames and things for
   fs_openfile_g etc 
   */
int fnlen;
unsigned char filename[256];

#ifdef UNIX
/* file handler for chip-bash permissions */
FILE *f;
#endif /* UNIX */

/* file that points to our communications device */
fs64_file file;

int pathdir;

/* snooz_time */
int snooz_time=70000; /* 70ms - ie "twitch delay" to appear instantaneous */


int commune (void)
{
  /* The actual programme loop which does the work of talking to the C64/C128
     or C65.
     This should be sufficiently general to work over serial, parallel or 
     otherwise (including real IEC buss).
   */

  int a, i=0, j, no, fn, sa;
  unsigned char c;

  init_hw ();

  /* Announce registration details */
  printf ("64NET/2 is covered by GPL\n");

  starthw();

  while (1)
  {

    /* the following line may effect stuff, so dont delete */
/*      debug_msg("Waiting. . .\n");     */
#ifndef USE_LINUX_KERNEL_MODULE
    tries = 0;
    a = syncchar ();
    while (a < 0)
    {
      a = syncchar ();
      if (a == -1)		/* -2 = disable, -1 = enable snoozing */
      {
	tries++;
	if (tries >= max_tries)
	{
	  if (tries == max_tries)
	  {
	    debug_msg ("Z");
	    fflush (stdout);
	  }
	  else
	  {
	    debug_msg ("z");
	    fflush (stdout);
	  }
#ifdef AMIGA
	  Delay (3);
#else
	  usleep (snooz_time);	/* dont hog cpu  */
#endif
	}
      }
    }
    if (tries >= max_tries)
    {
      debug_msg ("\n");
    }
#else /* USE_LINUX_KERNEL_MODULE */
    a = charget();
#endif
    switch (a)
    {
    case FSACCEL:
      {
	/* File System Accelerator */
	fs_accel ();
      }
      break;
    case SAVE:
      {
	int startaddr = 0xffff, endaddr = 0x0000, bc = 0;
	int s, ms;
	char buff[256];
	fs64_file savefile;
	char fname[256];

	debug_msg ("Save\n");

	/* read filename */
	fnlen = charget ();
	for (i = 0; i < fnlen; i++)
	  fname[i] = charget ();
	fname[fnlen] = 0;

	/* read secondary address */
	sa = charget ();

	/* print SAVING message */
	c64print ("\rSAVING ");
	c64print (fname);

	/* search for file */
	gettimer (&s, &ms);
	last_unit = 0;
	if (fs64_openfile_g (curr_dir[last_unit][curr_par[last_unit]],
			     fname, &savefile))
	{
	  /* file not found */
	  /* postpend ",W" to the filename and try again */
	  strcat (fname, ",W");
	  if (fs64_openfile_g (curr_dir[last_unit][curr_par[last_unit]],
			       fname, &savefile))
	  {
	    /* cannot open file for some reason */
	    /* simply abort */
	    sendchar (254);
	    sendchar (0);
	    goto next;
	  }
	}
	else
	{
	  /* file exists */
	  set_error (63, 0, 0);
	  sendchar (254);
	  sendchar (0);
	  goto next;
	}

	/* file is open, proceed to get address range */
	startaddr = c64peek (0xc1) + 256 * c64peek (0xc2);
	endaddr = c64peek (0xae) + 256 * c64peek (0xaf);

	/* write address link */
	fs64_writechar (&savefile, (startaddr & 255));
	fs64_writechar (&savefile, (startaddr / 256));

	/* write file body */
	while (startaddr <= endaddr)
	{
	  bc = endaddr - startaddr + 1;
	  if (bc > 254)
	    bc = 254;


	  if ((startaddr < 0xcf00) && (bc == 254) && allowFishSave)
	    /* fish save the block */
	    fastgetblock (startaddr, 254, buff);
	  else
	  {
	    /* get block */
	    sendchar (255);
	    sendchar (2);
	    sendchar (bc + 1);
	    sendchar (startaddr & 0xff);
	    sendchar (startaddr / 256);
	    sendchar (0);
	    for (i = 0; i < bc; i++)
	      buff[i] = charget ();
	  }
	  /* write block */
	  for (i = 0; i < bc; i++)
	    if (fs64_writechar (&savefile, buff[i]))
	    {
	      printf ("Closing save file due to error\n");
	      fs64_closefile_g (&savefile);
	      sendchar (254);
	      sendchar (0);
	      goto next;
	    }
	  startaddr += bc;
	}
	printf ("Closing save file\n");
	fs64_closefile_g (&savefile);

	{
	  int s2, ms2;
	  gettimer (&s2, &ms2);
	  if (ms > ms2)
	  {
	    ms2 += 1000;
	    s2--;
	  }
	  s = s2 - s;
	  ms = ms2 - ms;
	  printf ("Save time: %d.%d\n", s, ms);
	}
	sendchar (254);
	sendchar (0);
      }
      goto next;
    case LOAD:
      {
	/* Load a file using highly optimised routines */
	int startaddr, bc, n, l = 0;
	int filestart=0, kernstart;
	int s, ms;
	char buff[256];
	fs64_file loadfile;

	char fname[256];
	debug_msg ("Load\n");

	fnlen = charget ();
	for (i = 0; i < fnlen; i++)
	  fname[i] = charget ();
	fname[fnlen] = 0;
	/* sec addr */
	sa = charget ();

	/* print SEARCHING message */
	c64print ("\rSEARCHING FOR ");
	c64print (fname);
	c64print ("\r");
	/* search for file */
	last_unit = 0;
	if (fs64_openfile_g (curr_dir[last_unit][curr_par[last_unit]],
			     fname, &loadfile))
	{
	  if ((dos_status[last_unit][0] == '6') && (dos_status[last_unit][1] == '2'))
	  {
	    /* file not found, so check "path" */
	    uchar foo[256] =
	    {0};
	    int i;
	    for (i = 0; i < strlen (fname); i++)
	    {
	      if (fname[i] == ':')
	      {
		sprintf (foo, "%d%s", pathdir, &fname[i]);
	      }
	    }
	    if (!foo[0])
	      sprintf (foo, "%d:%s", pathdir, fname);
	    if (fs64_openfile_g (partn_dirs[last_unit][pathdir], foo, &loadfile))
	    {
	      /* file not found */
	      sendchar (254);
	      sendchar (4);
	      goto next;
	    }
	  }
	  else
	  {
	    /* file not found */
	    sendchar (254);
	    sendchar (4);
	    goto next;
	  }
	}

	/* file found - so load */
	c64print ("LOADING");
	client_turbo_speed ();
#ifdef DEBUG
	printf ("Done client_turbo_speed\n");
#endif
	gettimer (&s, &ms);
	/* get load address - BUG: Will load at $0801 if ,n,0
	   (ie will not load at the requested location */
#ifdef DEBUG
	printf ("About to run fs64_readchar\n");
#endif
	if (fs64_readchar (&loadfile, &c))
	{
	  /* empty file */
	  sendchar (254);
	  sendchar (0);
	}
	else
	  filestart = c;
	if (fs64_readchar (&loadfile, &c))
	{
	  /* empty file */
	  sendchar (254);
	  sendchar (0);
	}
	else
	  filestart += c * 256;

	/* get start address from kernel */
	kernstart = c64peek (0xc3) + 256 * c64peek (0xc4);

	/* which one to use? */
	if (!sa)
	  startaddr = kernstart;
	else
	  startaddr = filestart;

/*          debug_msg("KS $%04x, FS: $%04x, SA: $%04x\n",
   kernstart,filestart,startaddr); */

	bc = 0;
#ifdef DEBUG
	printf ("About to send file\n");
#endif

	while (!fs64_readchar (&loadfile, &c))
	{
	  l++;
	  buff[bc++] = c;
	  if (bc == 254)
	  {
#ifdef DEBUG
	    printf ("Sending block(in while loop)\n");
#endif
	    /* send block (using fastest allowed protocol) */
	    if (allowFishLoad&&(startaddr < 0xcf00))
	      {
#ifdef DEBUG
		printf ("Sending block addr<0xcf00 (in while loop)\n");
#endif
		fastsendblock (startaddr, 254, buff);
	      }
	    else
	      {
#ifdef DEBUG
		printf ("using raw sendchar(in while loop)");
#endif
		sendchar (255);
		sendchar (1);
		sendchar (bc + 1);
		sendchar (startaddr & 0xff);
		sendchar (startaddr / 256);
		for (n = 0; n < bc; n++)
		  sendchar (buff[n]);
	      }
	    startaddr += bc;
	    bc = 0;
	  }
	}
	
	/* final partial block */
	if (bc)
	{
	  /* send block */
	  sendchar (255);
	  sendchar (1);
	  sendchar (bc + 1);
	  sendchar (startaddr & 0xff);
	  sendchar (startaddr / 256);
	  for (n = 0; n < bc; n++)
	    sendchar (buff[n]);
	  startaddr += bc;
	  bc = 0;
	}
#ifdef DEBUG
	printf ("Successful load\n");
#endif

	/* successful load */
	fs64_closefile_g (&loadfile);
	c64poke (0x90, 0x40);	/* eof */
	set_error (0, 0, 0);
	/* disconnect c64 */
#ifdef DEBUG
	printf ("Disconnecting 64\n");
#endif

/* #ifndef DEBUG  */
	client_normal_speed ();
/* #endif */
	{
	  int s2, ms2;
	  gettimer (&s2, &ms2);
	  if (ms > ms2)
	  {
	    ms2 += 1000;
	    s2--;
	  }
	  s = s2 - s;
	  ms = ms2 - ms;
	  printf ("Load time: %d.%d\n", s, ms);
	}
	c64poke (0xb2, startaddr & 0xff);
	c64poke (0xb3, startaddr / 256);
	sendchar (0);
	sendchar (0);
	sendchar (254);
	sendchar (0);
      }
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
	/* boot sequence */
	debug_msg ("Boot\n");
	/* skip filename etal character */
	fnlen = charget ();
	printf ("fnlen = %02x\n", fnlen);
	for (i = 0; i < fnlen; i++)
	  charget ();
	/* skip sec addr */
	printf ("Sec addr= %02x\n", charget ());
	{
	  char temp[80];
	  sprintf (temp, "\r 64NET/2 SERVER %s", server_version ());
	  c64print (temp);
	}
	c64print ("\r");
	sendchar (254);
	sendchar (0);
      }
      goto next;
    case SETTALKER:
      {
	/* set the talker */
	/* the next char is the secondary address for the file to
	   listen to, and thats it */
	talklf = charget ();
	if (listenlf == 0xf)
	  /* dispatch DOS command */
	  if (dos_comm_len[last_unit])
	    do_dos_command ();
      }
      goto next;
    case TALK:
      {
	/* we are being asked to send a character */
	if (talklf < 0)
	{
	  /* talking without a talker */
	  /* send reply */
	  sendchar (66);
	  sendchar (199);
	  /* and set error */
	  set_error (70, 0, 0);
	}
	else
	  /* something to read from */
	{
	  /* we have a real talk device */
	  /* always unit 0 on normal 64net cable */
	  last_unit = 0;
	  /* which logical file (ie sec_addr) */
	  if (talklf == 0x0f)
	  {
	    /* command channel */
	    if (dos_stat_len[last_unit] < 1)
	      set_error (0, 0, 0);	/* default to OK message */

	    /* send char */
	    sendchar (0);
	    sendchar (dos_status[last_unit][0]);

	    /* update dos status */
	    for (j = 0; j <= dos_stat_len[last_unit]; j++)
	      dos_status[last_unit][j] = dos_status[last_unit][j + 1];

	    /* reduce length remainin */
	    dos_stat_len[last_unit]--;
	  }			/* end of command channel read */
	  /* non command channel read */
	  else
	  {
	    int r;
	    r = fs64_readchar (&logical_files[i][talklf], &c);
	    if (r == -1)
	    {
	      /* couldnt read a char */
/*                      debug_msg("EOF on lf#%d\n",talklf); */
	      /* since this is normal 64net where FILE NOT FOUND can
	         be explicitly indicated, we know its EOF */
	      sendchar (66);
	      sendchar (199);
	    }			/* end of failed to read from channel */
	    else
	    {
	      /* file read succeeded */
	      sendchar (r);
	      sendchar (c);
	    }			/* end of successful read from file */
	  }
	}			/* end of valid talklf */
      }				/* case TALK: */
      goto next;
    case SETLISTENER:
      {
	/* set the listener */
	/* the next char is the secondary address for the file to
	   listen to, and thats it */
	listenlf = charget ();
      }
      goto next;
    case LISTEN:
      {
	/* we are being asked to receive a character */
	if (listenlf < 0)
	{
	  /* listening without a listener */
	  /* this one's for the bit bucket */
	  charget ();
	}
	else
	  /* something to write to */
	{
	  /* we have a real listen device */
	  /* always unit 0 on normal 64net cable */
	  last_unit = 0;
	  /* which logical file (ie sec_addr) */
	  if (listenlf == 0x0f)
	  {
	    /* command channel */
	    dos_command[last_unit][dos_comm_len[last_unit]] = charget ();
	    ++dos_comm_len[last_unit];
	    if (dos_command[last_unit][dos_comm_len[last_unit] - 1] == 0xd)
	    {
	      /* carriage return */
	      debug_msg ("Processing dos command\n");
	      /* if line doesnt start with M-R or M-W, then submit
	         the command */
	      if ((dos_command[last_unit][0] == 'M') &&
		  (dos_command[last_unit][1] == '-'))
	      {
		switch (dos_command[last_unit][2])
		{
		case 'W':
		  /* memory write - variable length
		     (6+ # bytes to write) */
		  if (dos_comm_len[last_unit] > 6)
		  {
		    if (dos_comm_len[last_unit] ==
			(6 + dos_command[last_unit][5]))
		      do_dos_command ();
		  }
		case 'R':
		  /* memory read - fixed length */
		  if (dos_comm_len[last_unit] == 6)
		    do_dos_command ();
		  break;
		default:
		  /* unknown command, so just do it (and thus
		     spit out an error */
		  do_dos_command ();
		}
	      }
	      else
	      {
		do_dos_command ();
	      }
	    }
	  }			/* end of command channel write */
	  /* non command channel write */
	  else
	  {
	    c = charget ();
	    fs64_writechar (&logical_files[i][listenlf], c);
	  }
	}			/* end of valid listenlf */
      }				/* case LISTEN: */
      goto next;
    case OPEN:
      {
	/* open a file */
	/* receive chars until attn is asserted */
	fnlen = charget ();
	for (i = 0; i < fnlen; i++)
	  filename[i] = charget ();
	filename[fnlen] = 0;
	/* get secondary address */
	secaddr = charget ();
	/* add trailers as needed for "special" channels */
	if (secaddr == 0)
	  strcat (filename, ",R");
	if (secaddr == 1)
	  strcat (filename, ",W");

	/* open file on C64 end */
	no = c64peek (0x98);
	if (no > 9)
	{
	  /* too many files */
	  printf ("Too many files\n");
	  sendchar (254);
	  sendchar (1);
	  goto next;
	}
	else
	{
	  /* check for duplicates */
	  fn = c64peek (0xb8);
	  for (i = 0; i < no; i++)
	    if (c64peek (0x259 + i) == fn)
	    {
	      /* file open */
	      printf ("File open\n");
	      sendchar (254);
	      sendchar (2);
	      goto next;
	    }

	  /* add entry, ensuring to or with 0x60 !! */
	  /* this should really be done on the c64 side */
	  no++;
	  c64poke (0x258 + no, fn);
	  c64poke (0x262 + no, c64peek (0xba));
	  c64poke (0x26c + no, c64peek (0xb9) | 0x60);
	  c64poke (0x98, no);
	}

	/* find the unit number */
	i = 0;			/* unit is always zero on 64net cable */
	{
	  /* now *really* open the file */
	  debug_msg ("Filename: \"%s\"\n", filename);
	  /* set the old OK message */
	  if (secaddr < 0x0f)
	    set_error (0, 0, 0);
	  /* if the channel is in use, then close it */
	  debug_msg ("Opening logical file $%02x\n",
		     c64peek (0xb8));
	  /* if the channel is in use, then close it */
	  last_unit = 0;
	  if (secaddr == 0x0f)
	  {
	    /* command channel!
	       We dont have to explicitly open the command channel,
	       only yell at it :) */
	    strcpy (dos_command[last_unit], filename);
	    dos_comm_len[last_unit] = fnlen;
	    do_dos_command ();
	    sendchar (254);
	    sendchar (0);
	    goto next;
	  }
	  if (logical_files[i][secaddr].open == 1)
	    fs64_closefile_g (&logical_files[last_unit][secaddr]);
	  printf("*** Opening [%s]\n",filename);
	  if (fs64_openfile_g (curr_dir[last_unit][curr_par[last_unit]],
			       filename, &logical_files[i][secaddr]))
	  {
	    /* open failed */
	    sendchar (254);
	    sendchar (4);	/* file not found */
	    goto next;
	  }
	  lf_flags[i][secaddr] = 2;
	  sendchar (254);
	  sendchar (0);
	  goto next;
	}			/* end of valid device */
	/* release c64 end */
	sendchar (254);
	sendchar (0);
      }				/* case OPEN: */
      goto next;
    case CLOSE:
      {
	/* close a file */
	debug_msg ("Close\n");
	i = 0;			/* unit is always 0 */
	secaddr = charget () & 0x0f;
	{
	  debug_msg ("Closing logical file $%02x\n",
		     secaddr);
	  fs64_closefile_g (&logical_files[i][secaddr]);
	  if (secaddr == 0xf)
	  {
	    /* Closing command channel forces all files shut on the
	       drive */
	    for (j = 0; j < 15; j++)
	      fs64_closefile_g (&logical_files[i][j]);
	  }
	  debug_msg ("Done close\n");
	  goto next;
	}
      }				/* case CLOSE: */
      goto next;
    }				/* end switch(a) */
  next:
  }				/* while(1) */

}				/* int commune() */

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
		  (unsigned char *) malloc (georam_size * 256 + PADDING)))
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
	outport = strtol (port, NULL, 16);
	inport = outport + 1;
	printf ("INIT: Port now $%04x\n", outport);
#endif
      }
      else if (!strncmp ("path ", temp, 4))
      {
	/* path partition */
	pathdir = atol (&temp[5]);
	printf ("64NET/2 will look in partition %d for programmes\n", pathdir);
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
	  sprintf (&temp[7], " %s/.64net2.leds", getenv ("HOME"));
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
	  if ((pn < 1) || (pn > 255))
	  {
	    /* bad partition # */
	    fatal_error ("Bad partition number. Must be between 1 and 255.");
	  }
	  else
	  {
	    /* okey */
	    partn_dirs[dev_num][pn] =
	      (unsigned char *) malloc (strlen (&temp[i + 1]) + PADDING);
	    if (!partn_dirs[dev_num][pn])
	      /* couldnt malloc */
	      fatal_error ("Cannot allocate memory.");
	    else
	    {
	      /* strip newline */
	      char partition[8], path[1024];
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
		    (unsigned char *) malloc (strlen (path) + 1 + PADDING)))
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
set_drive_status (unsigned char *string, int len)
{
  /* set the drive message */
  int i, d;

  /* Only one drive at present */
  /*   d=which_unit(last_drive); */
  d = 0;

  if (d < 0)
    return (1);			/* no drive polled */

  /* copy string */
  for (i = 0; i <= len; i++)
    dos_status[d][i] = string[i];

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
  sendchar (0);

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
c64print (char *text)
{
#ifdef USE_LINUX_KERNEL_MODULE
    sendchar (0xfd);
    fishsendblock(strlen(text)+1,text);
#else
  int i;

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
  sendchar (a & 0xff);
  sendchar (x & 0xff);
  sendchar (y & 0xff);
  sendchar (0);

  return (0);
}

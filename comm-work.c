/*
   64NET/2 Kernal level emulation module
   (C) Copyright Maciej Witkowiak 2000

 */

#include "config.h"
#include "fs.h"
#include "machdep.h"
#include "comm-lpt.h"
#include "clientdep.h"
#include "version.h"
#include "datestamp.h"
#include "dosemu.h"
#include "comm-work.h"

#ifdef USE_SERIAL_DRIVER
int allowFishLoad=0;
int allowFishSave=0;
#else
int allowFishLoad=FISHLOAD;	/* 1 = on */
int allowFishSave=FISHSAVE;
#endif

/* Temporary variables used to store filenames and things for
   fs_openfile_g etc 
   */
int fnlen;
uchar filename[256];
int file_unit;

/* next character that will be sent to client */
uchar nextchar;
int nextstatus=64;

/* flag to use with OPEN and LOAD =0-> open file, =1->load file */
int dont_open;

void do_command(void) {

    /* to keep debug-output nice and be sure that everything is OK */
    dos_command[last_unit][dos_comm_len[last_unit]] = '\0';

    /* if line doesnt start with M-R or M-W, then submit
       the command */
    if ((dos_command[last_unit][0] == 'M') &&
	(dos_command[last_unit][1] == '-'))
	{
	    /* always unit 0 on normal 64net cable */
	    last_unit = 0;
	    switch (dos_command[last_unit][2]) {
		case 'W':
		    /* memory write - variable length (6+ # bytes to write) */
		    if (dos_comm_len[last_unit] > 6) {
			if (dos_comm_len[last_unit] == 
			  (6 + dos_command[last_unit][5])) do_dos_command (); }
		case 'R':
		    /* memory read - fixed length */
		    if (dos_comm_len[last_unit] == 6)
			do_dos_command ();
		default:
		    /* unknown command, so just do it (and thus spit out an error */
		    do_dos_command ();
	    }
	} else {
	    do_dos_command ();
	}
}

void do_ciout(int secaddr)
{
    int sa;
    uchar c;

	/* we are being asked to receive a character */
	  /* we have a real listen device */
	  /* always unit 0 on normal 64net cable */
	  last_unit = 0;
	  sa = secaddr & 0x0f;
	  /* which logical file (ie sec_addr) */
	  if (sa == 0x0f)
	  {
	    /* command channel */
	    dos_command[last_unit][dos_comm_len[last_unit]] = charget ();
	    ++dos_comm_len[last_unit];

	  }			/* end of command channel write */
	  /* non command channel write */
	  else
	  {
	    c = charget ();
	    fs64_writechar (&logical_files[file_unit][sa], c);
	  }
}

void do_acptr(int secaddr)
{
    int sa, status = 0;
    uchar c;
    
	/* we are being asked to send a character */
	  /* we have a real talk device */
	  /* always unit 0 on normal 64net cable */
	last_unit = 0;
	sa = secaddr & 0x0f;
	/* which logical file (ie sec_addr) */
	if (sa == 0x0f)
	{
	  /* command channel */
	    if (dos_stat_len[last_unit] < 1) {
		set_error (0, 0, 0);	/* default to OK message */
	    }
	  /* send status */
	    if (dos_stat_len[last_unit]==1)	/* assert EOF with last byte */
		status=64;
	    sendchar (status); sendchar (dos_status[last_unit][0]);
	    /* update dos status */
	    memmove (&dos_status[last_unit][0], &dos_status[last_unit][1], dos_stat_len[last_unit]);
	    /* reduce length remainin */
	    dos_stat_len[last_unit]--;
	}			/* end of command channel read */
	/* non command channel read */
	else
	{
	  int r;
	  /* read first byte if needed */
	  if (nextstatus==-2)
    	     nextstatus = fs64_readchar (&logical_files[file_unit][sa], &nextchar);
	  /* read next byte */
	  r = fs64_readchar (&logical_files[file_unit][sa], &c);
	  if (nextstatus==-1) {
	     /* reading past end */
	     sendchar(64); sendchar(199);
	  } else {
	     if (r==-1)				/* assert EOF with last byte */
	        status=64;
	     sendchar(status); sendchar(nextchar);
	     nextchar = c; nextstatus = r;
	  }
	}
}

void do_open(int secaddr)
{
    int sa;

	/* don't open file before loading (only get name) */
	if (dont_open==1) return;

	sa = secaddr & 0x0f;

	/* find the unit number */
	file_unit = 0;			/* unit is always zero on 64net cable */
	/* set the old OK message */
	if (sa < 0x0f) {
	    if (filename[0]==',') { set_error(62,0,0); sendchar(0); return;}
	    set_error (0, 0, 0);
	}

	/* if the channel is in use, then close it */
	last_unit = 0;
	if (logical_files[file_unit][sa].open == 1)
	  fs64_closefile_g (&logical_files[last_unit][sa]);
	debug_msg ("*** Opening [%s] on channel $%02x\n",filename, sa);
	if (fs64_openfile_g (curr_dir[last_unit][curr_par[last_unit]],
			       filename, &logical_files[file_unit][sa]))
	{
	  /* open failed */
	  sendchar (4);	/* file not found */
	}
	else {
	  /* release c64 end */
	  sendchar (0);
	}
	  nextstatus = -2;
	  nextchar = 0;
}

void do_close(int secaddr)
{
    int sa,i,j;
	/* close a file */
	i = 0;			/* unit is always 0 */
	sa = secaddr & 0x0f;
	debug_msg ("Closing logical file $%02x\n", sa);
	fs64_closefile_g (&logical_files[i][sa]);
	if (sa == 0xf)
	{
	    /* Closing command channel forces all files shut on the
	       drive */
	    for (j = 0; j < 15; j++)
	      fs64_closefile_g (&logical_files[i][j]);
	}
	/* XXX close is always successfull? */
	sendchar (0);
}

void do_save(void) {
	int startaddr = 0xffff, endaddr = 0x0000, bc = 0, i = 0;
	int s, ms;
	uchar buff[256];
	fs64_file savefile;

	/* we already have filename */
	debug_msg ("Saving file: %s\n",filename);

	/* clear this flag */
	dont_open=0;

	/* print SAVING message */
	c64print ((uchar*)"\rSAVING ");
	c64print (filename);

	client_turbo_speed();

	/* search for file */
	gettimer (&s, &ms);
	last_unit = 0;
	if (fs64_openfile_g (curr_dir[last_unit][curr_par[last_unit]],
			     filename, &savefile))
	{
	  /* file not found */
	  /* postpend ",W" to the filename and try again */
	  strcat ((char*)filename, ",W");
	  if (fs64_openfile_g (curr_dir[last_unit][curr_par[last_unit]],
			       filename, &savefile))
	  {
	    /* cannot open file for some reason */
	    /* simply abort */
	    sendchar (254); sendchar (0);
	    return;
	  }
	}
	else
	{
	  /* file exists */
	  set_error (63, 0, 0);
	  sprintf((char*)buff,"\r%s",dos_status[last_unit]);
	  c64print(buff);

	  sendchar (254); sendchar (128);
	  return;
	}

	/* file is open, proceed to get address range */
	startaddr = c64peek (0xc1) + 256 * c64peek (0xc2);
	endaddr = c64peek (0xae) + 256 * c64peek (0xaf);

	/* be user friendly on C= side and print addresses */
        sprintf((char*)buff," $%04x $%04x",startaddr,endaddr);
        c64print(buff);

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
	    for (i = 0; i < bc; i++)
	      buff[i] = charget ();
	  }
	  /* write block */
	  for (i = 0; i < bc; i++)
	    if (fs64_writechar (&savefile, buff[i]))
	    {
	      debug_msg ("Closing save file due to error\n");
	      fs64_closefile_g (&savefile);
	      sendchar (254); sendchar (0);
	      return;
	    }
	  startaddr += bc;
	}

	client_normal_speed();

	debug_msg ("Closing save file\n");
	fs64_closefile_g (&savefile);

	{
	  int s2, ms2;
	  gettimer (&s2, &ms2);
	  if (ms > ms2)
	  {
	    ms2 += 1000000;
	    s2--;
	  }
	  s = s2 - s;
	  ms = ms2 - ms;
	  debug_msg ("Save time: %d.%d\n", s, ms);
	}
	sendchar (254); sendchar (0);
}

void do_load(void)
{
	/* Load a file using highly optimised routines */
	int filestart=0;
	int s, ms, sa, mode, startaddr;
	uchar c;

	uchar buff[256];
	fs64_file loadfile;

	/* sec addr */
	mode = charget ();
	sa = lastlf & 0x0f;

	/* for correct directory handling */
	talklf = lastlf;

	/* we already have filename */
	debug_msg ("Loading file: %s, sa=%i\n",filename,sa);

	/* clear this flag */
	dont_open=0;

	/* print SEARCHING message */
	c64print ((uchar*)"\rSEARCHING FOR ");
	c64print (filename);
	c64print ((uchar*)"\r");
	/* search for file */
	last_unit = 0;
	if (fs64_openfile_g (curr_dir[last_unit][curr_par[last_unit]],
			     filename, &loadfile))
	{
	  if ((dos_status[last_unit][0] == '6') && (dos_status[last_unit][1] == '2'))
	  {
	    /* file not found, so check "path" */
	    uchar foo[256] = {0};
	    uchar *tmp;

	    if ((tmp=(uchar*)strchr((char*)filename,':'))!=NULL)
		sprintf((char*)foo, "%d%s", pathdir, tmp);

	    if (!foo[0])
	      sprintf ((char*)foo, "%d:%s", pathdir, filename);
	    if (fs64_openfile_g (partn_dirs[last_unit][pathdir], foo, &loadfile))
	    {
	      /* file not found */
	      sendchar (254); sendchar (4);
	      return;
	    }
	  }
	  else
	  {
	    /* file not found */
	    sendchar (254); sendchar (4);
	    return;
	  }
	}

	/* file found - so load */
	if (mode==0) {
		c64print ((uchar*)"LOADING");
	} else {
		c64print ((uchar*)"VERIFYING");
	}

	gettimer (&s, &ms);
	client_turbo_speed ();
#ifdef DEBUG
	printf ("About to run fs64_readchar\n");
#endif

	/* get load address */
	if (fs64_readchar (&loadfile, &c))
	{
	  /* empty file */
	  sendchar (254); sendchar (0);
	}
	else
	  filestart = c;
	if (fs64_readchar (&loadfile, &c))
	{
	  /* empty file */
	  sendchar (254); sendchar (0);
	}
	else
	  filestart += c * 256;

	/* which start address to use? */
	if (!sa)
	    /* kernel (provided by caller) */
	    startaddr = c64peek (0xc3) + 256 * c64peek (0xc4);
	else
	    /* or that one from file */
	    startaddr = filestart;

	/* be user friendly on C= side and print addresses */
        sprintf((char*)buff," $%04x",startaddr);
        c64print(buff);
#ifdef DEBUG
	debug_msg("FS: $%04x, SA: $%04x\n, mode:%i",filestart,startaddr,mode);
	debug_msg ("About to send file\n");
#endif
	/* check whether to load or verify */
	if (mode==0) {
	    do_load_lowlevel(&loadfile, &startaddr);
	} else {
	    do_verify_lowlevel(&loadfile, &startaddr);
	}

	/* successful load */
	fs64_closefile_g (&loadfile);
	c64poke (0x90, 0x40);	/* eof */
	set_error (0, 0, 0);
	/* disconnect c64 */
#ifdef DEBUG
	printf ("Disconnecting 64\n");
#endif

	client_normal_speed ();
	{
	  int s2, ms2;
	  gettimer (&s2, &ms2);
	  if (ms > ms2)
	  {
	    ms2 += 1000000;
	    s2--;
	  }
	  s = s2 - s;
	  ms = ms2 - ms;
	  debug_msg ("Load time: %d.%d\n", s, ms);
	}
	c64poke (0xae, startaddr & 0xff);
	c64poke (0xaf, startaddr / 256);
	sendchar (254);
	sendchar (0);
}

void do_verify_lowlevel(fs64_file *loadfile, int *startaddr) {

int n, bc = 0, l = 0;
int differences = 0;
uchar c;
uchar buff[256];

  while (!fs64_readchar (loadfile, &c))	{
	  l++;
	  buff[bc++] = c;
	  if (bc == 254) {
		sendchar (255);
		sendchar (2);
		sendchar (bc + 1);
		sendchar (*startaddr & 0xff);
		sendchar (*startaddr / 256);
		for (n = 0; n < bc; n++)
		  if (charget()!=buff[n]) differences++;
		*startaddr += bc;
		bc = 0;
	    }
  }
    /* final partial block */
    if (bc) {
	  /* send block */
	  sendchar (255);
	  sendchar (2);
	  sendchar (bc + 1);
	  sendchar (*startaddr & 0xff);
	  sendchar (*startaddr / 256);
	  for (n = 0; n < bc; n++)
	    if (charget()!=buff[n]) differences++;
	  *startaddr += bc;
	  bc = 0;
    }

	/* be user friendly on C= side and print addresses */
        sprintf((char*)buff," $%04x\r",*startaddr);
        c64print(buff);

    if (differences!=0)
	c64print((uchar*)"?VERIFY ERROR\r");

}

void do_load_lowlevel(fs64_file *loadfile, int *startaddr) {

int n, bc = 0, l = 0;
uchar c;
uchar buff[256];

	while (!fs64_readchar (loadfile, &c))
	{
	  l++;
	  buff[bc++] = c;
	  if (bc == 254)
	  {
#ifdef DEBUG
	    printf ("Sending block(in while loop)\n");
#endif
	    /* send block (using fastest allowed protocol) */
	    if (allowFishLoad&&(*startaddr < 0xcf00))
	      {
#ifdef DEBUG
		printf ("Sending block addr<0xcf00 (in while loop)\n");
#endif
		fastsendblock (*startaddr, 254, buff);
#ifdef DEBUG
		printf ("Done.\n");
#endif
	      }
	    else
	      {
#ifdef DEBUG
		printf ("using raw sendchar(in while loop)\n");
#endif
		sendchar (255);
		sendchar (1);
		sendchar (bc + 1);
		sendchar (*startaddr & 0xff);
		sendchar (*startaddr / 256);
		for (n = 0; n < bc; n++)
		  sendchar (buff[n]);

	      }
	    *startaddr += bc;
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
	  sendchar (*startaddr & 0xff);
	  sendchar (*startaddr / 256);
	  for (n = 0; n < bc; n++)
	    sendchar(buff[n]);

	  *startaddr += bc;
	  bc = 0;
	}
	/* be user friendly on C= side and print addresses */
        sprintf((char*)buff," $%04x",*startaddr);
        c64print(buff);

#ifdef DEBUG
	printf ("Successful load\n");
#endif
}


void do_boot(void)
{
	/* boot sequence */
	debug_msg ("\nBoot, starting server on device #%i\n",devnum);

	client_type = charget();
	debug_msg ("Client machine reported itself as %s\n",clientdep_name[client_type]);
	sendchar (devnum);

	{
	  uchar temp[80];
	  sprintf ((char*)temp, "\r 64NET/2 SERVER %s\r", server_version ());
	  c64print (temp);
	  sprintf ((char*)temp, "DEVICE: %d, MODE: %s\r", devnum, clientdep_name[client_type]);
	  c64print (temp);
	}
	c64print ((uchar*)"\r");
	sendchar (254);	sendchar (0);

}

/* to make it more readable */

#define FILETABLE clientdep_tables[client_type][0]
#define DEVTABLE  clientdep_tables[client_type][1]
#define SATABLE   clientdep_tables[client_type][2]

void do_open_small (void) {
    int i, fn, no;
    
    no = c64peek (0x98);
    /* are there free slots? */
    if (no > 9)	{
	  debug_msg("Too many opened files\n");
	  c64print((uchar*)"?TOO MANY FILES  ERROR\r");
	  sendchar (254); sendchar (1);
	  return;
    }

    fn = c64peek (0xb8);
    for (i = 0; i < no; i++)
	if (c64peek (FILETABLE + i) == fn) {
	      /* file already open */
	      debug_msg ("File %i already open\n",fn);
	      if (c64peek(0x9d)!=0) c64print((uchar*)"?FILE OPEN  ERROR\r");
	      sendchar (254); sendchar (0);
	      return;
	}
    /* everything seems OK, so add new entry */
    c64poke (FILETABLE + no, fn);
    c64poke (DEVTABLE + no, c64peek (0xba));
    c64poke (SATABLE + no, c64peek (0xb9) | 0x60);
    no++;
    c64poke (0x98, no);
    /* release client, it will send filename and fall into do_open right now */
    sendchar(254); sendchar(0);
}

void do_close_small(void) {
    int i, fn, no, dev, sa;

    /* client put number of file to close in $b8, error here means to fall to Kernal close */
    fn = c64peek (0xb8);
    no = c64peek (0x98);
    for (i = 0; i < no; i++)
	if (c64peek (FILETABLE + i) == fn) {
		/* i == offset in FILETABLE */
		debug_msg ("File %i open, will be closed\n", fn);
		dev = c64peek(DEVTABLE+i);
		if (dev!=devnum) {
		    /* this is not our device, Kernal must handle it */
		    sendchar(254); sendchar(1);
		    return;
		}
		sa = c64peek(SATABLE+i);
		if (i!=no) {
		/* need to fix tables - put last in place of closed */
		    c64poke(FILETABLE+i,c64peek(FILETABLE+no));
		    c64poke(DEVTABLE+i,c64peek(DEVTABLE+no));
		    c64poke(SATABLE+i,c64peek(DEVTABLE+no));
		}
		c64poke(0x98, no-1);
		do_close(sa);	/* command loop will ignore 128 from do_close */
		sendchar(0xfc);
		return;
	}
    sendchar(254); sendchar(0x40);
}

void do_chkinout(int type) {

    int i, no, fn, dev, sa;

    /* error code from this routine means to fall to Kernal */
    no = c64peek (0x98);
    fn = c64peek (0xb8);
    for (i = 0; i < no; i++)
	if (c64peek (FILETABLE + i) == fn) {
		dev = c64peek(DEVTABLE+i);
		sa  = c64peek(SATABLE+i);
		if (dev==devnum) {
#ifdef DEBUG
		    debug_msg("chkin/out on 64net/2 device\n");
#endif
		    sa  = c64peek(SATABLE+i);
		    c64poke(0xba, dev);
		    c64poke(0xb9, sa);
		    if (type==0) {
		    /* chkin */
			c64poke(0x99, dev);
			talklf = sa;
		    } else {
		    /* chkout */
			c64poke(0x9a, dev);
			listenlf = sa;
			    /* this is from comm-lpt */
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
		    /* release client */
		    sendchar(0xfc);
		    return;
		} else {
		    debug_msg("not our device\n");
		    sendchar(254); sendchar(1);
		    return;
		}
	}
    debug_msg("file not open for chkin/out\n");
    sendchar(254); sendchar(1);
}

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

/* 
   XXX - Linux doesnt seem to like fish save (fishload is fine)
*/
int allowFishLoad=FISHLOAD;	/* 1 = on */
int allowFishSave=FISHSAVE;


/* Temporary variables used to store filenames and things for
   fs_openfile_g etc 
   */
int fnlen;
uchar filename[256];
int file_unit;

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
    int sa;
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
		sendchar (128);		/* XXX: EOF first */
		sendchar (199);
		return;
	    }
	  /* send status */
	    sendchar (0);
	    sendchar (dos_status[last_unit][0]);
	    /* update dos status */
	    memmove (&dos_status[last_unit][0], &dos_status[last_unit][1], dos_stat_len[last_unit]);
	    /* reduce length remainin */
	    dos_stat_len[last_unit]--;
	}			/* end of command channel read */
	/* non command channel read */
	else
	{
	  int r;
	  r = fs64_readchar (&logical_files[file_unit][sa], &c);
	  if (r == -1)
	  {
	    /* couldnt read a char */
/*                    debug_msg("EOF on lf#%d\n",sa); */
	    /* since this is normal 64net where FILE NOT FOUND can
	       be explicitly indicated, we know its EOF */
	    sendchar (128);	/* XXX should be 64 EOF? */
	    sendchar (199);
	  }			/* end of failed to read from channel */
	  else
	  {
	    /* file read succeeded */
	    sendchar (r);
	    sendchar (c);
	  }			/* end of successful read from file */
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
//	  sendchar (4);	/* file not found */
	  sendchar (0);
	}
	else {
	  /* release c64 end */
	  sendchar (0);
	}
}

void do_close(int secaddr)
{
    int sa,i,j;
	/* close a file */
	debug_msg ("Close\n");
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
	sendchar (128);
	debug_msg ("Done close\n");
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
	    sendchar (254);
	    sendchar (0);
	    return;
	  }
	}
	else
	{
	  /* file exists */
	  set_error (63, 0, 0);
	  sprintf((char*)buff,"\r%s",dos_status[last_unit]);
	  c64print(buff);

	  sendchar (254);
	  sendchar (128);
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
	      sendchar (254);
	      sendchar (0);
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
	    ms2 += 1000;
	    s2--;
	  }
	  s = s2 - s;
	  ms = ms2 - ms;
	  debug_msg ("Save time: %d.%d\n", s, ms);
	}
	sendchar (254);
	sendchar (0);
}

void do_load(void)
{
	/* Load a file using highly optimised routines */
	int startaddr, bc, n, l = 0;
	int filestart=0;
	int s, ms, sa, mode;
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
	      sendchar (254);
	      sendchar (4);
	      return;
	    }
	  }
	  else
	  {
	    /* file not found */
	    sendchar (254);
	    sendchar (4);
	    return;
	  }
	}

	/* file found - so load */
	c64print ((uchar*)"LOADING");

	gettimer (&s, &ms);
	client_turbo_speed ();
#ifdef DEBUG
	printf ("Done client_turbo_speed\n");
	printf ("About to run fs64_readchar\n");
#endif

	/* get load address */
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

	/* check whether to load or verify */
	if (mode==1) {
	    debug_msg ("File verifying isn't supported yet, loading instead");
	}    

	bc = 0;

#ifdef DEBUG
	debug_msg("FS: $%04x, SA: $%04x\n, mode:%i",filestart,startaddr,mode);
	debug_msg ("About to send file\n");
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
		sendchar (startaddr & 0xff);
		sendchar (startaddr / 256);
		for (n = 0; n < bc; n++)
		  sendchar (buff[n]);

	      }
	    startaddr += bc;
	    bc = 0;
	  }
	}

	/* BUG!!! this printf here is for small delay before protocol change */
//	printf ("About to send last partial block\n");

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
	    sendchar(buff[n]);

	  startaddr += bc;
	  bc = 0;
	}
	/* be user friendly on C= side and print addresses */
        sprintf((char*)buff," $%04x",startaddr);
        c64print(buff);

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

	client_normal_speed ();
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
	  debug_msg ("Load time: %d.%d\n", s, ms);
	}
	c64poke (0xb2, startaddr & 0xff);
	c64poke (0xb3, startaddr / 256);
	sendchar (0);
	sendchar (0);
	sendchar (254);
	sendchar (0);
}

void do_boot(void)
{
	/* boot sequence */
	debug_msg ("\nBoot, starting server on device #%i\n",devnum);

	sendchar (devnum);

	{
	  uchar temp[80];
	  sprintf ((char*)temp, "\r 64NET/2 SERVER %s", server_version ());
	  c64print (temp);
	}
	c64print ((uchar*)"\r");
	sendchar (254);
	sendchar (0);

}


#ifdef OLD_IEC_STUFF
void do_open(void)
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
	  return;
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
	      return;
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
	    return;
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
	    return;
	  }
	  lf_flags[i][secaddr] = 2;
	  sendchar (254);
	  sendchar (0);
	  return;
	}			/* end of valid device */
	/* release c64 end */
	sendchar (254);
	sendchar (0);
}
#endif /* OLD_IEC_STUFF */

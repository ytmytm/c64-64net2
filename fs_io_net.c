/*
   InterNET File system module for 64net/2
   (C) Copyright Paul Gardner-Stephen 1996

   NOTES:
   File system will appear as:
   /hosts/<host_names>/
   /nnn/nnn/nnn/nnn/
   /listen/
   /server/

   Each with the following contents:
   services/<service_names>
   ports/nnn/nnn
   aliases

   Description:
   /hosts/ is the list of hostnames in /etc/hosts, or aliases in the
   64net.hosts file (BUG! - not implemented).  Each hostname
   is a subdirectory to the internet services offered for any
   server (services/ & ports/)
   /nnn/nnn/nnn/nnn/ is the list of all possible IP numbers, with sub
   dirs as described above.
   /server/ is the server 64net resides upon
   /aliases is a PRG file containing (in BASIC list form) the list
   of IP numbers and names this server has.  This will look
   up the host to gain this info.
   Legend:
   nnn - IP Quad (eg 127.0.0.1 = 127/0/0/1) *OR* a byte of a port number
   host_names - The list of hosts in /etc/hosts
   service_name - The name of a service offered on a standard port #
   (This inclusion is added as to simplify life)


   This file system will provide several facilities:
   * Anonymous FTP access
   * HTTP Access
   * IRC Access (pre-parsed??)
   * TCP/IP Port level connections (active, and passive
   on either any port, or a specified one)
   * IP number & hostname of localhost (64net server)


   This module will be (initially) unavailable in the MS-DOS and possibly
   also AMIGA versions, pending information on using sockets under either
   of these envirnoments.  The code should be sufficiently general to work
   under most unicies.

 */

#include "config.h"
#include "fs.h"
#include <fcntl.h>

#ifdef UNIX
#define net_errno errno
#include <arpa/inet.h>
#endif

#ifdef WINDOWS
#define net_errno WSAGetLastError()
/* XXX make dummy ones */
void setservent (int a) { }
void endservent (void) { }
/* replace by something from WinSock to set socket into non-blocking mode */
#define O_NONBLOCK 0
#define F_SETFL 0
void fcntl(int a, int b, long c) { }
#endif

#ifdef AMIGA
#include <bsdsocket.h>
#include <pragmas/socket_pragmas.h>
#include <proto/socket.h>
#include <exec/exec.h>
struct Library BSDBase;
int no_net;
#endif

int fs_net_dirtype (uchar *path);

/* --normal filesystem functions-------------------------------------- */

int 
fs_net_createfile (uchar *path, uchar *name, int t, fs64_file * f)
{
    if(no_net != 1)
    {
	/* cant create a new file on the net file space */
	return (-1);
    }
    else
	return (-1);
}


int 
fs_net_closefile (fs64_file * f)
{
  if(no_net != 1)
    {
      if (f->socket > -1)
	close (f->socket);
      if ((f->msocket > -1) && (f->sockmode == 0))
	close (f->msocket);
      f->open = 0;
  return (0);
    }
  else
    return (-1);
}

int 
fs_net_readchar (fs64_file * f, uchar *c)
{
  if(no_net != 1)
    {
      if (f->socket == -1)
	{
	  if (f->sockmode == 0)
	    {
	      int spare;
	      /* listen with out a connection */
	      f->socket = accept (f->msocket, (struct sockaddr *) &f->sockaddr, (socklen_t*)&spare);
	      if (f->socket > -1)
		{
		  /* woo hoo! connected! */
		  fcntl (f->socket, F_SETFL, O_NONBLOCK);
		  perror ("64net/2");
		  *c = 'C';
		  return (2);
		}
	      else
		{
		  *c = 0x00;
		  return (2);
		}
	    }
	  else
	    {
	      /* bad socket on active */
	      f->open = 0;
	      *c = 199;
	      return (-1);
	    }
	}
      else
	{
	  /* read a char from the stream */
	  debug_msg ("reading\n");
	  if (read (f->socket, c, 1) == 1)
	    {
	      /* success! */
	      debug_msg ("read char\n");
	      if (net_errno)
		perror ("64net/2");
	      return (0);
	    }
	  else
	    {
	      debug_msg ("read nothing\n");
	      if (net_errno != 0)
		{
		  if (net_errno == 35)
		    {
		      /* Resource Temporarily Unavailable
			 (ie no char yet) */
		      *c = 0;
		      return (2);
		    }
		  else
		    {
		      perror ("64net/2");
		      debug_msg ("Closing file (%d)\n", net_errno);
		      close (f->socket);
		      f->open = 0;
		      *c = 199;
		      return (-1);
		    }
		}
	      else
		{
		  *c = 0x00;
		  return (2);
		}
	    }
	  return (0);
	}
    }
  else
    return (-1);
}

int 
fs_net_writechar (fs64_file * f, uchar c)
{
  if(no_net != 1)
    {
      if (f->socket > -1)
	{
	  write (f->socket, &c, 1);
	  if (net_errno)
	    perror ("64net/2");
	}
      return (0);
    }
  else
    return (-1);
}

int 
fs_net_getinfo (fs64_direntry * de)
{
  if(no_net != 1)
    return (0);
  else
    return (-1);
}

int 
fs_net_openfile (fs64_file * f)
{
  if(no_net != 1)
    {
      /* open a file */
      
      debug_msg ("Openning\n");
      
      if (f->sockmode == 0)
	{
	  /* listen on port */
	  f->msocket = socket (AF_INET, SOCK_STREAM, 0);
	  f->sockaddr.sin_family = AF_INET;
	  f->sockaddr.sin_addr.s_addr = INADDR_ANY;
	  f->sockaddr.sin_port = htons (f->port);
	  f->socket = -1;		/* connection not active */
	  
	  if (bind (f->msocket, (struct sockaddr *) &f->sockaddr, sizeof (f->sockaddr)))
	    {
	      /* bind error */
	      debug_msg ("Bind error\n");
	      perror ("64net/2");
	      return (-1);
	    }
	  
	  if (listen (f->msocket, 1))
	    {
	      /* socket listen error */
	      debug_msg ("listen error\n");
	      perror ("64net/2");
	      return (-1);
	    }
	  
	  /* mark file open and return */
	  f->open = 1;
	  return (0);
	}
      else
	{
	  /* active connect */
	  uchar temp[32];
	  f->socket = socket (AF_INET, SOCK_STREAM, 0);
	  f->sockaddr.sin_family = AF_INET;
	  sprintf ((char*)temp, "%d.%d.%d.%d", ((f->ip >> 24) + 256) & 0xff, ((f->ip >> 16) + 256) & 0xff,
		   ((f->ip >> 8) + 256) & 0xff, ((f->ip & 0xff) + 256) & 0xff);
	  f->sockaddr.sin_addr.s_addr = inet_addr ((char*)temp);
	  f->sockaddr.sin_port = htons (f->port);
	  if ((connect (f->socket, (struct sockaddr *) &f->sockaddr, sizeof (f->sockaddr))) == -1)
	    {
	      /* connect error */
	      debug_msg ("Connect error\n");
	      perror ("64net/2");
	      return (-1);
	    }
	  fcntl (f->socket, F_SETFL, O_NONBLOCK);
	  perror ("non-blocking");
	  f->open = 1;
	  return (0);
	}
      
      
      return (-1);
    }
  else
    return (-1);
}

int 
fs_net_getopenablename (fs64_file * f, fs64_direntry * de)
{
  uchar pe[8][17];
  int pec = 0, pel = 0;
  int i;
  uchar path[256];

  if(no_net != 1)
  {
    strcpy (path, de->filesys.fspath);
    
    /* seperate path elements into a list */
    for (i = 2; i < strlen (path); i++)
      {
	if (path[i] == '/')
	  {
	    pe[pec++][pel] = 0;
	    pel = 0;
	  }
	else
	  pe[pec][pel++] = path[i];
      }
    /* terminate last term */
    if (pel)
      pe[pec][pel] = 0;
    else
      pec--;
    
    debug_msg ("PEC: %d\n", pec);
    for (i = 0; i <= pec; i++)
      debug_msg ("PE %d [%s]\n", i, pe[i]);
    
    f->sockmode = 1;		/* active */
    
    /* Calculate internet address and socket modes etc */
    debug_msg ("dir type %d\n", de->dirtype);
    switch (de->dirtype)
      {
      case net_PORTDIR:
	{
	  switch (pec)
	    {
	    case 5:
	      /* IP */
	      f->ip = (1 << 24) * atol (pe[0]);
	      f->ip += (1 << 16) * atol (pe[1]);
	      f->ip += (1 << 8) * atol (pe[2]);
	      f->ip += atol (pe[3]);
	      break;
	    case 2:
	      if (!strcmp ("LISTEN", pe[0]))
		{
		  /* listen */
		  f->ip = 0xffffffff;
		  f->sockmode = 0;
		}
	      else
		{
		  /* server */
		  f->ip = 0x7f000001;
		}
	      break;
	    case 3:
	      /* hosts */
	      break;
	    }
	  f->port = atol (pe[pec]) + 256 * atol (de->fs64name);
	}
	break;
      case net_SERVICESDIR:
	switch (pec)
	  {
	  case 4:
	    /* ip */
	    f->ip = (1 << 24) * atol (pe[0]);
	    f->ip += (1 << 16) * atol (pe[1]);
	    f->ip += (1 << 8) * atol (pe[2]);
	    f->ip += atol (pe[3]);
	    break;
	  case 1:
	    /* listen or server */
	    if (!strcmp (pe[0], "SERVER"))
	      f->ip = 0x7f000001;	/* server */
	    else
	      {
		f->ip = 0xffffffff;	/* listen */
		f->sockmode = 0;
	      }
	    break;
	  case 2:
	    /* hosts */
	    return (-1);
	  }
	f->port = de->blocks;
	break;
      default:
	return (-1);
      }
    
    debug_msg ("IP: %08x, port %04x, mode %d\n", f->ip, f->port, f->sockmode);
    
    return (0);
  }
  else
    return (-1);
}

int 
fs_net_scratchfile (fs64_direntry * de)
{
  if(no_net != 1)
    return (1);
  else
    return (-1);
}

int 
fs_net_headername (uchar *path, uchar *header, uchar *id, int par)
{
  if(no_net != 1)
    {
      /* use right 16 chars from path */
      int i, j;
      
      header[0] = 0;
      
  i = strlen (path) - 1;
      /* strip trailing '/' */
      if (path[i] == '/')
	i--;
      /* find start of this "bit" */
      while ((i) && (path[i] != '/'))
	i--;
      /* strip leading '/' on name */
      if (path[i] == '/')
	i++;
  /* copy  chars */
      j = 0;
      for (; !((path[i] == '/') || (path[i] == 0)); i++)
	header[j++] = path[i];
      /* end f the string */
      header[j] = 0;
      /* default */
      if ((!strcmp (path, "/")) || (header[0] == 0))
	sprintf ((char*)header, "INTERNET");
      
      strcpy (id, "TCPIP");
      
      return (0);
    }
  else
    return (-1);
}

int 
fs_net_findnext (fs64_direntry * de)
{
  int i;
  uchar temp[255];

  if(no_net != 1)
  {
      switch (de->dirtype)
      {
      case net_ROOTDIR:
      {
	  for (i = 0; i < 16; i++)
	      de->fs64name[i] = 0xa0;
	  de->filetype = cbm_DIR | cbm_CLOSED | cbm_LOCKED;
	  de->blocks = 0;
	  de->invisible = 0;
	  switch (de->intcount)
	  {
	  case 0:
	      strcpy (de->fs64name, "SERVER");
	      break;
	  case 1:
	      strcpy (de->fs64name, "HOSTS");
	      break;
	  case 2:
	      strcpy (de->fs64name, "LISTEN");
	      break;
	  default:
	      sprintf ((char*)temp, "%d", de->intcount - 3);
	      strcpy (de->fs64name, temp);
	  }
	  if (de->intcount == 259)
	  {
	      de->active = 0;
	      return (1);
	  }
	  de->intcount++;
	  sprintf ((char*)de->realname, "%s%s/", de->filesys.fspath, de->fs64name);
	  for (i = 0; i < 16; i++)
	      if (de->fs64name[i] == 0)
		  de->fs64name[i] = 0xa0;
	  return (0);
      };
	  break;
      case net_SERVERDIR:
      {
	  for (i = 0; i < 16; i++)
	      de->fs64name[i] = 0xa0;
	  de->filetype = cbm_DIR | cbm_CLOSED | cbm_LOCKED;
	  de->blocks = 0;
	  de->invisible = 0;
	  switch (de->intcount)
	  {
	  case 0:
	      strcpy (de->fs64name, "SERVICES");
	      break;
	  case 1:
	      strcpy (de->fs64name, "PORTS");
	      break;
	  case 2:
	      strcpy (de->fs64name, "ALIASES");
	      de->filetype = cbm_PRG | cbm_CLOSED | cbm_LOCKED;
	      break;
	  default:
	  {
	      de->active = 0;
	      return (1);
	  };
	  }
	  de->intcount++;
	  sprintf ((char*)de->realname, "%s%s/", de->filesys.fspath, de->fs64name);
	  for (i = 0; i < 16; i++)
	      if (de->fs64name[i] == 0)
		  de->fs64name[i] = 0xa0;
	  return (0);
      };
	  break;
      case net_BYTEDIR:
      {
	  for (i = 0; i < 16; i++)
	      de->fs64name[i] = 0xa0;
	  de->filetype = cbm_DIR | cbm_CLOSED | cbm_LOCKED;
	  de->blocks = 0;
	  de->invisible = 0;
	  switch (de->intcount)
	  {
	  default:
	      sprintf ((char*)temp, "%d", de->intcount);
	      strcpy (de->fs64name, temp);
	  }
	  if (de->intcount == 256)
	  {
	      de->active = 0;
	      return (1);
	  }
	  de->intcount++;
	  sprintf ((char*)de->realname, "%s%s/", de->filesys.fspath, de->fs64name);
	  for (i = 0; i < 16; i++)
	      if (de->fs64name[i] == 0)
		  de->fs64name[i] = 0xa0;
	  return (0);
      };
	  break;
      case net_PORTDIR:
      {
	  for (i = 0; i < 16; i++)
	      de->fs64name[i] = 0xa0;
	  de->filetype = cbm_NET | cbm_CLOSED;
	  de->blocks = 0;
	  de->invisible = 0;
	  switch (de->intcount)
	  {
	  default:
	      sprintf ((char*)temp, "%d", de->intcount);
	      strcpy (de->fs64name, temp);
	  }
	  if (de->intcount == 256)
	  {
	      de->active = 0;
	      return (1);
	  }
	  de->intcount++;
	  sprintf ((char*)de->realname, "%s%s/", de->filesys.fspath, de->fs64name);
	  for (i = 0; i < 16; i++)
	      if (de->fs64name[i] == 0)
		  de->fs64name[i] = 0xa0;
	  return (0);
      };
	  break;
      case net_SERVICESDIR:
      {
	  for (i = 0; i < 16; i++)
	      de->fs64name[i] = 0xa0;
	  de->blocks = 0;
	  de->invisible = 0;
      {
	  struct servent *se;
	  struct protoent *pn = getprotobyname ("tcp");
	  se = getservbyport (htons (de->intcount), pn->p_name);
	  while ((!se) && (de->intcount < 1023))
	  {
	      de->intcount++;
	      se = getservbyport (htons (de->intcount), pn->p_name);
	  }
	  if (se)
	  {
	      int j;
	      de->blocks = ntohs (se->s_port);
	      de->filetype = cbm_NET | cbm_CLOSED | cbm_LOCKED;
	      sprintf ((char*)temp, "%s", se->s_name);
	      temp[16] = 0;
	      for (j = 0; j < 16; j++)
		  temp[j] = toupper (temp[j]);
	      strcpy (de->fs64name, temp);
	  }
	  else
	  {
	      endservent ();
	      de->active = 0;
	      return (-1);
	  }
      }
	  de->intcount++;
	  sprintf ((char*)de->realname, "%s%s/", de->filesys.fspath, de->fs64name);
	  for (i = 0; i < 16; i++)
	      if (de->fs64name[i] == 0)
		  de->fs64name[i] = 0xa0;
	  return (0);
      };
	  break;
      default:
	  de->active = 0;
	  return (-1);
      }
  }
  else
      return (-1);
}

int 
fs_net_openfind (fs64_direntry * de, uchar *path2)
{
    /* Dir searches are easy in the net fs
       BUG: Doesnt allow for transparent FTP access yet */
    
    uchar path[1024];
    
    if(no_net != 1)
    {
	/* step 1 - get dir of preceding @ if present */
	if (path2[0] == '@')
	    strcpy (path, path2);
	else
	    strcpy (path, &path2[1]);
	
	/* step 2 - work out the directory type */
	de->dirtype = fs_net_dirtype (path);
	/* rewind services file */
	if (de->dirtype == 4)
	    setservent (0);
	
	/* step 3 - search from start of dir */
	de->intcount = 0;
	
	/* step 4 - Copy path to de->filesys.fspath */
	strcpy (de->filesys.fspath, path);
	
	de->active = 1;
	return (0);
    }
    else
	return (-1);
}

int 
fs_net_writeblock (fs64_file * f)
{
    if(no_net != 1)
    {
	debug_msg ("Pretending to write a block to a inet stream\n");
	return (-1);
    }
    else    
	return (-1);
}

int 
fs_net_readblock (fs64_file * f)
{
    if(no_net != 1)
    {
	/* Read a block from a network file stream */
	return (-1);
    }
    else
	return (-1);
}

/* --Other routines (inet and filesystem support etc..)--------------- */

int 
fs_net_dirtype (uchar *path2)
{
    /* what type of synthesised directory is this? 
       BUG: Doesnt allow for transparent FTP access yet */
    
    uchar pe[8][17];
    int pec = 0, pel = 0;
    int i;
    uchar path[256];

    if(no_net != 1)
    {
	strcpy (path, &path2[1]);
	
	if (!strcmp (path, "/"))
	    return (net_ROOTDIR);
	
	/* seperate path elements into a list */
	for (i = 1; i < strlen (path); i++)
	{
	    if (path[i] == '/')
	    {
		pe[pec++][pel] = 0;
		pel = 0;
	    }
	    else
		pe[pec][pel++] = path[i];
	}
	/* terminate last term */
	if (pel)
	    pe[pec][pel] = 0;
	else
	    pec--;
	
	debug_msg ("PEC: %d\n", pec);
	for (i = 0; i <= pec; i++)
	    debug_msg ("PE %d [%s]\n", i, pe[i]);
	
	/* work out the dir type */
	if (!strcmp (pe[0], "HOSTS"))
	{
	    if (pec == 0)
		return (net_HOSTSDIR);
	    if (pec == 1)
		return (net_SERVERDIR);
	    if (!strcmp (pe[2], "SERVICES"))
		return (net_SERVICESDIR);
	    if (!strcmp (pe[2], "PORTS"))
	    {
		if (pec == 1)
		    return (net_BYTEDIR);
		if (pec == 2)
		    return (net_PORTDIR);
	    }
	}
	if (!strcmp (pe[0], "LISTEN"))
	{
	    if (pec == 0)
		return (net_SERVERDIR);
	    if (!strcmp (pe[1], "SERVICES"))
		return (net_SERVICESDIR);
	    if (!strcmp (pe[1], "PORTS"))
	    {
		if (pec == 1)
		    return (net_BYTEDIR);
		if (pec == 2)
		    return (net_PORTDIR);
	    }
	}
	if (!strcmp (pe[0], "SERVER"))
	{
	    if (pec == 0)
		return (net_SERVERDIR);
	    if (!strcmp (pe[1], "SERVICES"))
		return (net_SERVICESDIR);
	    if (!strcmp (pe[1], "PORTS"))
	    {
		if (pec == 1)
		    return (net_BYTEDIR);
		if (pec == 2)
		    return (net_PORTDIR);
	    }
	}
	/* ip based directory */
	if (pec < 3)
	    return (net_BYTEDIR);
	if (pec == 3)
	    return (net_SERVERDIR);
	if (pec == 4)
	{
	    if (!strcmp (pe[4], "SERVICES"))
		return (net_SERVICESDIR);
	    else
		return (net_BYTEDIR);
	}
	if (pec == 5)
	    return (net_PORTDIR);
	
	debug_msg ("Bad net dir!\n");
	return (-1);
    }
    else
    {
	return (-1);
    }
}

int 
fs_net_blocksfree (fs64_filesystem * fs)
{
    if(no_net != 1)
	return (0);
    else
	return (-1);
}

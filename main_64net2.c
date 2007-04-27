/*
   64NET/2 Main Module
   (C)Copyright Paul Gardner-Stephen 1995, 1996, All Rights Reserved
 */

#include "config.h"
#include <signal.h>

#include "dosemu.h"
#include "misc_func.h"
#include "comm-rrnet.h"
#include "version.h"

int no_net = NONET;
#ifdef AMIGA
int steal_parallel_port = 0;
extern Library BSDBase;
#endif

void do_quit(void) {
	rrnet_quit();
    /* a dummy routine that should shut down 64net/2 */
}

void sigint(int dummy) {
    do_quit();
    fatal_error("SIGINT caught");
}

void sigterm(int dummy) {
    do_quit();
    fatal_error("SIGTERM caught");
}

int 
main (int argc, char **argv)
{
  int ch;
  char *config_file="/etc/64net.conf";
  
  signal(SIGINT, sigint);
  signal(SIGTERM, sigterm); 
  
#ifdef DEBUG
  initDebug();
#endif

  while ((ch = getopt(argc, argv, "sc:")) != -1)
    switch (ch) {
    case 'c':
      /* Select config file */
      config_file=optarg;
      break;
#ifdef AMIGA
    case 's': /* Steal the parallel port, don't ask, just take it */
      steal_parallel_port = 1;
      break;
#endif
    case '?':
    default:
      if ((ch!='h')&&(ch!='?'))
	fprintf(stderr,"Illegal option: -'%c'\n",ch);
      fprintf(stderr,"usage: 64net2 [-c config_file] [-s]\n");
      fprintf(stderr,
	      "  -c - Specify alternate config file (default is 64netrc)\n");
      fprintf(stderr,
	      "  -s - [AMIGA only] Steal parallel port without asking.\n");
      fprintf(stderr,"\n");
      exit(-1);
    }
  argc -= optind;
  argv += optind;

#ifdef AMIGA    
  if (stacksize() < 99000)
    {
      printf("Stack needs to be >= 100000 bytes!(%d)\n", stacksize());
      exit(10);
    }
  
  if(BSDBase = OpenLibrary("bsdsocket.library", 0) == NULL)
    {
	printf("AmiTCP not loaded, network filesystem disabled.\n");
	no_net = 1;
    }    
#endif

  printf ("64NET/2 server %s\n",server_version());

  /* read config info */
  read_config(config_file);

  /* initialize dos */
  init_dos ();

  /* all ready, be cute */

  /* do it man! */
  iec_commune(0);

  /* all done */
  return (0);
}

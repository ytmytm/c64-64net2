/*
   64NET/2 Main Module
   (C)Copyright Paul Gardner-Stephen 1995, 1996, All Rights Reserved
 */

#include "config.h"
#include <signal.h>

#include "dosemu.h"
#include "misc_func.h"
#include "comm-lpt.h"
#include "version.h"

int no_net = NONET;
#ifdef AMIGA
int steal_parallel_port = 0;
extern Library BSDBase;
#endif

void do_quit(void) {
    /* a dummy routine that should shut down 64net/2 */
}

void sigint() {
    do_quit();
    fatal_error("SIGINT caught");
}

void sigterm() {
    do_quit();
    fatal_error("SIGTERM caught");
}

int 
main (int argc, char **argv)
{
       signal(SIGINT, sigint);
       signal(SIGTERM, sigterm); 

#ifdef DEBUG
  initDebug();
#endif
    
#ifdef AMIGA
    while(argc > 1)
    {
	switch(argv[argc][1])
	{
	case 's': /* Steal the parallel port, don't ask, just take it */
	    steal_parallel_port = 1;
	    break;

	default:
	    printf("Unknown argument %c\n", argv[argc][1]);
	    break;
	}
	argc--;
	
    }

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
	read_config ((uchar*)"./64netrc");
	/* initialize dos */
	init_dos ();
	/* all ready, be cute */
	printf ("Network started.\n");
	/* do it man! */
	parallel_iec_commune(0);
	/* all done */
	return (0);
}

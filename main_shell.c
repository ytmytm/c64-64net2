/*
   64NET/2 Shell module
   (C)Copyright Paul Gardner-Stephen 1995, 1996, 1998 All Rights Reserved
 */

#include <stdio.h>
#include <signal.h>

#include "fs.h"
#include "dosemu.h"
#include "misc_func.h"
#include "comm-lpt.h"


int steal_parallel_port = 0;
int no_net = 0;
#ifdef AMIGA
extern Library BSDBase;
#endif

int showlist(char *foo);

int 
main (int argc, char **argv)
{
    /*   signal(SIGINT, (void *)fatal_error("SIGINT caught")); */
    /*   signal(SIGTERM, (void *)fatal_error("SIGTERM caught")); */

  debug_mode = 0;

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

    printf ("64NET/2 r0 pre-BETA\n");
    /* read config info */
    read_config ("./64netrc");
    /* initialize dos */
    init_dos ();
    /* all ready, be cute */
    printf ("Network started.\n");
    
    printf("64net/2 CBM DOS shell v0.1.0 (C)Paul Gardner-Stephen 1998\n");
    
    while(1)
      {
	char comm[1024];
	last_unit=0;

	/* Show dos status */
	/* dos_status[last_unit][dos_stat_len[last_unit]]=0;
	   if (dos_status[last_unit][2]==',')
	   printf("%s\n",dos_status[last_unit]);
	   else
	   printf("Status: %s\n",dos_status[last_unit]);
	   set_error(0,0,0); */

	/* Read command */
	printf("64net/2> "); fflush(stdout);
	if (!fgets(comm,1024,stdin)) 
	  {
	    printf("\n64net/2 DOS Shell terminated.\n");
	    exit(0);
	  }
	if (strlen(comm)) comm[strlen(comm)-1]=0; /* strip NL */

	if (!strcasecmp(comm,"help"))
	  {
	    printf("64net/2 DOS Shell\n");
	    printf("$[[nnn][/path]:][filespec]     - Display specified directory\n");
	    printf("@[[nnn][/path]:][filespec]     - List of first specified file\n");
	    printf("All other commands are interpretted as disk commands\n");
	    continue;
	  }

	switch (comm[0])
	  {
	  case 'O':
	    
	  case '$':
	    /* Show directory */
	    showlist(comm);
	    break;
	  case '@': /* Show listing */
	    showlist(&comm[1]);
	    break;
	  default:
	    /* do dos command */
	    strcpy(dos_command[last_unit],comm);
	    dos_comm_len[last_unit]=strlen(comm);
	    do_dos_command();
	    break;
	  }
      }


    /* all done */
    return (0);
}

int showlist(char *dir)
{
  int i;
  unsigned char c;
  fs64_file f;

  /* prepare the 64net file system */
  last_unit = 0;

  /* okay, open file */
  if (!fs64_openfile_g (curr_dir[last_unit][curr_par[last_unit]], dir, &f))
  {
    /* its open! */
    printf("Listing for %s ...\n",dir);

    /* skip load address */
    if (fs64_readchar (&f, &c))
      {
	printf("Error reading directory\n");
	return(-1);
      }
    if (fs64_readchar (&f, &c))
      {
	printf("Error reading directory\n");
	return(-1);
      }

    while (1)
    {
      /* skip line link */
      if (fs64_readchar (&f, &c))
	{ printf("Error reading directory\n");
	return(-1); }
      if (fs64_readchar (&f, &c))
	{ printf("Error reading directory\n");
	return(-1); }
      if (c == 0)
	return (0);		/* end of basic */

      /* line number */
      if (fs64_readchar (&f, &c))
	{ printf("Error reading directory\n");
	return(-1); }
      i = c;
      if (fs64_readchar (&f, &c))
	{ printf("Error reading directory\n");
	return(-1); }
      i += (c * 256);

      printf ("%d ", i);

      /* line text */
      c = 1;
      while (c)
      {
	if (fs64_readchar (&f, &c))
	{ printf("Error reading directory\n");
	return(-1); }
	switch (c)
	{
	case 128:
	  printf ("END");
	  break;
	case 129:
	  printf ("FOR");
	  break;
	case 130:
	  printf ("NEXT");
	  break;
	case 131:
	  printf ("DATA");
	  break;
	case 132:
	  printf ("INPUT#");
	  break;
	case 133:
	  printf ("INPUT");
	  break;
	case 134:
	  printf ("DIM");
	  break;
	case 135:
	  printf ("READ");
	  break;
	case 136:
	  printf ("LET");
	  break;
	case 137:
	  printf ("GOTO");
	  break;
	case 138:
	  printf ("RUN");
	  break;
	case 139:
	  printf ("IF");
	  break;
	case 140:
	  printf ("RESTORE");
	  break;
	case 141:
	  printf ("GOSUB");
	  break;
	case 142:
	  printf ("RETURN");
	  break;
	case 143:
	  printf ("REM");
	  break;
	case 144:
	  printf ("STOP");
	  break;
	case 145:
	  printf ("ON");
	  break;
	case 146:
	  printf ("WAIT");
	  break;
	case 147:
	  printf ("LOAD");
	  break;
	case 148:
	  printf ("SAVE");
	  break;
	case 149:
	  printf ("VERIFY");
	  break;
	case 150:
	  printf ("DEF");
	  break;
	case 151:
	  printf ("POKE");
	  break;
	case 152:
	  printf ("PRINT#");
	  break;
	case 153:
	  printf ("PRINT");
	  break;
	case 154:
	  printf ("CONT");
	  break;
	case 155:
	  printf ("LIST");
	  break;
	case 156:
	  printf ("CLR");
	  break;
	case 157:
	  printf ("CMD");
	  break;
	case 158:
	  printf ("SYS");
	  break;
	case 159:
	  printf ("OPEN");
	  break;
	case 160:
	  printf ("CLOSE");
	  break;
	case 161:
	  printf ("GET");
	  break;
	case 162:
	  printf ("NEW");
	  break;
	case 163:
	  printf ("TAB(");
	  break;
	case 164:
	  printf ("TO");
	  break;
	case 165:
	  printf ("FN");
	  break;
	case 166:
	  printf ("SPC(");
	  break;
	case 167:
	  printf ("THEN");
	  break;
	case 168:
	  printf ("NOT");
	  break;
	case 169:
	  printf ("STEP");
	  break;
	case 170:
	  printf ("+");
	  break;
	case 171:
	  printf ("-");
	  break;
	case 172:
	  printf ("*");
	  break;
	case 173:
	  printf ("/");
	  break;
	case 174:
	  printf ("^");
	  break;
	case 175:
	  printf ("AND");
	  break;
	case 176:
	  printf ("OR");
	  break;
	case 177:
	  printf (">");
	  break;
	case 178:
	  printf ("=");
	  break;
	case 179:
	  printf ("<");
	  break;
	case 180:
	  printf ("SGN");
	  break;
	case 181:
	  printf ("INT");
	  break;
	case 182:
	  printf ("ABS");
	  break;
	case 183:
	  printf ("USR");
	  break;
	case 184:
	  printf ("FRE");
	  break;
	case 185:
	  printf ("POS");
	  break;
	case 186:
	  printf ("SQR");
	  break;
	case 187:
	  printf ("RND");
	  break;
	case 188:
	  printf ("LOG");
	  break;
	case 189:
	  printf ("EXP");
	  break;
	case 190:
	  printf ("COS");
	  break;
	case 191:
	  printf ("SIN");
	  break;
	case 192:
	  printf ("TAN");
	  break;
	case 193:
	  printf ("ATN");
	  break;
	case 194:
	  printf ("PEEK");
	  break;
	case 195:
	  printf ("LEN");
	  break;
	case 196:
	  printf ("STR$");
	  break;
	case 197:
	  printf ("VAL");
	  break;
	case 198:
	  printf ("ASC");
	  break;
	case 199:
	  printf ("CHR$");
	  break;
	case 200:
	  printf ("LEFT$");
	  break;
	case 201:
	  printf ("RIGHT$");
	  break;
	case 202:
	  printf ("MIDS");
	  break;
	case 0:
	  printf ("\n");
	  break;
	default:
	  printf ("%c", c);
	}
      }
    }
  }
  else
  {
    /* it didnt open */
    printf ("? FILE NOT FOUND  ERROR\n");
  }

  /* All done */
  fs64_closefile_g (&f);
  set_error(0,0,0);

  return (0);
}


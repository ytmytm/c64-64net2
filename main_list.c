/* test programme for 64net/2 for testing bits and pieces */

#include "config.h"
#include "fs.h"
#include "comm-lpt.h"

int no_net=NONET;

int 
main (int argc, char **argv)
{
  int i;
  uchar c;
  fs64_file f;

  talklf = 0;
  if (argc < 2)
  {
    printf ("64list: Usage 64list [nnn[/dir[ ..]:]<64filename>\n");
    return (1);
  }

  /* prepare the 64net file system */
  last_unit = 0;
  partn_dirs[last_unit][1] = (uchar *) malloc (8);
  strcpy (partn_dirs[last_unit][1], "./");
  curr_dir[last_unit][1] = (uchar *) malloc (8);
  strcpy (curr_dir[last_unit][1], "./");
  curr_par[last_unit] = 1;

  /* okay, open file */
  if (!fs64_openfile_g ((uchar*)"./", (uchar*)argv[1], &f))
  {
    /* its open! */

    /* skip load address */
    if (fs64_readchar (&f, &c))
      exit (1);
    if (fs64_readchar (&f, &c))
      exit (1);

    while (1)
    {
      /* skip line link */
      if (fs64_readchar (&f, &c))
	exit (1);
      if (fs64_readchar (&f, &c))
	exit (1);
      if (c == 0)
	return (0);		/* end of basic */

      /* line number */
      if (fs64_readchar (&f, &c))
	exit (1);
      i = c;
      if (fs64_readchar (&f, &c))
	exit (1);
      i += (c * 256);

      printf ("%d ", i);

      /* line text */
      c = 1;
      while (c)
      {
	if (fs64_readchar (&f, &c))
	  exit (1);
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

  return (0);
}

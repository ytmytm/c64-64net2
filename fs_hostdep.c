/*
   Host Filesystem specific routines for 64net/2
   (C)Copyright Paul Gardner-Stephen 1996, All rights reserved
 */

#include "config.h"

/* string used for filename `~xx' char selection */
//static char *fchar = "0123456789abcdefghijklmnopqrstuvwxyz";

int
shortname (uchar *path, uchar *lname,
	   uchar *sname, uchar *ext)
{
  /* derive a shortened name for a c64 file.
     Will give a unique shortened name for a file,
     will also behave differently under messy-dos
   */

  FILE *f = 0;
  unsigned int i;
  uchar temp[1024];
#ifdef MS_DOS
  /* spit! caugh! etc.. */
  unsigned int maxlen = 8;
#else
  /* amiga's only have 32 char files, and it wont hurt to limit
     UNIX to this number */
  unsigned int maxlen = 27;
#endif

  sname[0] = 0;

  /* Step 1: Remove nasties (eg space, comma, dot etc..) */
  for (i = 0; i < strlen (lname); i++)
  {
    /* strip out extra nasties pronto! */
//    if ((lname[i] < 128) && (lname[i] > 31))
//    {
      switch (lname[i])
      {
     //case ' ':
      case ',':
     //case '.':
     //case '+':
      case '/':
      case '\\':
      case '*':
      case '?':
      case ':':
      case ';':
      case '~':
	break;
      default:
	sprintf ((char*)sname, "%s%c", sname, tolower(lname[i]));
	//XXX $40-$5a -> eor $20, $80-$9a eor $40
	break;
      }				/* end switch */
//    }				/* end if */
  }				/* end for */

  /* step 2: Is the name too long? */
  if (strlen (sname) > maxlen)
  {
    /* snip! */
    sname[maxlen] = 0;
  }

  if (!strlen (sname))
  {
    /* make one up! */
    strcpy ((char*)sname, "unnamed");
  }

//  for (i = 0; i < (36 * 36); i++)
//  {
    /* try to open it */
	sprintf ((char*)temp, "%s%s%s", path, sname, ext);
	errno = 0;
	f = fopen ((char*)temp, "r");
	if(errno==ENOENT) {
		/* bingo! 
		   Either the file doesnt exist, or the path is bad.
		   I prefer the first idea :) */
		return (0);
	}
	else {
		/* doh.. try another */
		if (f) {
			fclose (f);
			f = 0;
		}
		return (-1);
	}
//}

//    /* pick next filename to try */
//    switch (i)
//    {
//    case 0:
//      /* try with twiddle number at end */
//      printf ("Case 0\n");
//      sname[maxlen - 3] = 0;
//      break;
//#ifndef AMIGA
//      strcat ((char*)sname, "~00");
//#else
//      strcat ((char*)sname, ".00");
//#endif
//      break;
//    default:
//      sname[strlen (sname) - 3] = 0;
//#ifndef AMIGA
//      sprintf ((char*)sname, "%s~%c%c", sname, fchar[(i / 36)], fchar[i % 36]);
//#else
//      sprintf (sname, "%s.%c%c", sname, fchar[(i / 36)], fchar[i % 36]);
//#endif
//      break;
//    }
//
//  }				/* end for */
//
//  /* none found... barf! */
//  return (-1);

}

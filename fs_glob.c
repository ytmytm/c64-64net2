/* 
   64net/2 Module - globbing routines
   (C)opyright Paul Gardner-Stephen 1995, All Rights Reserved
 */

#include "config.h"
#include "fs.h"

/* structure for glob match list generation */
typedef struct
{
  int gap;
  int c;
  int star;
}
glob_record;

int
glob_comp (uchar *pattern, uchar *test, int filetype)
{
  /* do a glob comparison of the 16 byte string `test' to the pattern
     pattern is a string like "*cheese*,foo?blah*a,S,P,U" */

  int i;
  uchar glob_array[17][32];

  if (parse_glob (glob_array, pattern))
  {
    /* bad glob list */
    return (0);			/* fail coz of bad input */
  }

  /* check file type */
  for (i = 0; i < 8; i++)
    if ((glob_array[0][i]) && (i == (filetype & 0x0f)))
      break;
  if (i == 8)
  {
    /* no match */
    return (0);
  }

  /* check all match strings */
  for (i = 1; i < 17; i++)
  {
    if (strlen (glob_array[i]))
      if (glob_match (glob_array[i], test))
	return (1);
  }

  /* return no match for now */
  return (0);
}

int
glob_p_comp (uchar glob_array[17][32], uchar *pattern)
{
  /* glob the file (ignoring filetype obviously!) */
  int i;


  /* glob each individual match line */
  for (i = 1; i < 17; i++)
  {
    if (strlen (glob_array[i]))
    {
      /* okay, this match line is non-trivial */
      if (glob_match (glob_array[i], pattern))
	/* match */
	return (1);
    }
  }

  /* no match */
  return (0);
}

int
glob_match (uchar *glob, uchar *pattern)
{
  /* glob compare a single string (with 0xa0 padding in pattern) */
  glob_record globs[32];
  int glob_count = 0;
  int i, poss;

  globs[0].gap = 0;
  globs[0].c = 0xa0;
  globs[0].star = 0;

  for (i = 0; i < strlen (glob); i++)
  {
    switch (glob[i])
    {
    case '?':
      /* a single char is required */
      globs[glob_count].gap++;
      break;
    case '*':
      /* any number of chars is required */
      globs[glob_count].star = 1;
      break;
    default:
      /* okay, a char to match *happy happy* */
      globs[glob_count++].c = glob[i];
      /* prepare next glob */
      globs[glob_count].gap = 0;
      globs[glob_count].c = 0xa0;
      globs[glob_count].star = 0;
      break;
    }
  }

  /* patch up last glob */
  if (globs[glob_count].gap | globs[glob_count].star)
  {
    /* last glob counts */
  }
  else
  {
    /* last glob doesnt count */
    glob_count--;
  }

  /* now certify if the glob terms exist in the pattern string */
  poss = 0;
  for (i = 0; i <= glob_count; i++)
  {
    /* add for gap */
    poss += globs[i].gap;

    /* now, what if there was a star, we can skip some chars */
    if (globs[i].star)
    {
      /* star */
      while ((poss < 16) && (pattern[poss] != globs[i].c))
	poss++;
    }

    /* check we have the char,
       or if poss==16 & .star &.c==0xa0 (openended *) */

    if ((pattern[poss] == globs[i].c) ||
	((poss == 16) && (globs[i].star) && (globs[i].c == 0xa0)))
      if (glob_count == i)
	return (1);		/* ok */
      else
      {;
      }				/* do nothing */
    else
      return (0);		/* no match on this char */

    /* check we havent run out of filename */
    if (poss > 16)
      /* no match, coz we need chars past the maximum file length */
      return (0);

    poss++;
  }

  /* ok, weve used up all of our globs */
  if (pattern[poss] != 0xa0)
  {
    /* but we have some filename left! */
    return (0);
  }

  /* match! */
  return (1);
}


int
parse_glob (uchar glob_array[17][32], uchar *pattern)
{
  /* BUGS: This procedure cant differentiate between a search glob and an
     open glob - this means at the moment you could quite happily load a 
     REL or USR file! */

  /* parse a glob string into a form ready for happyness */
  int i, flag = 1, snum, sposs, dir_search = 0;
  uchar fnord[1024];

  /* work on a copy so we dont muck it up */
  strcpy (fnord, pattern);


  /* clear glob_array */
  for (i = 0; i < 16; i++)
  {
    glob_array[0][i] = 0;
    glob_array[i + 1][0] = 0;
  }

  /* check right hand side for special delimiters
     ,P - match PRG   ,S - match SEQ
     ,U - match USR   ,L - match REL (for read/open)
     ,D - match DIR   ,C - match CBM
     ,UFS - match UFS ,$ - dir search, not OPEN
     ,ALL - All files ,- - match none
     ,* - All         ,N - match NET
     ,L, - REL for write(create)

     AND - Remove and ignore ,R & ,W

   */

  while (flag)
  {
    flag = 0;
    i = strlen (fnord) - 1;
    /* this may be bad.... */
    /* REL files can be openned two ways, 
       1) Specifying the record size (for create only) */
    if ((fnord[i - 3] == ',') && (fnord[i - 2] == 'L') && (fnord[i - 1] == ','))
    {
      glob_array[0][cbm_REL] = 1;
      glob_array[0][30] = 1;	/* REL record length is valid */
      glob_array[0][31] = fnord[i];	/* rel record size */
      fnord[strlen (fnord) - 4] = 0;
      flag = 1;
    }
    /* or 
       2) For read mode, with no record size */
    if ((fnord[i - 1] == ',') && (fnord[i - 2] == 'L'))
    {
      glob_array[0][cbm_REL] = 1;
      glob_array[0][30] = 0;	/* rel record length is invalid */
      glob_array[0][31] = 0;	/* rel record size (just to make sure) */
      fnord[strlen (fnord) - 2] = 0;
      flag = 1;
    }
    if ((fnord[i - 1] == ',') && (fnord[i] == 'W'))
    {
      fnord[strlen (fnord) - 2] = 0;
      flag = 1;
    }
    if ((fnord[i - 1] == ',') && (fnord[i] == 'R'))
    {
      fnord[strlen (fnord) - 2] = 0;
      flag = 1;
    }
    if ((fnord[i - 1] == ',') && (fnord[i] == '$'))
    {
      dir_search = 1;
      fnord[strlen (fnord) - 2] = 0;
      flag = 1;
    }
    if ((fnord[i - 1] == ',') && (fnord[i] == '*'))
    {
      glob_array[0][cbm_SEQ] = 1;
      glob_array[0][cbm_PRG] = 1;
      glob_array[0][cbm_USR] = 1;
      glob_array[0][cbm_REL] = 1;
      glob_array[0][cbm_CBM] = 1;
      glob_array[0][cbm_DIR] = 1;
      glob_array[0][cbm_UFS] = 1;
      glob_array[0][cbm_NET] = 1;
      fnord[strlen (fnord) - 2] = 0;
      flag = 1;
    }
    if ((fnord[i - 1] == ',') && (fnord[i] == 'P'))
    {
      glob_array[0][cbm_PRG] = 1;
      fnord[strlen (fnord) - 2] = 0;
      flag = 1;
    }
    if ((fnord[i - 1] == ',') && (fnord[i] == 'S'))
    {
      glob_array[0][cbm_SEQ] = 1;
      fnord[strlen (fnord) - 2] = 0;
      flag = 1;
    }
    if ((fnord[i - 1] == ',') && (fnord[i] == 'U'))
    {
      glob_array[0][cbm_USR] = 1;
      fnord[strlen (fnord) - 2] = 0;
      flag = 1;
    }
    if ((fnord[i - 1] == ',') && (fnord[i] == 'C'))
    {
      glob_array[0][cbm_CBM] = 1;
      fnord[strlen (fnord) - 2] = 0;
      flag = 1;
    }
    if ((fnord[i - 1] == ',') && (fnord[i] == 'D'))
    {
      glob_array[0][cbm_DIR] = 1;
      fnord[strlen (fnord) - 2] = 0;
      flag = 1;
    }
    if ((fnord[i - 1] == ',') && (fnord[i] == 'N'))
    {
      glob_array[0][cbm_NET] = 1;
      fnord[strlen (fnord) - 2] = 0;
      flag = 1;
    }
    if ((fnord[i - 3] == ',') && (fnord[i - 2] == 'U') && (fnord[i - 1] == 'F') && (fnord[i] == 'S'))
    {
      glob_array[0][cbm_UFS] = 1;
      fnord[strlen (fnord) - 4] = 0;
      flag = 1;
    }
    if ((fnord[i - 3] == ',') && (fnord[i - 2] == 'A') && (fnord[i - 1] == 'L') && (fnord[i] == 'L'))
    {
      /* everything but DEL files */
      glob_array[0][cbm_SEQ] = 1;
      glob_array[0][cbm_PRG] = 1;
      glob_array[0][cbm_USR] = 1;
      glob_array[0][cbm_REL] = 1;
      glob_array[0][cbm_CBM] = 1;
      glob_array[0][cbm_DIR] = 1;
      glob_array[0][cbm_UFS] = 1;
      glob_array[0][cbm_NET] = 1;
      fnord[strlen (fnord) - 4] = 0;
      flag = 1;
    }

    if ((fnord[i - 1] == ',') && (fnord[i] == '-'))
    {
      /* nothing */
      glob_array[0][cbm_SEQ] = 0;
      glob_array[0][cbm_PRG] = 0;
      glob_array[0][cbm_USR] = 0;
      glob_array[0][cbm_REL] = 0;
      glob_array[0][cbm_CBM] = 0;
      glob_array[0][cbm_DIR] = 0;
      glob_array[0][cbm_UFS] = 0;
      glob_array[0][cbm_NET] = 0;
      fnord[strlen (fnord) - 2] = 0;
      flag = 1;
    }
  }

  for (i = 0; i < 8; i++)
    flag += glob_array[0][i];
  if (flag == 0)
  {
    if (dir_search)
    {
      /* no UFS, but all else */
      glob_array[0][cbm_SEQ] = 1;
      glob_array[0][cbm_PRG] = 1;
      glob_array[0][cbm_USR] = 1;
      glob_array[0][cbm_REL] = 1;
      glob_array[0][cbm_CBM] = 1;
      glob_array[0][cbm_DIR] = 1;
      glob_array[0][cbm_NET] = 1;
    }
    else
      /* only PRG files matched by default for open */
      glob_array[0][cbm_PRG] = 1;

  }


  /* seperate seperate "globables" */
  /* This bit is just as easy - all we do is look for commas */
  snum = 1;
  sposs = 0;
  i = 0;
  while (i < strlen (fnord))
  {
    if (sposs > 31)
    {
      /* yick! a globable which is over-globulated and wont fit into its glob-hole */
      return (-2);
    }

    if (fnord[i] == ',')
    {
      /* next globable! */
      glob_array[snum][sposs] = 0;
      snum++;
      sposs = 0;
      i++;
    }
    else
    {
      /* add to string */
      glob_array[snum][sposs++] = fnord[i++];
    }
  }
  /* end off the last string */
  if (sposs == 0)
  {
    /* null, ergo add a * */
    glob_array[snum][sposs++] = '*';
  }
  glob_array[snum][sposs] = 0;

  /* Well by George, we've done it! */
  return (0);

}

/*
   Resource Manager module for 64net/2
   (C)Copyright Paul Gardner-Stephen 1996

   This module is responsible for keeping track of files and other resources
   which are in use.  It may be possible to extend this later to ensure that
   the same file is not openned for write multiple times etc.  Its primary use 
   at present is to ensure that 64net/2 does not become a memory seive.

   This module is unlikely to remain in releases of 64net/2, but instead for
   debugging purposes.

 */

#include "config.h"

#ifdef DEBUG_RES

/* fopen / fclose monitoring */

#undef fopen
#undef fclose

int fopenlist[1000] = {0};
int fopencount = 0;
char fopennames[1000][80] = { {0} };

int show_resource (int i);

FILE *
res_fopen (uchar *fn, uchar *mode)
{
  FILE *ff = 0;
  printf ("`%s' opened (mode `%s')::", fn, mode);
  ff = fopen (fn, mode);
  if ((int) ff)
  {
    printf ("fopen returned $%x::", (int) ff);
    strcpy (fopennames[fopencount], fn);
    fopenlist[fopencount++] = (int) ff;
    printf ("%d open files\n", fopencount);
  }
  return (ff);
}

int 
res_fclose (FILE * f)
{
  int i, j;

  printf ("fclosing $%x::", (int) f);
  for (i = 0; i < fopencount; i++)
    if (fopenlist[i] == (int) f)
    {
      printf ("closing: [%s] ($%x)::", fopennames[i], (int) f);
      for (j = i; j < fopencount; j++)
	fopenlist[j] = fopenlist[j + 1];
      fopencount--;
      break;
    }
  printf ("%d open files\n", fopencount);
  return (fclose (f));
}


#undef opendir
#undef closedir

int opendirlist[1000] = {0};
int opendircount = 0;

DIR *
res_opendir (uchar *fn)
{
  DIR *df;
  printf ("`%s' opened::", fn);
  df = opendir (fn);
  if ((int) df)
  {
    printf ("opendir returned $%x::", (int) df);
    opendirlist[opendircount++] = (int) df;
    printf ("%d open dirs\n", opendircount);
  }
  return (df);
}

int 
res_closedir (DIR * d)
{
  int i, j;

  printf ("closdiring $%x::", (int) d);
  for (i = 0; i < opendircount; i++)
    if (opendirlist[i] == (int) d)
    {
      for (j = i; j < opendircount; j++)
	opendirlist[j] = opendirlist[j + 1];
      opendircount--;
    }
  printf ("%d open dirs\n", opendircount);
  closedir (d);
  d = 0;
  return (0);
}


/* below is not currently used */

char reslist[MAX_RES_COUNT][RES_DESC_LEN];
int restypes[MAX_RES_COUNT] =
{RES_none};
int rescount = 0;

int 
register_resource (int rt, uchar *source, uchar *desc)
{
  /* register a resource from some function (source), with a *short*
     description (desc), of resource type rt.  Returns a resource handle.
     This handle is then later used to free the resource.
   */

  int i;

  for (i = 0; i < MAX_RES_COUNT; i++)
    if (restypes[i] == RES_none)
    {
      /* free slot! */
      sprintf (reslist[i], "%s: %s", source, desc);
      restypes[i] = rt;
      printf ("Resource added: ");
      show_resource (i);
      return (i);
    }

  printf ("64net/2: Resource manager full (bad tm)\n");
  return (-1);

}

int 
show_resource (int rn)
{

  char temp[16];


  if ((rn >= 0) && (rn < MAX_RES_COUNT))
  {
    switch (restypes[rn])
    {
    case RES_file:
      strcpy (temp, "file");
      break;
    case RES_dir:
      strcpy (temp, "dir");
      break;
    default:
      strcpy (temp, "unknown");
      break;
    }

    printf ("%s (%s)\n", reslist[rn], temp);
    return (0);
  }

  return (-1);
}

#endif /* DEBUG_RES */

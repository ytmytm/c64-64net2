/*
   Client communications module for 64net/2
   (C) Copyright Paul Gardner-Stephen 1996, All rights reserved 
 */

#include "config.h"

static FILE *f = 0;
static uchar fname[MAX_FS_LEN] =
{0};

int 
client_init (uchar *temp)
{
  if ((f = fopen (temp, "w")) == NULL)
  {
    printf ("INIT: Cannot open file for Virtual LED states\n");
    printf ("         (Client will not show correct led states)\n");
  }
  else
  {
    errno = 0;
    fseek (f, 0, SEEK_SET);
    fprintf (f, "%c%c", 0, 0);
    fflush (f);
    printf ("INIT: Virtual LED states available for client(s) in file\n");
    printf ("         [%s]\n", temp);
    strcpy (fname, temp);
  }

  return (0);
}

int 
client_error (int foo)
{
  if (fname[0])
  {
    if ((f = fopen (fname, "r+")) != NULL)
    {
      errno = 0;
      fseek (f, 0, SEEK_SET);
      fprintf (f, "%c", foo);
      fflush (f);
      fclose (f);
    }
  }
  return (0);
}

int 
client_activity (int foo)
{
  if (fname[0])
  {
    if ((f = fopen (fname, "r+")) != NULL)
    {
      errno = 0;
      fseek (f, 1, SEEK_SET);
      fprintf (f, "%c", foo);
      fflush (f);
      fclose (f);
    }
  }

  return (0);
}

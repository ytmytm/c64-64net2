/* test programme for 64net/2 for testing bits and pieces */

#include <stdio.h>
#include "fs.h"

int no_net=1;

int 
main (int argc, char **argv)
{
  unsigned char c;
  fs64_file f;

  if (argc < 2)
  {
    printf ("64cat: Usage 64cat [nnn[/dir[ ..]:]<64filename>\n");
    return (1);
  }

  /* prepare the 64net file system */
  last_unit = 0;
  partn_dirs[last_unit][1] = (char *) malloc (8);
  strcpy (partn_dirs[last_unit][1], "./");
  curr_dir[last_unit][1] = (char *) malloc (8);
  strcpy (curr_dir[last_unit][1], "./");
  curr_par[last_unit] = 1;

  /* okay, open file */
  if (!fs64_openfile_g ("./", argv[1], &f))
  {
    /* its open! */

    while (!(fs64_readchar (&f, &c)))
    {
      printf ("%c", c);
    }
    return (0);
  }
  else
  {
    /* it didnt open */
    printf ("? FILE NOT FOUND  ERROR\n");
    return (1);
  }

}

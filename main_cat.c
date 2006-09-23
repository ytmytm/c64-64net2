/* test programme for 64net/2 for testing bits and pieces */

#include "config.h"
#include "fs.h"
#include "comm-rrnet.h"

int no_net=NONET;

int 
main (int argc, char **argv)
{
  uchar c;
  fs64_file f;

  talklf[curr_client] = 0;

  if (argc < 2)
  {
    printf ("64cat: Usage 64cat [nnn[/dir[ ..]:]<64filename>\n");
    return (1);
  }

  /* prepare the 64net file system */
  curr_client = 0;
  partn_dirs[curr_client][1] = (uchar *) malloc (8);
  strcpy ((char*)partn_dirs[curr_client][1], "./");
  curr_dir[curr_client][1] = (uchar *) malloc (8);
  strcpy ((char*)curr_dir[curr_client][1], "./");
  curr_par[curr_client] = 1;

  /* okay, open file */
  if (!fs64_openfile_g ((uchar*)"./", (uchar*)argv[1], &f))
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

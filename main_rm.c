/* test programme for 64net/2 for testing bits and pieces */

#include "config.h"
#include "fs.h"

int no_net=NONET;

int 
main (int argc, char **argv)
{

  if (argc < 2)
  {
    printf ("64rm: Usage 64rm [nnn[/dir[ ..]:]<64filename>\n");
    return (1);
  }

  /* okay, scratch file */
  if (!fs64_scratchfile_g ((uchar*)argv[1]))
  {
    printf ("All went well\n");
  }
  else
  {
    printf ("Errors were encountered.\n");
  }
  return (0);
}

/* test programme for 64net/2 for testing bits and pieces */

#include <stdio.h>
#include "fs.h"

int 
main ()
{
  /* test open and read of files and directories and things */
  char fnord[1024];
  int i;
  unsigned char c;
  fs64_file f;

  fgets (fnord, 256, stdin);

  /* okay, open file */
  if (!fs64_openfile_g (".", fnord, &f))
  {
    /* its open! */
    printf ("It Openeth!\n");
    /* read a block */
    for (i = 0; i < 16; i++)
    {
      fs64_readchar (&f, &c);
      printf ("%02x\n", c);
    }
  }
  else
  {
    /* it didnt open */
    printf ("It openeth not\n");
  }

  return (0);
}

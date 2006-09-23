/*  64net UNIX version 64 filesystem "ls" command for the shell 
   (C)Copyright Paul Gardner-Stephen 1995, 1996
   All rights reserved. This copyright notice applies to all portions of the
   64NET system, whether for unix, xx-DOS, or otherwise
 */

#include "config.h"
#include "fs.h"
#include "comm-rrnet.h"

int no_net=NONET;

int 
main (int argc, char **argv)
{
  int blub, bf;
  int i, qf = 0;
  int /* dt=-1, */ ds = -1;
  uchar path[1024], glob[256];

  fs64_direntry de;
  de.dir = 0;
  talklf[curr_client] = 0;

  if (argc > 1)
    strcpy ((char*)path, argv[1]);
  else
    strcpy ((char*)path, ".");

  if (argc > 2)
  {
    strcpy ((char*)glob, argv[2]);
    strcat ((char*)glob, ",$");
  }
  else
    strcpy ((char*)glob, "*,$");

  /* ensure path ends in '/' */
  if (fs64_mediatype (path) == media_UFS)
    if (path[strlen (path) - 1] != '/')
      strcat ((char*)path, "/");

  blub = fs64_findfirst_g (path, glob, &de, &ds, &ds);

  /* get # of free blocks */
  bf = fs64_blocksfree (&de.filesys);

  while (!blub)
  {
    if (de.filetype)
    {
      printf ("%5ld \"", de.blocks);
      qf = 0;
      for (i = 0; i < 16; i++)
	if (de.fs64name[i] != 0xa0)
	  printf ("%c", de.fs64name[i]);
	else
	{
	  /* do a quote and print the rest of the chars */
	  printf ("\"");
	  qf = 1;
	  for (; i < 16; i++)
	    if (de.fs64name[i] != 0xa0)
	      printf ("%c", de.fs64name[i]);
	    else
	      printf (" ");
	}
      if (qf == 0)
	printf ("\"");
      qf = 0;
      for (i = strlen (de.fs64name); i < 16; i++)
	printf (" ");
      if (de.filetype < 128)
	printf ("*");
      else
	printf (" ");
      switch (de.filetype & 0x0f)
      {
      case cbm_DEL:
	printf ("DEL");
	break;
      case cbm_SEQ:
	printf ("SEQ");
	break;
      case cbm_PRG:
	printf ("PRG");
	break;
      case cbm_USR:
	printf ("USR");
	break;
      case cbm_REL:
	printf ("REL");
	break;
      case cbm_CBM:
	printf ("CBM");
	break;
      case cbm_DIR:
	printf ("DIR");
	break;
      case cbm_UFS:
	printf ("UFS");
	break;
      }
      if (de.filetype & 0x40)
	printf ("<\n");
      else
	printf ("\n");
    }
    blub = fs64_findnext_g (&de);
  }
  /* blocks free */
  printf ("%d Blocks free.\n", bf);
  return (0);
}

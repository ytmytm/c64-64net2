/*
   64NET/2 Version module (C)Copyright Paul Gardner-Stephen 1997

   This module contains code for getting the current version of 64NET/2
 */

#define VER_MAJ 0
#define VER_MIN 5

#include <stdio.h>

int 
server_hversion (void)
{
  /* return major version */
  return (VER_MAJ);
}

int 
server_lversion (void)
{
  /* return minor version */
  return (VER_MIN);
}

char *
server_version (void)
{
  static char servvers[80];

  sprintf (servvers, "V%02d.%02d ALPHA",
	   server_hversion (), server_lversion ());
  return (servvers);
}

/*
   64NET/2 Version module (C)Copyright Paul Gardner-Stephen 1997

   This module contains code for getting the current version of 64NET/2
 */

#include "config.h"

char *
server_version (void)
{
  static char servvers[80];

  sprintf (servvers, VERSIONSTRING, VER_MAJ, VER_MIN);
  return (servvers);
}

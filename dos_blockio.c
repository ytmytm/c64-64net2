/*
   64NET/2 block io related dos commands
   (C)Copyright Paul Gardner-Stephen 1996, All rights reserved
 */

#include "config.h"
#include "fs.h"
#include "dosemu.h"

int
dos_u1 (uchar *doscomm, int commlen, int lu)
{
  /* sector-read */
  int sa, pn, t, s;

  /* get all parameters */
  if (parseu1 (doscomm, commlen, &sa, &pn, &t, &s))
    return (-1);

  /* get channel */
  if (!logical_files[lu][sa].open)
  {
    /* no channel */
    set_error (70, 0, 0);
    return (-1);
  }

  /* ensure file is open for buffer io */
  if (!logical_files[lu][sa].isbuff)
  {
    set_error (70, 0, 0);
    return (-1);
  }

  /* read sector */
  if (readts (&logical_files[lu][sa].filesys, t, s, logical_files[lu][sa].buffer))
    return (-1);

  {
    int i, j;
    for (i = 0; i < 16; i++)
    {
      for (j = 0; j < 16; j++)
	debug_msg ("%02x ", logical_files[lu][sa].buffer[i * 16 + j]);
      debug_msg ("\n");
    }
  }

  /* set bp */
  logical_files[lu][sa].bp = 0;

  set_error (00, 00, 00);
  return (0);
}

int
parseu1 (uchar *doscomm, int commlen, int *a, int *b, int *c, int *d)
{
  /* sector-read */
  int sa = 0, pn = 0, t = 0, s = 0;
  int j;

  /* spaces between u1 and sa */
  for (j = 2; j < commlen; j++)
    switch (doscomm[j])
    {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      goto dcu1_1;
      break;
    case ' ':
    case ':':
      /* ignore */
      break;
    default:
      set_error (30, 0, 0);
      return (-1);
    }
dcu1_1:
  /* digits for sa */
  for (; j < commlen; j++)
    switch (doscomm[j])
    {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      sa *= 10;
      sa += doscomm[j] - '0';
      break;
    case ' ':
      /* break out */
      goto dcu1_2;
      break;
    default:
      set_error (30, 0, 0);
      return (-1);
    }
dcu1_2:
  /* spaces between sa and pn */
  for (; j < commlen; j++)
    switch (doscomm[j])
    {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      goto dcu1_3;
      break;
    case ' ':
      /* ignore */
      break;
    default:
      set_error (30, 0, 0);
      return (-1);
    }
dcu1_3:
  /* digits for pn */
  for (; j < commlen; j++)
    switch (doscomm[j])
    {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      pn *= 10;
      pn += doscomm[j] - '0';
      break;
    case ' ':
      /* break out */
      goto dcu1_4;
      break;
    default:
      set_error (30, 0, 0);
      return (-1);
    }
dcu1_4:
  /* spaces between pn and t */
  for (; j < commlen; j++)
    switch (doscomm[j])
    {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      goto dcu1_5;
      break;
    case ' ':
      /* ignore */
      break;
    default:
      set_error (30, 0, 0);
      return (-1);
    }
dcu1_5:
  /* digits for t */
  for (; j < commlen; j++)
    switch (doscomm[j])
    {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      t *= 10;
      t += doscomm[j] - '0';
      break;
    case ' ':
      /* break out */
      goto dcu1_6;
      break;
    default:
      set_error (30, 0, 0);
      return (-1);
    }
dcu1_6:
  /* spaces between t and s */
  for (; j < commlen; j++)
    switch (doscomm[j])
    {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      goto dcu1_7;
      break;
    case ' ':
      /* ignore */
      break;
    default:
      set_error (30, 0, 0);
      return (-1);
    }
dcu1_7:
  /* digits for s */
  for (; j < commlen; j++)
    switch (doscomm[j])
    {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      s *= 10;
      s += doscomm[j] - '0';
      break;
    case ' ':
      /* break out */
      goto dcu1_8;
      break;
    default:
      set_error (30, 0, 0);
      return (-1);
    }
dcu1_8:
  debug_msg ("SA: %d PN: %d T%d S%d\n", sa, pn, t, s);

  *a = sa;
  *b = pn;
  *c = t;
  *d = s;
  return (0);
}

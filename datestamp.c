/*
   64NET/2 Date stamp module.
   (C) Copyright Paul Gardner-Stephen 1996

 */

#include "config.h"
#include <time.h>
#ifndef AMIGA
#include <sys/timeb.h>
#endif


static struct tm *t;
static time_t foo;

int
gettimestamp (int *year, int *month, int *day, int *hour, int *minute, int *second)
{
  time (&foo);
  t = localtime (&foo);

  *year = t->tm_year;
  *month = t->tm_mon + 1;
  *day = t->tm_mday;
  *hour = t->tm_hour;
  *minute = t->tm_min;
  *second = t->tm_sec;

  return (0);
}

int
gettimer (int *second, int *ms)
{
#ifdef HAS_FTIME
    struct timeb tb;
    ftime(&tb);
    *ms=tb.millitm;
#else
/* does it work on Amiga? */
  unsigned int clock[2];

  timer (clock);
  *ms = clock[1] * 1000;
#endif
  /* KLUGE: same arg is okay, as second gets assigned last */
  gettimestamp (second, second, second, second, second, second);

  return (0);
}

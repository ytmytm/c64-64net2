/*
   64NET/2 Date stamp module.
   (C) Copyright Paul Gardner-Stephen 1996

 */

#include "config.h"
#include <time.h>
#include <sys/time.h>
#ifdef BSD
#include <sys/types.h>
#endif
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

void gettimer (int *second, int *ms)
{
    struct timeval tv;

    gettimeofday(&tv,NULL);
    *second = tv.tv_sec;
    *ms  = tv.tv_usec;
}

void current_time (int *timebuf[]) {

	time(&foo);
	t = localtime(&foo);

	*timebuf[0] = t->tm_year / 100 + 19;	/* century 19-99 */
	*timebuf[1] = t->tm_year % 100 ;	/* year    00-99 */
	*timebuf[2] = t->tm_mon + 1;		/* month   00-12 */
	*timebuf[3] = t->tm_mday;		/* day     01-31 */
	*timebuf[4] = t->tm_hour;		/* hour    00-23 */
	*timebuf[5] = t->tm_min;		/* minute  00-59 */
	*timebuf[6] = t->tm_sec;		/* second  00-59 */
	*timebuf[7] = t->tm_wday;		/* weekday (Mon=1,...Sun=7) */

	if (t->tm_wday==0) *timebuf[7]=7;

}

char *current_time_string(void) {

	time(&foo);

	return (ctime(&foo));

}

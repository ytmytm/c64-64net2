/* 

 */
#include <sys/types.h>
#include <time.h>
#include <stdio.h>

struct tm *t;
time_t foo;

int
main ()
{
  time (&foo);
  t = localtime (&foo);
  printf ("%d:%d.%d\n", t->tm_hour, t->tm_min, t->tm_sec);
  return 0;
}

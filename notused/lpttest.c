#include "cpufunc.h"
#include <stdio.h>

FILE *f = 0;

int 
main ()
{
  if ((f = fopen ("/dev/io", "r")) == NULL)
  {
    printf ("could not gain chip-bash privelige\n");
    exit (2);
  }

/*
   outb(0x3be,0xa0);
   outb(0x3bc,0xaa);
   if (inb(0x3bc)==0xaa)
   printf("Gark! still mono-directional\n");
   else
   printf("Rapture!\n");
 */
  outb (0x3be, 0x41);
  outb (0x3be, 0x40);

  printf ("%02x %02x %02x\n", inb (0x3bc), inb (0x3bd), inb (0x3be));

  fclose (f);
  f = 0;
  return (0);
}

#include "linux.h"

unsigned char inb(int a) 
{
  unsigned char i; 
  __asm__ __volatile__ ("mov a,%dx\nin %dx,%al\nmov %al,i\n");
  return(i);
}



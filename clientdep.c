/*
   64NET/2 Client dependent routines
   (eg Burst Load engage and dis-engage sequences
   (C)Copyright Paul Gardner-Stephen 1996, All rights reserved
 */

#include <stdio.h>

#include "machdep.h"
#include "comm-lpt.h"
#include "clientdep.h"
#include "debug.h"

int 
client_turbo_speed (void)
{
  sendchar (0xf7);

  switch (charget ())
  {
  case MACH_C64:		/* c64/c128 in c64 mode */
    debug_msg ("C128 in C64 mode\n");
    /* it pokes & jsr's coz io is banked out (doh!) */
    c64poke (0x7e8, 0x8d);
    c64poke (0x7e9, 0x30);
    c64poke (0x7ea, 0xd0);
    c64poke (0x7eb, 0x60);
    c64jsr (0x7e8, 1, 0, 0);
    return (0);
    break;
  case MACH_C65:		/* c65 in c64 mode */
    debug_msg ("C65 in C64 mode\n");
    /* it pokes & jsr's coz io is banked out (doh!) */
    c64poke (0x7e8, 0x8d);	/* sta $d02f  */
    c64poke (0x7e9, 0x2f);
    c64poke (0x7ea, 0xd0);
    c64poke (0x7eb, 0x8e);	/* stx $d02f */
    c64poke (0x7ec, 0x2f);
    c64poke (0x7ec, 0xd0);
    c64poke (0x7ed, 0x8c);	/* sty $d031 */
    c64poke (0x7ee, 0x31);
    c64poke (0x7ef, 0xd0);
    c64poke (0x7f0, 0x60);
    c64jsr (0x7e8, 0xa5, 0x96, 0x60);
    break;
  }

  return (-1);
}

int 
client_normal_speed (void)
{
  sendchar (0xf7);

  switch (charget ())
  {
  case MACH_C64:		/* c64/c128 in c64 mode */
    debug_msg ("C128 in C64 mode\n");
    c64poke (0x7e8, 0x8d);
    c64poke (0x7e9, 0x30);
    c64poke (0x7ea, 0xd0);
    c64poke (0x7eb, 0x60);
    c64jsr (0x7e8, 0, 0, 0);
    return (0);
    break;
  case MACH_C65:		/* c65 in c64 mode */
    debug_msg ("C65 in C64 mode\n");
    /* it pokes & jsr's coz io is banked out (doh!) */
    c64poke (0x7ed, 0x8c);	/* sty $d031 */
    c64poke (0x7ee, 0x31);
    c64poke (0x7ef, 0xd0);
    c64poke (0x7e8, 0x8d);	/* sta $d02f  */
    c64poke (0x7e9, 0x2f);
    c64poke (0x7ea, 0xd0);
    c64jsr (0x7e8, 0, 0, 0);
    return (0);
    break;
  }

  return (-1);
}

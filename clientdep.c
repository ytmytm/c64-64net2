/*
   64NET/2 Client dependent routines
   (eg Burst Load engage and dis-engage sequences
   (C)Copyright Paul Gardner-Stephen 1996, All rights reserved
 */

#include "config.h"
//#include "comm-lpt.h"
#include "clientdep.h"

#define	MACH_C64	0
#define	MACH_C65	1
#define	MACH_C128	2
#define MACH_C128IN64	3
#define MACH_C65IN64	4

const char *clientdep_name[NUM_OSES]= { "C64", "C65", "C128 NATIVE", "C128 IN C64 MODE", "C65 IN C64 MODE" };
const int clientdep_tables[NUM_OSES][3]= { { 0x259, 0x263, 0x26d }, { 0x0, 0x0, 0x0 },
				  { 0x362, 0x36c, 0x376 }, { 0x259, 0x263, 0x26d }, { 0x259, 0x263, 0x26d } };

int 
client_turbo_speed (void)
{
  int type;

  sendchar (0xf7);

  type=charget();
  debug_msg ("%s enter turbo\n", clientdep_name[type]);
  switch (type)
  {
  case MACH_C128IN64:		/* c64/c128 in c64 mode */
    /* it pokes & jsr's coz io is banked out (doh!) */
    c64poke (0x7e8, 0x8d);
    c64poke (0x7e9, 0x30);
    c64poke (0x7ea, 0xd0);
    c64poke (0x7eb, 0x60);
    c64jsr (0x7e8, 1, 0, 0);
    return (0);
    break;
  case MACH_C65IN64:		/* c65 in c64 mode */
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
  int type;

  sendchar (0xf7);

  type=charget();
  debug_msg ("%s exit turbo\n", clientdep_name[type]);

  switch (type)
  {
  case MACH_C128IN64:		/* c64/c128 in c64 mode */
    c64poke (0x7e8, 0x8d);
    c64poke (0x7e9, 0x30);
    c64poke (0x7ea, 0xd0);
    c64poke (0x7eb, 0x60);
    c64jsr (0x7e8, 0, 0, 0);
    return (0);
    break;
  case MACH_C65IN64:		/* c65 in c64 mode */
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

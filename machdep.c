
#include "config.h"
#include "machdep.h"
#include "fs.h"

#ifdef USE_LINUX_KERNEL_MODULE
#include "machdep-lin.c"
#else
#ifdef USE_SERIAL_DRIVER
#include "machdep-ser.c"
#else
#include "machdep-old.c"
#endif
#endif

int
fastsendblock (int addr, int size, uchar * the_block)
{
  /* send a block of data *super* fast */
  /* Algorithm:
     - Send block backwards (easier for 64 to receive!) 
     using much less handshaking due to mono-directionality.
     1) Send command
     2) send address
     3) send 254 bytes of data using minimal hand shaking
   */

#ifdef DEBUG2
  debug_msg ("Inside fastsendblock\n");
  fflush (stdout);
#endif
  sendchar (0xf5);
  sendchar (addr & 0xff);
  sendchar (addr / 256);

#ifdef DEBUG2
  debug_msg ("Outside fastsendblock\n");
  fflush (stdout);
#endif
  return (fishsendblock (254, the_block));

}

int
fastgetblock (int addr, int size, uchar * the_block)
{
  /* send a block of data *super* fast */
  /* Algorithm:
     - Send block backwards (easier for 64 to receive!) 
     using much less handshaking due to mono-directionality.
     1) Send command
     2) send address
     3) send 254 bytes of data using minimal hand shaking
   */

#ifdef DEBUG2
  debug_msg ("Inside fastgetblock\n");
  fflush (stdout);
#endif
  sendchar (0xf4);
  sendchar (addr & 0xff);
  sendchar (addr / 256);

#ifdef DEBUG2
  debug_msg ("Outside fastgetblock\n");
  fflush (stdout);
#endif
  return (fishgetblock (254, the_block));

}

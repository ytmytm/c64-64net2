/*
  IEC serial info
  Pilfered from vc1541.h (C) Torsten Paul 1997

  Changes (C) Paul Gardner-Stephen 1997 
*/

#define IEC_COMMAND  0xf0
#define IEC_DEVICE   0x0f

#define IEC_UNTALK   0x50
#define IEC_UNLISTEN 0x30
#define IEC_LISTEN   0x20
#define IEC_TALK     0x40

#define IEC_MODE     0xf0
#define IEC_ADDRESS  0x0f

#define IEC_OPEN     0xf0
#define IEC_CLOSE    0xe0
#define IEC_DATA     0x60


/* XXX these values are not used presently */
#define IO_TIMEOUT1 20
/* transition time out value,
   does this means Wait will wait upto 1second before aborting? */
#define IO_TIMEOUT2 50000


/* These values doesn't apply for Amiga */
#ifndef AMIGA

/*
   first value defines wait time (ms) for first byte before sleeping,
   second value is number of sleep cycles when waiting for handshake
   before throwing data out
*/
#define synctolerance	5000
#define synctolerance_2	5


/*
   sleeping time - first is between bytes, second is for handshake,
   therefore should be much, much lower, because handshake comes
   immediately (if ever comes)
*/

#define snooz_time	70000
#define snooz_time_2	10000

#endif

#define uchar unsigned char

void init_hw(void);
void starthw(void);

int syncchar(void);
int sendchar(int);
int charget(void);
int fastsendblock(int addr,int size,uchar *the_block);
int fastgetblock(int addr,int size,uchar *the_block);
int fishgetblock (int size, uchar * block);
int fishsendblock (int size, uchar * block);

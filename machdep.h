
#ifdef AMIGA

#define synctolerance	5000
#define synctolerance_2	5

#define snooz_time	3
#define snooz_time_2	3

#else
/*
   first value is number of port checks before going to sleep for (snooz_time)
   waiting for first byte, it will be overriden by timeout from config file
   second value is number of port checks when waiting for data handshake
   before throwing it out
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

void init_hw(void);
void starthw(void);

int syncchar(void);
int sendchar(int);
int charget(void);
int fastsendblock(int addr,int size,uchar *the_block);
int fastgetblock(int addr,int size,uchar *the_block);
int fishgetblock (int size, uchar * block);
int fishsendblock (int size, uchar * block);

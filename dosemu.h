/* 
   Header file for 64NET/2 CBM/CMD dos emulation module
   (C) Copyright Paul Gardner-Stephen 1996, All rights reserved
   */

/* DOS commands */
int dos_u1(char *doscomm,int commlen,int lu);

/* back end routines */
int parseu1(char *doscomm,int commlen,int *a,int *b,int *c,int *d);
int do_dos_command(void);

void init_dos(void);

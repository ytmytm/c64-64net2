/*
   Commune.H for 64NET/2 (C)Copyright Paul Gardner-Stephen 1995, 2004
   All rights reserved.
   
   Commune.H - Constants and structures for communicating to the C64/C65/C128
*/

/* port addresses */
extern int portout,portin;
extern int max_tries;
extern uchar port[1024];

/* function prototypes */
int commune(void);
int read_config (char *file);
int read_device (FILE * cf);
int set_drive_status (uchar *string, int len);
int which_unit (int);

int iec_commune(int a);

extern int listenlf, talklf, lastlf, devnum;
extern int client_type;

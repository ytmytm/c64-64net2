/*
  Header for clientdep.c for 64NET/2
  (C)Copyright Paul Gardner-Stephen 1996, All rights reserved
  */

int client_turbo_speed(void);
int client_normal_speed(void);

/* number of supported client OSes */
#define NUM_OSES	5

extern const char *clientdep_name[NUM_OSES];
extern const int clientdep_tables[NUM_OSES][3];

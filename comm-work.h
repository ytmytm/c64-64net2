
/* prototypes for use only within comm-lpt */
void do_boot(void);
void do_load(void);
void do_save(void);
void do_open(int secaddr);
void do_close(int secaddr);
void do_acptr(int secaddr);
void do_ciout(int secaddr);
void do_command(void);

extern int fnlen;
extern unsigned char filename[256];
extern int dont_open;
extern int allowFishLoad, allowFishSave;

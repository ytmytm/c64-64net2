
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
extern uchar filename[256];
extern int dont_open;
extern int allowFishLoad, allowFishSave;

void do_open_small(void);
void do_close_small(void);
void do_chkinout(int);

void do_load_lowlevel(fs64_file *loadfile, int startaddr);
void do_verify_lowlevel(fs64_file *loadfile, int startaddr);

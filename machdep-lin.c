
static FILE *in_device;
static FILE *out_device;

#define COMM_DEVICE "linux/fastpc"

int direction = 0;	// 0 - read, !=0 - write

void starthw (void) { };

void init_hw (void) {
    debug_msg("LINUX: opening " COMM_DEVICE " device...\n");
    in_device = fopen (COMM_DEVICE,"r");
    out_device = fopen (COMM_DEVICE,"w");
    if ((in_device==0) || (out_device==0)) {
	fatal_error("Error: couldn't open communication device.\nCheck if kernel module is loaded.\n");
    } else { debug_msg ("Success\n"); };
    fflush(stdout);
}

void checkdir(void) {
    if (direction!=0) {
	fflush (out_device);
	direction=0;
    };
}

int charget (void) {
int value=0;
    checkdir();
    fread(&value,1,1,in_device);
    return (value);
}

int syncchar(void) { return charget(); }

int sendchar (int byte) {
    direction = fwrite(&byte,1,1,out_device);
    return (0);
}

int fishgetblock(int size, uchar *block) {
    checkdir();
    fread(block,size,1,in_device);
    return (0);
}

int fishsendblock (int size, uchar *block) {
    direction = fwrite (block,size,1,out_device);
    return (0);
}

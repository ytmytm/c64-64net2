
#include <fcntl.h>
#include <termios.h>
#include "comm-lpt.h"

static int ser_device;
struct termios oldtio;

void starthw (void) { };

void init_hw (void) {
    struct termios newtio;

//    debug_msg("LINUX: opening " COMM_DEVICE " device...\n");
//    ser_device = open(COMM_DEVICE, O_RDWR | O_NOCTTY );
    debug_msg("LINUX: opening %s device...\n", port);
    ser_device = open(port, O_RDWR | O_NOCTTY );

    if (ser_device <0) {perror(port); exit(-1); }

    tcgetattr(ser_device,&oldtio); /* save current port settings */

    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = BAUDRATE | CRTSCTS | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;

    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;
    newtio.c_oflag = 0;

    newtio.c_cc[VTIME]    = 0;   /* inter-character timer unused */
    newtio.c_cc[VMIN]     = 1;   /* blocking read until 1 char received */

    tcflush(ser_device, TCIFLUSH);
    tcsetattr(ser_device,TCSANOW,&newtio);
}

int charget (void) {
int value=0;
    read(ser_device,&value,1);
    return (value);
}

int syncchar(void) { return charget(); }

int sendchar (int byte) {
    write(ser_device,&byte,1);
//    fsync(ser_device);
    return (0);
}

int fishgetblock(int size, uchar *block) {
    read(ser_device,block,size);
    return (0);
}

int fishsendblock (int size, uchar *block) {
    write (ser_device,block,size);
//    fsync(ser_device);
    return (0);
}

/*
    FastPC Linux Kernel Module driver

    Maciej 'YTM/Alliance' Witkowiak

    9.7.99   - first version
    2-4.8.99 - interrupts, buffers, duplex
    15-16.8.99  - only irq/nmi version, no direction-line
    21.4.00 - trashed buffer stuff for 64net/2 - both ends know at each time
	      what is going on, always try to read first

    26.5.00 - trashed scheduling and rest of buffer stuff, introduced irq accounting,
    29.5.00 - code is rewritten to work transparently for (slightly modified) C= wedge,
	      back to skeleton from 21.4
    30.5.00 - as above

TODO:
    + during write cycle (C= waits) C= should be blocked until something is really written
      to the device
    + the same applies to reading (if it doesn't work now)
    - PA2 might be considered as 'direction line' - it is either HIGH or LOW in both irqs
      (leave it, it's more errorproof)
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/wrapper.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/tqueue.h>
#include <linux/interrupt.h>
#include <asm/delay.h>

/* two values below should be defineable in commandline */
#define BASE		0x378
#define IRQ_NM		7
#define	READPORT	0x31
#define WRITEPORT	0x11
#define BUSY		(inb(BASE+1) & 0x80)

#define DIR_UNK		0
#define DIR_READ	1
#define DIR_WRITE	2
#define SUCCESS		0
#define DEVICE_NAME	"fastpc"

/* For sleep/wake queue */
static struct wait_queue *WaitQ_rd=NULL;
static struct wait_queue *WaitQ_wr=NULL;

static unsigned char data=0;		// current I/O data
static unsigned char dir=DIR_UNK;			// current port direction - irq handler needs that
static unsigned char FastPC_Open = 0;		// # of opened files
static int Major;				// major dev. #, later change into defined constant
static unsigned char phase=0;			// read/write phase
static unsigned char got_something=0;		// !=0 when C64 wanted sth before host started talking

static inline void SendStrobe(void) {
unsigned char a;

    a=inb(BASE+2);
    outb(a & 0xfe, BASE+2);
    outb(a, BASE+2);
}

static int FPCInit(void) {
    if (check_region(BASE,3)) {return 1;}
    else
	{ request_region(BASE, 3, DEVICE_NAME);
	  outb(READPORT, BASE+2);
	  return SUCCESS;
    }
}

static void FastPC_irq_read(void *dane) {

    if (phase==0) {
        outb(READPORT,BASE+2);
	data=inb(BASE);
	phase++; }
    else {
	phase--;
	if (WaitQ_rd!=NULL) wake_up(&WaitQ_rd); }
    SendStrobe();
}
 
static void FastPC_irq_write(void *dane) {

    if (phase==0) {
	outb(WRITEPORT,BASE+2);
	outb(data, BASE);
	phase++; }
    else {
	phase--;
	if (WaitQ_wr!=NULL) wake_up(&WaitQ_wr); }
    SendStrobe();
}

static void FastPC_irq_error(void *dane) {
    printk("error - PA2 should be high during read irqs and low during write\n");
    data=0xff;
    SendStrobe();
    if (dir==DIR_READ) {
    if (WaitQ_rd!=NULL) wake_up(&WaitQ_rd); } else {
    if (WaitQ_wr!=NULL) wake_up(&WaitQ_wr); }
}

static void FastPC_irq(int irq_nm, void *dev_id, struct pt_regs *regs) {
static struct tq_struct task_rd={NULL, 0, FastPC_irq_read, NULL};	//reading lower half
static struct tq_struct task_wr={NULL, 0, FastPC_irq_write, NULL};	//writting lower half
static struct tq_struct task_er={NULL, 0, FastPC_irq_error, NULL};	//error lower half
static unsigned char isbusy;

    isbusy = BUSY;

    switch (dir) {
	case DIR_READ:
	    if (!(isbusy)) {				// PA2 is high, for sure it is read
		queue_task(&task_rd,&tq_immediate);
		mark_bh(IMMEDIATE_BH); }
	    else {					// PA2 is low, this shouldn't happen
		queue_task(&task_er,&tq_immediate);
		mark_bh(IMMEDIATE_BH); }
	case DIR_WRITE:
	    if (isbusy) {				// PA2 is low, C= wants data
		queue_task(&task_wr,&tq_immediate);
		mark_bh(IMMEDIATE_BH); }
	    else {					// PA2 is high, throw it out
//		queue_task(&task_er,&tq_immediate);
		queue_task(&task_wr,&tq_immediate);
		mark_bh(IMMEDIATE_BH); }
	case DIR_UNK:
	default:
	    got_something++;
    }
}

static ssize_t FastPC_read(struct file *file, char *buffer, size_t length, loff_t *ppos) {
int bytes_read = 0;
    phase=0;
    dir=DIR_READ;
    if (got_something!=0) {
        outb(READPORT,BASE+2);
	data=inb(BASE);
	phase++;
	put_user (data, buffer++);
	length--;
	bytes_read++;
	got_something=0;
	SendStrobe();
	sleep_on (&WaitQ_rd);
    }
    while (length) {
	sleep_on (&WaitQ_rd);
        put_user (data, buffer++);
        bytes_read++;
        length--;
    }
    dir=DIR_UNK;
#ifdef DEBUG
    printk("Read request(%d)",bytes_read);
#endif
    return bytes_read;
}

static ssize_t FastPC_write(struct file *file, const char *buffer, size_t length, loff_t *ppos) {
int bytes_written=0;
    phase=0;
    dir=DIR_WRITE;
    if (got_something!=0) {
        get_user(data, buffer++);
        bytes_written++;
        length--;
	outb(WRITEPORT,BASE+2);
	outb(data, BASE);
	phase++;
	got_something=0;
	SendStrobe();
        sleep_on(&WaitQ_wr);
    }
    while (length) {
        get_user(data, buffer++);
        sleep_on(&WaitQ_wr);
        bytes_written++;
        length--;
    }
    dir=DIR_UNK;
#ifdef DEBUG
    printk("Write request(%d)",bytes_written);
#endif
    return bytes_written;
}

static int FastPC_open(struct inode *inode, struct file *file) {
#ifdef DEBUG
    printk ("FastPC open(%p,%p)\n", inode, file);
#endif
    if (FastPC_Open>2) return -EBUSY;

    FastPC_Open++;
    MOD_INC_USE_COUNT;
    return SUCCESS;
}

static int FastPC_release(struct inode *inode, struct file *file) {
#ifdef DEBUG
    printk ("FastPC_release(%p,%p)\n", inode, file);
#endif
    FastPC_Open--;
    MOD_DEC_USE_COUNT;
    return SUCCESS;
}

struct file_operations Fops={
    read:		FastPC_read,
    write:		FastPC_write,
    open:		FastPC_open,
    release:		FastPC_release,
};

int init_module() {

    Major = module_register_chrdev(0, DEVICE_NAME, &Fops);
    if (Major < 0) {
	printk ("Sorry, registering failed with %d\n",Major);
	return Major;
    }

    printk("Registration is a success. Its number is %d.\n", Major);
    printk("Trying to allocate IO space: ");
    if (FPCInit()) {printk("Failed\n"); return Major; } else {printk("Success\n");};

    printk("Trying to allocate IRQ: ");
    if (request_irq(IRQ_NM, &FastPC_irq, 0, DEVICE_NAME, NULL))
    { printk("Failed\n"); return Major; } else { printk("Success\n"); };

    dir=DIR_UNK; phase=0; got_something=0;

    return SUCCESS;
}

void cleanup_module() { 
int ret;

    free_irq(IRQ_NM, NULL);
    release_region(BASE, 3);

    if ((ret = module_unregister_chrdev(Major, DEVICE_NAME)) < 0) 
	printk("Error in module_unregister_chrdev: %d\n",ret);

}

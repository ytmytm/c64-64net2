/*
    FastPC Linux Kernel Module driver

    Maciej 'YTM/Alliance' Witkowiak

    9.7.99   - first version
    2-4.8.99 - interrupts, buffers, duplex
    15-16.8.99  - only irq/nmi version, no direction-line
    21.4.00 - trashed buffer stuff for 64net/2 - both ends know at each time
	      what is going on, always try to read first

TODO:	FPCInit/Done may be moved to init/done_module
        reading buffer stuff (incorporate from older versions)
	writting buffer (seems to be unneccessary)

PROTOCOL:
	at start mode is reading
	if something is read/written to the device it is pushed onto lines immediately

	linux  read: flag(data online)->strobe(confirm)
	linux write: strobe(data online)->flag(confirm)
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

/* two values below should be defineable in commandline */
#define BASE 0x378
#define IRQ_NM  7
#define	READPORT	0x31
#define WRITEPORT	0x11

#define DIR_READ	0
#define DIR_WRITE	1
#define SUCCESS 0
#define DEVICE_NAME "fastpc"

/* For sleep/wake queue */
static struct wait_queue *WaitQ_rd=NULL;
static struct wait_queue *WaitQ_wr=NULL;

static int data=0;			// current I/O data
static int dir=DIR_READ;		// current port direction - irq handler needs that

static int RdBuff_Full=0;

inline void SendStrobe(void) {
unsigned char a;

    a=inb(BASE+2);
    outb(a & 0xfe, BASE+2);
    outb(a, BASE+2);
}
  
int FPCInit(void) {
    if (check_region(BASE,3)) {return 1;}
    else
	{ request_region(BASE, 3, DEVICE_NAME);
	  outb(READPORT, BASE+2);
	  return SUCCESS;
    }
}

static void FastPC_irq_read(void *dane) {
    if (WaitQ_rd!=NULL) wake_up(&WaitQ_rd);
}
 
static void FastPC_irq_write(void *dane) {
/* This is just confirmation */
    if (WaitQ_wr!=NULL) wake_up(&WaitQ_wr);
}

static void FastPC_irq(int irq_nm, void *dev_id, struct pt_regs *regs) {
static struct tq_struct task_rd={NULL, 0, FastPC_irq_read, &data};	//reading lower half
static struct tq_struct task_wr={NULL, 0, FastPC_irq_write, &data};	//writting lower half

    if (dir!=DIR_WRITE) {
        RdBuff_Full=1;
	outb(READPORT,BASE+2);
        data=inb(BASE);
        queue_task(&task_rd,&tq_immediate);
        mark_bh(IMMEDIATE_BH);
    }
    else
    {
	queue_task(&task_wr,&tq_immediate);
	mark_bh(IMMEDIATE_BH);
    }
}

static ssize_t FastPC_read(struct file *file, char *buffer, size_t length, loff_t *ppos) {
/* number of bytes actually written */
int bytes_read = 0;

    dir=DIR_READ;
    if (RdBuff_Full!=0) {
	put_user (data, buffer++);
	SendStrobe();
	length--;
	bytes_read++;
    }

    while (length) {
	sleep_on (&WaitQ_rd);
        put_user (data, buffer++);
	SendStrobe();
        length--;
        bytes_read++;
    }
    RdBuff_Full=0;
#ifdef DEBUG
    printk("Read request(%d)",bytes_read);
#endif
    return bytes_read;
}

static ssize_t FastPC_write(struct file *file, const char *buffer, size_t length, loff_t *ppos) {
unsigned char tmp;
int bytes_written=0;

    dir=DIR_WRITE;
    outb(WRITEPORT,BASE+2);
    while (length) {
        get_user(tmp, buffer++);
        outb(tmp, BASE);
        SendStrobe();
        sleep_on(&WaitQ_wr);
        bytes_written++;
        length--;
    }
#ifdef DEBUG
    printk("Write request(%d)",bytes_written);
#endif
    return bytes_written;
}

static int FastPC_Open = 0;

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

static int Major;
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

    return SUCCESS;
}

void cleanup_module() { 
int ret;

    free_irq(IRQ_NM, NULL);
    release_region(BASE, 3);

    if ((ret = module_unregister_chrdev(Major, DEVICE_NAME)) < 0) 
	printk("Error in module_unregister_chrdev: %d\n",ret);

}

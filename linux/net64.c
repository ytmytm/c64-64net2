
/*
    This is kernel driver for 2.4.x kernels (2.2.x probably can be supported by
    reviewing this file with cbmlinux code in hand, this code is based on it)
    Unlike fastpc.o module this one can be interrupted with CTRL+C and will not
    hang your system.
    This one requires parport/parport_pc modules although they don't help much.
    The device is /dev/misc/net64 (with MINOR of cbm)

    net64_read routine is implemented and works ok for _small_ and _separated_
    bytes of data. It chokes on continuous flow of data, most of the irqs are
    missed. Curious thing is that the first 16 bytes seem to be ack'ed properly.
    I don't know what causes this behavior. udelays in send_strobe didn't help.

    Warning! Although CTRL+C works, at least once I managed to hang my PC -
    the cause must be somewhere in irq routine (?)

    Warning! Revise again PA2 state - is it inverted or real on PC side?

    Maciej 'YTM/Elysium' Witkowiak, 01.08.2003
*/

/*
c64->pc (net64_read)

    lda #$97		PA2 high
    sta $dd00
    sta $dd01		irq on PC (wake it)
-   lda $dd0d		wait for strobe (PC will setup DDR)
    and #$10
    beq -
    lda #$ff
    sta $dd03		setup own DDR
    lda #data
    sta $dd01		irq on PC (fetch data)
-   lda $dd0d		wait for strobe (PC will acknowledge)
    and #$10
    beq -
    lda #0
    sta $dd03
    rts

pc->c64
    lda #0
    sta $dd03		DDR as input (default)
    lda #$93		PA2 low
    sta $dd00
    lda $dd01		wake PC
    lda $dd0d		wait for data ready
    and #$10
    beq -
    lda $dd01		will send ack
    lda $dd0d		wait for ack of ack
    and #$10
    beq -
    rts
*/

#include <linux/config.h>
#include <linux/version.h>
#include <linux/module.h>
#include <asm/io.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <linux/parport.h>
#include <linux/parport_pc.h>

static wait_queue_head_t net64_wait;
static spinlock_t net64_lock = SPIN_LOCK_UNLOCKED;
static int port;

static int busy;			/* != 0 - already opened */

static struct pardevice *net64_device;
unsigned int lp = 0;			/* parport device # (future module parameter) */

volatile static int irq_count;
volatile static int direction;
volatile static int phase;		/* devel only */
volatile static unsigned char last_data;
volatile static int got_something;

#define READPORT 0x31
#define WRITEPORT 0x11
#define BUSY (inb(port+1) & 0x80)	/* PA2 state */

#define NAME "net64"
#define NET64_MINOR 177

MODULE_LICENSE("GPL");
#define net64_init init_module
#define net64_cleanup cleanup_module


static inline void net64_send_strobe(void)
{
    int a = inb(port+2);
    outb(a & 0xfe, port+2);
    udelay(20);
    outb(a, port+2);
    udelay(20);
}

static int net64_read (struct file *f, char *buf, size_t cnt, loff_t *ppos)
{
    int received = 0;
#ifdef DECLARE_WAITQUEUE
    DECLARE_WAITQUEUE(wait, current);
#else
    struct wait_queue wait = { current, NULL };
#endif

    printk("read: (%i)\n",cnt);

    if (got_something) {
	put_user((char)last_data,buf++);
	received++;
	cnt--;
	// ack the upper half of the last byte
	got_something = 0;
	net64_send_strobe();
    }

    add_wait_queue(&net64_wait, &wait);
    set_current_state(TASK_INTERRUPTIBLE);

    while (cnt) {
	// sleep
	schedule();
	// continue
	printk("putting %d byte, %d remained\n",received,cnt);
	put_user((unsigned char)last_data,buf++);
	received++;
	cnt--;
	// ack
	got_something = 0;
	net64_send_strobe();
	if (signal_pending(current)) {
	    remove_wait_queue(&net64_wait, &wait);
	    set_current_state(TASK_RUNNING);
	    return -EINTR;
	}
	set_current_state(TASK_INTERRUPTIBLE);
    }
    remove_wait_queue(&net64_wait, &wait);
    set_current_state(TASK_RUNNING);
    return received;
}

static int net64_write (struct file *f, const char *buf, size_t cnt, loff_t *ppos)
{
    return 0;
}

static int net64_open (struct inode *inode, struct file *f)
{
    if (busy>2) return -EBUSY;
    if (busy==0) init_waitqueue_head(&net64_wait);
    busy++;
    MOD_INC_USE_COUNT;
    return 0;
}

static int net64_release (struct inode *inode, struct file *f)
{
    MOD_DEC_USE_COUNT;
    busy--;
    return 0;
}

static void net64_interrupt (int irq, void *dev_id, struct pt_regs *regs)
{
    direction = BUSY;
    switch (direction) {
	case 0x00:
	    /* PA2 High - C64 writes */
	    if (phase==0) {
		/* being waked - setup things and strobe */
//		printk("upper\n");
		spin_lock(&net64_lock);
		phase++;
		spin_unlock(&net64_lock);
		outb(READPORT, port+2);
		if (got_something == 0)	//ack only if there are no pending bytes
		    net64_send_strobe();
	    } else {
		spin_lock(&net64_lock);
		/* fetch data, ack it and pass to lower half */
		last_data = inb(port);
		/* reset things */
		phase = 0;
		got_something = 1;
		spin_unlock(&net64_lock);
		/* wake lower half */
		wake_up_interruptible(&net64_wait);
//		printk("lower, data=%d\n",last_data);
		/* don't ack here, lower half must do it*/
	    }
	    break;
	case 0x80:
	    /* PA2 Low */
	    printk("pa2 low,dir==%d\n",direction);
	    break;
    }
}

static struct file_operations net64_ops =
{
    owner:	THIS_MODULE,	/* owner */
    read:	net64_read,	/* read */
    write:	net64_write,	/* write */
    open:	net64_open,	/* open */
    release:	net64_release	/* release */
};

static struct miscdevice net64_dev =
{
    NET64_MINOR,
    NAME,
    &net64_ops
};

void net64_cleanup(void)
{
    parport_release(net64_device);
    parport_unregister_device(net64_device);
    misc_deregister(&net64_dev);
}

int net64_init(void)
{
    int i;
    struct parport *pp;

    for (i=lp, pp = parport_enumerate(); pp && i; i--, pp = pp->next) ;

    if (pp == NULL) {
	printk ("init: non-existant port: %d\n", lp);
	return -ENODEV;
    }
    if (pp->irq <= 0) {
	printk ("init: irq not configured on %d\n", lp);
	return -ENODEV;
    }

    net64_device = parport_register_device(pp, NAME, NULL, NULL, net64_interrupt, PARPORT_DEV_EXCL, NULL);

    if (net64_device == NULL) {
	printk ("init: could not register parport\n");
	return -EBUSY;
    }

    if (parport_claim(net64_device)) {
	parport_unregister_device(net64_device);
	printk ("init: could not initialize\n");
	return -EBUSY;
    }
    port = net64_device->port->base;
    printk("parport %d is mine!\n", port);

    busy = 0;
    phase = 0;
    got_something = 0;

    outb(READPORT, port+2);

    misc_register(&net64_dev);

    schedule_timeout(HZ/20);

    return 0;
}

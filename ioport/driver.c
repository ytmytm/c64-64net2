/* ++++++++++
	driver.c
	A skeletal device driver
+++++ */

#include <SupportDefs.h>
#include <KernelExport.h>
#include <Drivers.h>
#include <ISA.h>
#include "t.h"


#define DEVICE_NAME "misc/ioport"

#define TOUCH(x) ((void)x)
#define DPRINTF(a) dprintf("ioport: "); dprintf a

isa_module_info *isa;
unsigned char inportb(uint32 port);
unsigned char outportb(uint32 port,uint8 data);

/* ----------
	init_hardware - called once the first time the driver is loaded
----- */
status_t
init_hardware (void)
{
DPRINTF(("hello\n"));
	return B_OK;
}


/* ----------
	init_driver - optional function - called every time the driver
	is loaded.
----- */



/* ----------
	uninit_driver - optional function - called every time the driver
	is unloaded
----- */
void
uninit_driver (void)
{
put_module(B_ISA_MODULE_NAME);
}

	
/* ----------
	my_device_open - handle open() calls
----- */

static status_t
my_device_open (const char *name, uint32 flags, void** cookie)
{
TOUCH(name); TOUCH(flags);

	DPRINTF(("open\n"));
		get_module(B_ISA_MODULE_NAME,(module_info**) &isa);

*cookie = NULL;
	return B_OK;
}

/* ----------
	my_device_read - handle read() calls
----- */

static status_t
my_device_read (void* cookie, off_t position, void *buf, size_t* num_bytes)
{

    return B_OK;
}

	
			/* tell caller nothing was read */

/* ----------
	my_device_write - handle write() calls
----- */

static status_t
my_device_write (void* cookie, off_t position, const void* buffer, size_t* num_bytes)
{
	TOUCH(position);
		
	
	return B_OK;
	//*num_bytes = 0;				/* tell caller nothing was written */
	//return B_IO_ERROR;


}
/* ----------
	my_device_control - handle ioctl calls
----- */

static status_t
my_device_control (void* cookie, uint32 op, void* arg, size_t len)
{
	IO_Tuple *iot = (IO_Tuple *)arg;
  switch (op)
    {
        case DRV_READ_IO_8:
          	iot->Data = isa->read_io_8(iot->Port); //inportb implementation
          return B_OK;

        case DRV_WRITE_IO_8:
          	isa->write_io_8(iot->Port, iot->Data); //outportb implementation
          return B_OK;
          
         case DRV_READ_IO_16:
         	iot->Data16 = isa->read_io_16(iot->Port); //inportw implementation
         	return B_OK;
         
         case DRV_WRITE_IO_16:
         	isa->write_io_16(iot->Port, iot->Data16); //outportw implementation
         	return B_OK;

        default:
          return B_BAD_VALUE;
      }
	return B_BAD_VALUE;
}


unsigned char inportb(uint32 port) // I've also successfully used asm in kernel land, the ucon64 asm could be used if wanted
{
  __asm__("
  movl    8(%ebp),%edx
  inb     %dx,%al
  movzbl  %al,%eax                  
  ");
}

unsigned char outportb(uint32 port, uint8 data)
{
  __asm__("
  movl    0x8(%ebp),%edx
  movl    0xc(%ebp),%eax
  outb    %al,%dx
  ");
}

/* ----------
	my_device_close - handle close() calls
----- */

static status_t
my_device_close (void* cookie)
{
TOUCH(cookie);

put_module(B_ISA_MODULE_NAME);

DPRINTF(("close\n"));
	

	return B_OK;
}


/* -----
	my_device_free - called after the last device is closed, and after
	all i/o is complete.
----- */
static status_t
my_device_free (void* cookie)
{
DPRINTF(("free\n"));
free(cookie);
	return B_OK;
}


/* -----
	null-terminated array of device names supported by this driver
----- */

static const char *my_device_name[] = {
	"misc/ioport",
	NULL
};

/* -----
	function pointers for the device hooks entry points
----- */

device_hooks my_device_hooks = {
	my_device_open, 			/* -> open entry point */
	my_device_close, 			/* -> close entry point */
	my_device_free,			/* -> free cookie */
	my_device_control, 		/* -> control entry point */
	my_device_read,			/* -> read entry point */
	my_device_write			/* -> write entry point */
};

/* ----------
	publish_devices - return a null-terminated array of devices
	supported by this driver.
----- */

const char**publish_devices()
{
	return my_device_name;
}

/* ----------
	find_device - return ptr to device hooks structure for a
	given device name
----- */

device_hooks*
find_device(const char* name)
{
	return &my_device_hooks;
}


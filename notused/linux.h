#define outb(a,v) __asm__ __volatile__ ( "\nmov 1,%al\nmov 2,%dx\nout %al,%dx\n");
unsigned char inb(int a);

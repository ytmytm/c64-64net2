
/* Sample program for use with Inpout32
   Maciej Witkowiak 2001
*/


#include <stdio.h>
#include <windows.h>

int port, value;

typedef void (CALLBACK * outb_t)(int,int);
typedef unsigned char (CALLBACK * inb_t)(int);

HINSTANCE hDLL;
outb_t  outb;
inb_t inb;

int main(void) {
    hDLL = LoadLibrary("Inpout32");
    if (hDLL!=NULL) {
        printf("Loaded lib\n");
        outb = (outb_t)GetProcAddress(hDLL,"Out32");
        inb = (inb_t)GetProcAddress(hDLL,"Inp32");
        printf("Set adresses\n");
        if (!outb || !inb) {
           printf("address error\n");
        }
        port = 0x378; value=0xaa;
        while(1) {
                 port=0x37a; value=0xaa;
                 outb(port,value);
                 printf("Sent %i to %i  ",value,port);
                 value = inb(port);
                 printf("Got %i from %i\n",value,port);
        }
    } else { printf("Could not find inpout32.dll\n"); }
    FreeLibrary(hDLL);
    return 0;

}


#include "config.h"

/* debug messages flag */
int debug_mode=0;

static int mallocList[128]={-1};
static int sizeList[128];

int initDebug(void)
{
  int i;

  for(i=0;i<128;i++) mallocList[i]=-1;

  return 0;
}

void *trap_malloc(int size)
{
  int i,j;
  uchar *p;

  for(i=0;i<128;i++)
    if (mallocList[i]==-1)
      break;
  
  if (i==128) {
      printf("DEBUG: malloc debug table full\n");
      return(0);
    }

  mallocList[i]=(int)malloc(size+32);
  sizeList[i]=size;
  
  p=(uchar *)mallocList[i];
  for(j=0;j<16;j++) p[j]=0xbd;
  for(j=0;j<16;j++) p[size+16+j]=0xbd;

  printf("malloc(%d) called.  Returning %08x (with 16byte buffers each end)\n",
	 size,mallocList[i]+16);
  fflush(stdout);

  return((void *)((char *)mallocList[i]+16));

}

void trap_free(int addr)
{
  int i,j;
  uchar *p;

  printf("free(%08x) called.\n", addr);
  fflush(stdout);

  for(i=0;i<128;i++)
    if (addr==mallocList[i]+16) break;

  if (i==128) {
      printf("DEBUG: free() attempted on illegal value (%08x)\n", addr);
      return;
    }

  /* test for corruption */
//  p=(void *)((char*)addr-16);
  p=(uchar*)(addr-16);
  for(j=0;j<16;j++) {
      if (p[j]!=0xbd)
	  printf("DEBUG: free() show memory corruption at (-) %d (%08x)\n",16-j,addr);
      if (p[sizeList[i]+16+j]!=0xbd)
	  printf("DEBUG: free() show memory corruption at (+) %d (%08x)\n",j,addr);
    }

  free((void*)((char*)addr-16));
  return;
}

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* debug messages flag */
int debug_mode=0;

int mallocList[128]={-1};
int sizeList[128];

int initDebug(void)
{
  int i;

  for(i=0;i<128;i++) mallocList[i]=-1;

  return 0;
}

void *trap_malloc(int size)
{
  int i,j;
  char *p;

  for(i=0;i<128;i++)
    if (mallocList[i]==-1)
      break;
  
  if (i==128)
    {
      printf("DEBUG: malloc debug table full\n");
      return(0);
    }

  mallocList[i]=(int)malloc(size+32);
  sizeList[i]=size;
  
  p=(char *)mallocList[i];
  for(j=0;j<16;j++) p[j]=0xbd;
  for(j=0;j<16;j++) p[size+16+j]=0xbd;

  printf("malloc(%d) called.  Returning %08x (with 16byte buffers each end)\n",
	 size,mallocList[i]+16);
  fflush(stdout);
  /* sleep(1); */

  return((void *)mallocList[i]+16);

}

void trap_free(int addr)
{
  int i,j;
  unsigned char *p;

  printf("free(%08x) called.\n",
	 addr);
  fflush(stdout);
  sleep(1);

  for(i=0;i<128;i++)
    if (addr==mallocList[i]+16) break;

  if (i==128)
    {
      printf("DEBUG: free() attempted on illegal value (%08x)\n",
	     addr);
      sleep(3);
      return;
    }

  /* test for corruption */
  p=(void *)addr-16;
  for(j=0;j<16;j++)
    {
      if (p[j]!=0xbd)
	{
	  printf("DEBUG: free() show memory corruption at (-) %d (%08x)\n",16-j,addr);
	  sleep(3);
	}
      if (p[sizeList[i]+16+j]!=0xbd)
	{
	  printf("DEBUG: free() show memory corruption at (+) %d (%08x)\n",j,addr);
	  sleep(3);
	}
    }

  sleep(1);

  free((void*)addr-16);
  return;
}

/* This one conflicts with stuff from Resource Manager so I disabled it */

/*
int trap_fclose(FILE *f)
{
  printf("fclosing %08x\n",(unsigned int)f);

  if (!f)
    printf("Attempting to fclose null.\n");
  else
    fclose(f);
  printf("Fclose'd okay\n");

  return 0;
}
*/

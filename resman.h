/* 
   resource manager defines 
   */


#ifdef DEBUG

/* limit of resource library size */
#define MAX_RES_COUNT 32
#define RES_DESC_LEN    80
/* resource types */
#define RES_none -1
#define RES_file 1
#define RES_dir 2

/* file open and close logging */

FILE *res_fopen(uchar *a,char *b);
int res_fclose(FILE *f);
DIR *res_opendir(uchar *a);
int res_closedir(DIR *d);

#define fopen res_fopen
#define fclose res_fclose
#define opendir res_opendir
#define closedir res_closedir

#endif

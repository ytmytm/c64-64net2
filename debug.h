
#ifdef DEBUG
int initDebug(void);
void trap_free(int addr);
void *trap_malloc(int size);

/* malloc() and free() watching */
#define malloc trap_malloc
#define free   trap_free
/* fclose() monitoring */
/* int trap_fclose(FILE *f); */
/* #define fclose trap_fclose */
/* above confilts with res_fclose */
#endif

extern int debug_mode;

/* appropriate define for debug messages */
/* its hacky, but it works fine :) */

#define debug_msg if (debug_mode) printf("client#%d: ",curr_client); printf 

/* same for log messages */
#define log_msg printf

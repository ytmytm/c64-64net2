
int fs_accel (unsigned char command);

extern int georam_size;
extern unsigned char *georam;

#define FSA_READ_PAGE		'I'
#define FSA_READ_MEMORY		'H'
#define FSA_WRITE_PAGE		'O'
#define FSA_WRITE_MEMORY	'N'
#define FSA_READ_SECTOR		'R'
#define FSA_WRITE_SECTOR	'W'
#define FSA_SIZE_MEMORY		'S'
#define FSA_PAR_INFO		'P'

/*
    Structure holding all fileoperations for fs64
    Not everything is necessary for all filesystems

    Each filesystem have to register itself and fill in proper fields

*/

struct fsops64 = {
    int (*createfile) (char *path,char *name,int t,int rel_len,fs64_file *f,int dirtrack,int dirsect,int media);
    int (*getopenablename) (fs64_file *f,fs64_direntry *de);
    int (*openfile) (fs64_file *f);
    int (*scratchfile) (fs64_direntry *de);
    int (*writeblock) (fs64_file *file);
    int (*readblock) (fs64_file *file);
    int (*openfind) (fs64_direntry *de,char *path,int *dirtrack,int *dirsector);
    int (*closefile) (fs64_file *f);
    int (*writechar) (fs64_file *file,unsigned char c);
    int (*readchar) (fs64_file *file,unsigned char *c);
    int (*allocateblock) (fs64_filesystem *fs,int track,int sector);
    int (*deallocateblock) (fs64_filesystem *fs,int track,int sector);
    int (*findfreeblock) (fs64_filesystem *fs,int *track,int *sector);
    int (*finddirblock) (char *path,int *dirtrack,int *dirsector,char *fsfilename);
    int (*format) (fs64_filesystem *fs,char *name,char *id);
    int (*validate) (fs64_filesystem *fs,int purgeflag);
    int (*findnext) (fs64_direntry *de);
    int (*getinfo) (fs64_direntry *de);
    int (*validate_dir) (fs64_filesystem *fs,int purgeflag,void *bam,int t,int s);
    int (*makebam) (uchar *t18s0);
    int (*bamalloc) (int track,int sector,uchar *block);
    int (*bamdealloc) (uchar *blocks,int t,int s);
    int (*readbam) (fs64_filesystem *fs,uchar *bam);
    int (*writebam) (fs64_filesystem *fs,uchar *bam);
    int (*bamblocksfree) (uchar *blocks);
    int (*bamfindfreeblock) (uchar *blocks,int *t,int *s);
    int (*isblockfree) (fs64_filesystem *fs,int track,int sector);
    int (*blocksfree) (fs64_filesystem *fs);
    int (*headername) (char *path,char *header,char *id,int par,fs64_file *f);
}


struct bamops = {
    int (*makebam) (uchar *t18s0);
    int (*bamalloc) (int track,int sector,uchar *block);
    int (*bamdealloc) (uchar *blocks,int t,int s);
    int (*readbam) (fs64_filesystem *fs,uchar *bam);
    int (*writebam) (fs64_filesystem *fs,uchar *bam);
    int (*bamblocksfree) (uchar *blocks);
    int (*bamfindfreeblock) (uchar *blocks,int *t,int *s);
    int (*blocksfree) (fs64_filesystem *fs);
}

struct fops = {
    int (*createfile) (char *path,char *name,int t,int rel_len,fs64_file *f,int dirtrack,int dirsect,int media);
    int (*getopenablename) (fs64_file *f,fs64_direntry *de);
    int (*openfile) (fs64_file *f);
    int (*scratchfile) (fs64_direntry *de);
    int (*closefile) (fs64_file *f);
    int (*writechar) (fs64_file *file,unsigned char c);
    int (*readchar) (fs64_file *file,unsigned char *c);
}

struct dirops = {
    int (*finddirblock) (char *path,int *dirtrack,int *dirsector,char *fsfilename);
    int (*format) (fs64_filesystem *fs,char *name,char *id);
    int (*validate) (fs64_filesystem *fs,int purgeflag);
    int (*findnext) (fs64_direntry *de);
    int (*getinfo) (fs64_direntry *de);
    int (*validate_dir) (fs64_filesystem *fs,int purgeflag,void *bam,int t,int s);
    int (*headername) (char *path,char *header,char *id,int par,fs64_file *f);
}

struct blockops = {
    int (*allocateblock) (fs64_filesystem *fs,int track,int sector);
    int (*deallocateblock) (fs64_filesystem *fs,int track,int sector);
    int (*findfreeblock) (fs64_filesystem *fs,int *track,int *sector);
    int (*isblockfree) (fs64_filesystem *fs,int track,int sector);
    int (*writeblock) (fs64_file *file);
    int (*readblock) (fs64_file *file);
    int (*openfind) (fs64_direntry *de,char *path,int *dirtrack,int *dirsector);
}


struct fsops = {
    struct bamops	bamop;
    struct fops		fop;
    struct blockops	blockop;
    struct dirops	dirop;
}

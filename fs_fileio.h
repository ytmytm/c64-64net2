
int fs_pathtofilesystem (fs64_filesystem * fs, uchar *path);
int fs64_scratchfile_g (uchar *filespec);
int fs64_readchar (fs64_file * f, uchar *c);
int fs64_writechar (fs64_file * f, uchar c);
int fs64_writeblock (fs64_file * f);
int fs64_readblock (fs64_file * f);
int fs64_openfile_g (uchar *curdir, uchar *filespec, fs64_file * f);
int fs64_closefile_g (fs64_file * f);
int fs64_openfile (fs64_direntry * de, fs64_file * f);
int fs64_closefile (fs64_file * f);

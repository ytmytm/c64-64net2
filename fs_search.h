
int fs64_findfirst_g(unsigned char *path,unsigned char *glob,fs64_direntry *de,int *dirtrack,int *dirsect);
int fs64_openfind_g(unsigned char *path,unsigned char *glob,fs64_direntry *de,int *dirtrack,int *dirsect);
int fs64_closefind_g(fs64_direntry *fe);
int fs64_findnext_g(fs64_direntry *de);
int fs64_findfirst(char *path,fs64_direntry *de,int *dirtrack,int *dirsect);
int fs64_findnext(fs64_direntry *de);
int fs64_getinfo(fs64_direntry *de);


int fs64_resolve_partition(unsigned char *partition,unsigned char *path,int *track,int *sector);
int fs64_parse_filespec(unsigned char *filespec,unsigned char *path,
			unsigned char *glob,int *dirflag,int *mode,
			int *replace,int *par,int *dirtrack,int *dirsect);
int fs64_parse_path(unsigned char *filespec,unsigned char *path,
			int *par,int *dirtrack,int *dirsect);

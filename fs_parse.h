
int fs64_resolve_partition(uchar *partition,uchar *path,int *track,int *sector);
int fs64_parse_filespec(uchar *filespec,uchar *path,
			uchar *glob,int *dirflag,int *mode,
			int *replace,int *par,int *dirtrack,int *dirsect);
int fs64_parse_path(uchar *filespec,uchar *path,
			int *par,int *dirtrack,int *dirsect);

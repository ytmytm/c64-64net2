
int readts(fs64_filesystem *fs,int track,int sector,uchar *buffer);
int writets(fs64_filesystem *fs,int track,int sector,uchar *buffer);
long fs_resolve_ts(int media,int track,int sector);

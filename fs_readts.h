
int readts(fs64_filesystem *fs,int track,int sector,unsigned char *buffer);
int writets(fs64_filesystem *fs,int track,int sector,unsigned char *buffer);
long fs_resolve_ts(int media,int track,int sector);

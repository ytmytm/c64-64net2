
int fs64_mediatype(char *path);
int fs64_readts(fs64_filesystem *fs,int track,int sector,unsigned char *sb);
int fs64_writets(fs64_filesystem *fs,int track,int sector,unsigned char *sb);
int fs64_deallocateblock(fs64_filesystem *fs,int track,int sector);
int fs64_allocateblock(fs64_filesystem *fs,int track,int sector);
int fs64_findfreeblock(fs64_filesystem *fs,int *track,int *sector);
int fs64_blocksfree(fs64_filesystem *fs);

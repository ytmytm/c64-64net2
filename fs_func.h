/*
   Prototypes for file system dependant calls for 64NET/2
   (C)Copyright Paul Gardner-Stephen 1996

   Each new filesystem should provide most of the nn functions below.
   This will then allow 64net/2 to to recognise and use the filesystem
   seamlessly, and with block buffering.  It should also minimise the 
   amount of file system dependencies in the code.  There may yet be more
   functions required (notably filesystem recognition).

   Each filesystem should also have its recognition code in the first 
   function of fs_media.c.

   Currently supported/proposed filesystems include:

   UFS - UNIX File system (really Host File System)
   D64 - 1541 Disk image
   D71 - 1571 Disk Image
   D81 - 1581 Disk Image
   DHD - CMD HD Disk Image
   NET - Internet socket space
   LNX - Lynx Files
*/


/* Disk Image File System (D64 etc..) 
   This includes routines unique to each image, as well as routines 
   suitable for more than one variety of disk image.
   They are sorted into blocks by applicability to media.
   This is by far the biggest filesystem group. */
int fs_d64_blocksfree(fs64_filesystem *fs);
int fs_d64_deallocateblock(fs64_filesystem *fs,int track,int sector);
int fs_d64_allocateblock(fs64_filesystem *fs,int track,int sector);
int fs_d64_findfreeblock(fs64_filesystem *fs,int *track,int *sector);
int fs_d64_bamalloc(int track,int sector,uchar block[256]);
int fs_d64_validate(fs64_filesystem *fs,int purgeflag);
int fs_d64_format(fs64_filesystem *fs,uchar *name,uchar *id);
int fs_d64_makebam(uchar t18s0[256]);
/* not used outside driver source */
int fs_d64_readbam(fs64_filesystem *fs,uchar bam[256]);
int fs_d64_writebam(fs64_filesystem *fs,uchar bam[256]);

/* common for d64 and d71 */
int fs_d47_headername(uchar *path,uchar *header,uchar *id,int par,fs64_file *f);

int fs_d71_blocksfree(fs64_filesystem *fs);
int fs_d71_deallocateblock(fs64_filesystem *fs,int track,int sector);
int fs_d71_allocateblock(fs64_filesystem *fs,int track,int sector);
int fs_d71_findfreeblock(fs64_filesystem *fs,int *track,int *sector);
int fs_d71_bamalloc(int track,int sector,uchar blocks[2][256]);
int fs_d71_validate(fs64_filesystem *fs,int purgeflag);
int fs_d71_format(fs64_filesystem *fs,uchar *name,uchar *id);
/* not used outside driver source */
int fs_d71_makebam(uchar blocks[2][256]);
int fs_d71_readbam(fs64_filesystem *fs,uchar blocks[2][256]);
int fs_d71_writebam(fs64_filesystem *fs,uchar blocks[2][256]);

int fs_d81_blocksfree(fs64_filesystem *fs);
int fs_d81_deallocateblock(fs64_filesystem *fs,int track,int sector);
int fs_d81_allocateblock(fs64_filesystem *fs,int track,int sector);
int fs_d81_findfreeblock(fs64_filesystem *fs,int *track,int *sector);
int fs_d81_headername(uchar *path,uchar *header,uchar *id,int par,fs64_file *f);
int fs_d81_bamalloc(int track,int sector,uchar blocks[3][256]);
int fs_d81_validate(fs64_filesystem *fs,int purgeflag);
int fs_d81_format(fs64_filesystem *fs,uchar *name,uchar *id);
/* not used outside driver source */
int fs_d81_readbam(fs64_filesystem *fs,uchar blocks[3][256]);
int fs_d81_writebam(fs64_filesystem *fs,uchar blocks[3][256]);
int fs_d81_makebam(uchar blocks[3][256]);
int fs_d81_bamblocksfree(uchar blocks[3][256]);
int fs_d81_bamfindfreeblock(uchar blocks[3][256],int *t,int *s);
int fs_d81_bamdealloc(uchar blocks[3][256],int t,int s);

int fs_dhd_blocksfree(fs64_filesystem *fs);
int fs_dhd_deallocateblock(fs64_filesystem *fs,int track,int sector);
int fs_dhd_allocateblock(fs64_filesystem *fs,int track,int sector);
int fs_dhd_findfreeblock(fs64_filesystem *fs,int *track,int *sector);
int fs_dhd_headername(uchar *path,uchar *header,uchar *id,int par,fs64_file *f);
int fs_dhd_bamalloc(int track,int sector,uchar blocks[34][256]);
int fs_dhd_validate(fs64_filesystem *fs,int purgeflag);
int fs_dhd_format(fs64_filesystem *fs,uchar *name,uchar *id);
/* not used outside driver source */
int fs_dhd_readbam(fs64_filesystem *fs,uchar blocks[34][256]);
int fs_dhd_writebam(fs64_filesystem *fs,uchar blocks[34][256]);
int fs_dhd_makebam(uchar blocks[34][256],int num_tracks);
int fs_dhd_bamblocksfree(uchar blocks[34][256]);
int fs_dhd_bamfindfreeblock(uchar blocks[34][256],int *t,int *s);
int fs_dhd_bamdealloc(uchar blocks[34][256],int t,int s);

/* C64 Emulator Tape File System (T64) 
   BUG: This module is suspiciously incomplete! */
int fs_t64_readblock(fs64_file *f);
int fs_t64_writeblock(fs64_file *f);
int fs_t64_getopenablename(fs64_file *f,fs64_direntry *de);
int fs_t64_openfile(fs64_file *f);
int fs_t64_openfind(fs64_direntry *de,uchar *path);
int fs_t64_findnext(fs64_direntry *de);
int fs_t64_getinfo(fs64_direntry *de);

/* 
   Common Interface to Disk Image File System
   This provides common way to access a disk image
*/
int fs_dxx_format(fs64_filesystem *fs,uchar *name,uchar *id);
int fs_dxx_validate(fs64_filesystem *fs,int purgeflag);
int fs_dxx_finddirblock(uchar *path,int *dirtrack,int *dirsector,uchar *fsfilename);
int fs_dxx_createfile(uchar *path,uchar *name,int t,int rel_len,fs64_file *f,int dirtrack,int dirsect,int media);
int fs_dxx_scratchfile(fs64_direntry *de);
int fs_dxx_writeblock(fs64_file *file);
int fs_dxx_readblock(fs64_file *file);
int fs_dxx_getopenablename(fs64_file *f,fs64_direntry *de);
int fs_dxx_openfile(fs64_file *f);
int fs_dxx_validate_dir(fs64_filesystem *fs,int purgeflag,void *bam,int t,int s);
int fs_dxx_openfind(fs64_direntry *de,uchar *path,int *dirtrack,int *dirsector);
int fs_dxx_findnext(fs64_direntry *de);
int fs_dxx_getinfo(fs64_direntry *de);
/* not used outside driver source */
int fs_dxx_deallocateblock(fs64_filesystem *fs,int track,int sector);
int fs_dxx_allocateblock(fs64_filesystem *fs,int track,int sector);
int fs_dxx_findfreeblock(fs64_filesystem *fs,int *track,int *sector);

/* UNIX (Under) File System (UFS)
   This is one of the two "base" file systems, which are not hosted on any
   other filesystem (The other is fs_net). */
int fs_ufs_blocksfree(fs64_filesystem *fs);
int fs_ufs_deallocateblock(fs64_filesystem *fs,int track,int sector);
int fs_ufs_allocateblock(fs64_filesystem *fs,int track,int sector);
int fs_ufs_findfreeblock(fs64_filesystem *fs,int *track,int *sector);
int fs_ufs_createfile(uchar *path,uchar *name,int t,int rec_len,fs64_file *f);
int fs_ufs_scratchfile(fs64_direntry *de);
int fs_ufs_writeblock(fs64_file *file);
int fs_ufs_readblock(fs64_file *file);
int fs_ufs_headername(uchar *path,uchar *header,uchar *id,int par);
int fs_ufs_getopenablename(fs64_file *f,fs64_direntry *de);
int fs_ufs_openfile(fs64_file *f);
int fs_ufs_openfind(fs64_direntry *de,uchar *path);
int fs_ufs_findnext(fs64_direntry *de);
int fs_ufs_getinfo(fs64_direntry *de);
/* not used outside driver source */
int fs_ufs_isblockfree(fs64_filesystem *fs,int track,int sector);

/* LYNX File System (LNX)
 This file system allows *READONLY* access to .LNX
 files*/
int fs_lnx_createfile(uchar *path,uchar *name,int t,fs64_file *f);
int fs_lnx_scratchfile(fs64_direntry *de);
int fs_lnx_writeblock(fs64_file *file);
int fs_lnx_readblock(fs64_file *file);
//int fs_lnx_headername(uchar *path,uchar *header,uchar *id,int par,fs64_file *f);
int fs_lnx_headername (uchar *path, uchar *header, uchar *id, int par);
    
int fs_lnx_getopenablename(fs64_file *f,fs64_direntry *de);
int fs_lnx_openfile(fs64_file *f);
int fs_lnx_openfind(fs64_direntry *de,uchar *path);
int fs_lnx_findnext(fs64_direntry *de);
int fs_lnx_getinfo(fs64_direntry *de);
/* not used outside driver source */
int fs_lnx_isblockfree(fs64_filesystem *fs,int track,int sector);
int fs_lnx_deallocateblock(fs64_filesystem *fs,int track,int sector);
int fs_lnx_allocateblock(fs64_filesystem *fs,int track,int sector);
int fs_lnx_blocksfree(fs64_filesystem *fs);
int fs_lnx_findfreeblock(fs64_filesystem *fs,int *track,int *sector);

/* InterNET File System (NET)
 This filesystem allows access to the Internet TCP/IP (IP4)
 domain */
int fs_net_blocksfree(fs64_filesystem *fs);
int fs_net_createfile(uchar *path,uchar *name,int t,fs64_file *f);
int fs_net_scratchfile(fs64_direntry *de);
int fs_net_writeblock(fs64_file *file);
int fs_net_readblock(fs64_file *file);
int fs_net_headername(uchar *path,uchar *header,uchar *id,int par);
int fs_net_getopenablename(fs64_file *f,fs64_direntry *de);
int fs_net_openfile(fs64_file *f);
int fs_net_closefile(fs64_file *f);
int fs_net_writechar(fs64_file *file,uchar c);
int fs_net_readchar(fs64_file *file,uchar *c);
int fs_net_openfind(fs64_direntry *de,uchar *path);
int fs_net_findnext(fs64_direntry *de);
int fs_net_getinfo(fs64_direntry *de);
/* not used outside driver source */
int fs_net_isblockfree(fs64_filesystem *fs,int track,int sector);
int fs_net_deallocateblock(fs64_filesystem *fs,int track,int sector);
int fs_net_allocateblock(fs64_filesystem *fs,int track,int sector);
int fs_net_findfreeblock(fs64_filesystem *fs,int *track,int *sector);

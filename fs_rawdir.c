/*
   64NET/2 Directory handlers
   (C) Copyright Paul Gardner-Stephen 1996 and Maciej Witkowiak 2000
*/

/*
    functions to regenerate directory as binary file if file opened with
    secondary address != 0
    
    Later merge this file with fs_fileio.c or extract similar functions from
    there and put here.
    
*/

#include "config.h"
#include "fs.h"
#include "fs_media.h"

int
fs64_rawdirheader (fs64_file * f, int par, uchar *label, uchar *id)
{
    int i;
/*    char buffer[256]; */
    /* send 254 bytes of raw disk header */

    switch (f->filesys.media) {
    
    case media_D64:
    case media_D71:
    case media_D81:
    case media_DHD:
    /* get real block and send last 254 bytes */
/*	fs64_readts(&f->filesys,f->filesys.dirtrack, f->filesys.dirsector, buffer);
	for (i=2;i<256;i++)
	    f->buffer[f->be++] = buffer[i];
	break;
*/    
    default:
    {

    f->buffer[f->be++] = 65;

    /* now the BAM + 0 */
    for (i=0;i<141;i++)
	f->buffer[f->be++] = 0;

    /* now the name */
    /* 16 chars of par */
    for (i = 0; i < 16; i++)
	if (i < strlen (label))
	    f->buffer[f->be++] = label[i];
	else
	    f->buffer[f->be++] = 0xa0;

    /* filler */
    f->buffer[f->be++] = 0xa0;
    f->buffer[f->be++] = 0xa0;

    /* id */
    for (i = 0; i < 5; i++)
	if (i < strlen (id))
	    f->buffer[f->be++] = id[i];
	else
	    f->buffer[f->be++] = 0xa0;

    /* filler */
    f->buffer[f->be++] = 0xa0;
    f->buffer[f->be++] = 0xa0;
    f->buffer[f->be++] = 0xa0;
    f->buffer[f->be++] = 0xa0;

    /* remains */
    for (i=0;i<85;i++)
	f->buffer[f->be++] = 0;

    }
    }
    return (0);
}

int
fs64_direntry2rawentry (fs64_file *f)
{
    int i;

    /* convert the current direntry into a plain 1541 style 32 byte dir entry */
    f->buffer[f->be++] = f->de.filetype;
    f->buffer[f->be++] = f->de.track;	/* track */
    f->buffer[f->be++] = f->de.sector;	/* sector */

    for (i=0; i<16; i++)
	f->buffer[f->be++] = f->de.fs64name[i];

    f->buffer[f->be++] = 0;
    f->buffer[f->be++] = 0;
    f->buffer[f->be++] = 0;
    f->buffer[f->be++] = 0;
    
    f->buffer[f->be++] = 0;
    f->buffer[f->be++] = 0;
    f->buffer[f->be++] = 0;
    f->buffer[f->be++] = 0;
    f->buffer[f->be++] = 0;
    
    if (f->de.blocks < 65536) {
    /* <65536 blocks */
	f->buffer[f->be++] = (f->de.blocks) & 0xff;
	f->buffer[f->be++] = (f->de.blocks) >> 8; }
    else {
    /* 65536+ blocks */
	f->buffer[f->be++] = 0xff;
	f->buffer[f->be++] = 0xff; }

    return (0);
}

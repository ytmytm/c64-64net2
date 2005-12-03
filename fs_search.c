/* 
   64NET/2 directort operations module
   (C)Copyright Paul Gardner-Stephen 1995, 1996
 */

#include "config.h"
#include "fs.h"
#include "fs_func.h"

/* globbed searches */

int
fs64_findfirst_g (uchar *path, uchar *glob, fs64_direntry * de, int *dirtrack, int *dirsect)
{
  if (!fs64_openfind_g (path, glob, de, dirtrack, dirsect)) {
    return (fs64_findnext_g (de));
  }
  else
    return (-1);

}

int
fs64_closefind_g (fs64_direntry * de)
{

  /* dont have to do anything at this point in time */

  if ((de->filesys.fsfile) && (de->filesys.media != media_UFS) && (de->filesys.media != media_NET))
  {
#ifndef AMIGA
    fclose (de->filesys.fsfile);
    de->filesys.fsfile = 0;
#endif
  }

  if ((de->dir) && (de->filesys.media == media_UFS))
  {
#ifndef AMIGA
    closedir (de->dir);
    de->dir = 0;
#endif
  }


  return (0);

}

int
fs64_openfind_g (uchar *path, uchar *glob, fs64_direntry * de, int *dt, int *ds)
{
/*   int i; */

  /* Do a normal search, then check the returned de to ensure it is
     valid in-terms of the globs */

  /* parse glob string in */
  parse_glob (de->glob, glob);

  /* ok, fire things up as usual */

  /* Determine what media type the source object is
     (media_D64,media_D71,media_D81,media_DHD media_UFS or media_T64 )
   */

  strcpy (de->path, path);

  de->filesys.media = fs64_mediatype (path);
  /* FILESYSTEM_SPECIFIC */
  switch (de->filesys.media)
  {
  case media_NOTFS:
  case media_BAD:
    {
      /* Bad/missing media = missing directory */
      /* 39, FILE NOT FOUND,00,00 */
      debug_msg ("Bad/missing media\n");
      set_error (39, 0, 0);
      return (-1);		/* no entry got */
    }
  case media_UFS:
    return (fs_ufs_openfind (de, path));
    break;
  case media_D64:
  case media_D71:
  case media_D81:
  case media_DHD:
    return (fs_dxx_openfind (de, path, dt, ds));
    break;
  case media_T64:
    return (fs_t64_openfind (de, path));
    break;
  case media_LNX:
    return (fs_lnx_openfind (de, path));
    break;
  case media_NET:
    return (fs_net_openfind (de, path));
    break;
  default:
    /* unknown media */
    /* 74,DRIVE NOT READY,00,00 */
    set_error (74, 0, 0);
/*      for(i=0;i<17;i++)
   if (de->glob[i]) free(de->glob[i]); */
    return (-1);
  }
}


int
fs64_findnext_g (fs64_direntry * de)
{
  /* keep doing normal fs64_findnext's until we get a glob match on it */
  int foo = 1;

  while (foo)
  {
    /* find next normally */
    foo = fs64_findnext (de);
    /* if we cant get another entry... */
    if (foo)
    {
      /* gluk! End of directory */
      /* not an error */
      return (-1);
    }

    if (de->invisible)		/* invisibility check */
    {
      foo = 1;
    }
    else
    {
      if ((de->glob[0][de->filetype & 0x0f]) == 0)	/* filetype check */
//      if ((de->filetype & 0x0f) == 0)	/* filetype check */
      {
	/* we cant match this file type */
	/* for some reason we need the following extra statement,
	   or this routine misbehaves somewhat */
	de->filetype += 0;
	foo = 1;
      }
      else
      {
	/* glob check */
	foo = !glob_p_comp (de->glob, de->fs64name);
      }
    }
  }

  /* we have a match! */
  return (0);
}

/* Non-globbing directory searches */

int
fs64_findfirst (uchar *path, fs64_direntry * de, int *dirtrack, int *dirsect)
{
  /* Find first file matching the spec, in file system object path
     also matching the commodore filetype in filetypemask */

  /* Determine what media type the source object is
     (media_D64,media_D71,media_D81,media_DHD media_UFS or media_T64 )
   */

  strcpy (de->path, path);

  de->filesys.media = fs64_mediatype (path);
  /* FILESYSTEM_SPECIFIC */
  switch (de->filesys.media)
  {
  case media_NOTFS:
  case media_BAD:
    {
      /* Bad/missing media */
      /* 39,DIR NOT FOUND,00,00 */
      set_error (39, 0, 0);
      return (-1);		/* no entry got */
    }
  case media_UFS:
    fs_ufs_openfind (de, path);
    return (fs64_findnext (de));
  case media_D64:
  case media_D71:
  case media_D81:
  case media_DHD:
    fs_dxx_openfind (de, path, dirtrack, dirsect);
    return (fs64_findnext (de));
  case media_T64:
    fs_t64_openfind (de, path);
    return (fs64_findnext (de));
  case media_LNX:
    fs_lnx_openfind (de, path);
    return (fs64_findnext (de));
  case media_NET:
    fs_net_openfind (de, path);
    return (fs64_findnext (de));
  default:
    /* unknown media */
    /* 74, DRIVE NOT READY,00,00 */
    set_error (74, 0, 0);
    return (-3);
  }
}

int
fs64_findnext (fs64_direntry * de)
{
  if (de->active != 1)
  {
    /* if it isnt active, then dont try to get the next file! */
    return (-1);
  }

  /* FILESYSTEM_SPECIFIC */
  switch (de->filesys.media)
  {
  case media_UFS:
    return (fs_ufs_findnext (de));
  case media_D71:
  case media_D81:
  case media_DHD:
  case media_D64:
    return (fs_dxx_findnext (de));
  case media_T64:
    return (fs_t64_findnext (de));
  case media_LNX:
    return (fs_lnx_findnext (de));
  case media_NET:
    return (fs_net_findnext (de));
  default:
    /* probably something bad */
    /* 74,DRIVE NOT READY,00,00 */
    set_error (74, 0, 0);
    return (-1);
  }
}

int
fs64_getinfo (fs64_direntry * de)
{
  /* Get all the information about the directory entry including the cbm_xxx
     file base type (eg cbm_N64 cbm_UFS cbm_RAW etc)
   */

  /* FILESYSTEM_SPECIFIC */
  switch (de->filesys.media)
  {
  case media_UFS:
    return (fs_ufs_getinfo (de));
  case media_D71:
  case media_D81:
  case media_DHD:
  case media_D64:
    return (fs_dxx_getinfo (de));
  case media_T64:
    return (fs_t64_getinfo (de));
  case media_LNX:
    return (fs_lnx_getinfo (de));
  case media_NET:
    return (fs_net_getinfo (de));
  default:
    {
      /* dont do anything to unknown media */
    }
  }				/* end switch de.media */

  /* 74,DRIVE NOT READY,00,00 */
  set_error (74, 0, 0);
  return (-1);			/* probably bad */
}				/* end of fs64_getinfo */

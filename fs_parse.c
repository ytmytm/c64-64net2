/*
   File system parsing routines for 64NET/2
   (C)Copyright Paul Gardner-Stephen 1995, All rights reserved
 */

#include "config.h"
#include "fs.h"

/* Re-written version of resolve partition, to allow globbing in paths in
   a proper manner */
int
fs64_resolve_partition (uchar *partition, uchar *path, int *dirtrack, int *dirsect)
{
  /* resolve the partition and path into a single path that points 
     to the right place */

  uchar temp[1024];
  int i;

  if (partition[0] == 0)
  {
    /* default partition is 0: */
    partition[0] = '0';
    partition[1] = 0;
  }

  if (strcmp ((char*)partition, "0"))
  {
    if (partition[0] == 'n')
    {
      /* no partition */
      temp[0] = '/';
      temp[1] = 0;
    }
    else
    {
      /* its not partition `0' */
      if (partn_dirs[last_unit][atol (partition)])
      {
	strcpy ((char*)temp, (char*)curr_dir[last_unit][atol (partition)]);
	*dirtrack = curr_dirtracks[last_unit][atol (partition)];
	*dirsect = curr_dirsectors[last_unit][atol (partition)];
      }
      else
      {
	/* No path for this partition */
	/* 77, ILLEGAL PARTITION,00,00 */
	debug_msg("Error reason: Path for specified partition (%s) is null\n",
		  partition);
	set_error (77, 0, 0);
	return (-1);
      }
    }
  }
  else
  {
    {
      /* its partition `0' */
      strcpy ((char*)temp, (char*)curr_dir[last_unit][curr_par[last_unit]]);
      *dirtrack = curr_dirtracks[last_unit][curr_par[last_unit]];
      *dirsect = curr_dirsectors[last_unit][curr_par[last_unit]];
    }
  }

  if (((path[0] == 95) && (path[1] == 0)) || (path[0]=='.' && path[1]=='.' && path[2]==0))
  {
    /* <- */
    /* This is unchanged from the old resolve partition */
    if (!atol (partition))
    {
      strcpy ((char*)temp, (char*)curr_dir[last_unit][curr_par[last_unit]]);
      *dirtrack = curr_dirtracks[last_unit][curr_par[last_unit]];
      *dirsect = curr_dirsectors[last_unit][curr_par[last_unit]];
      switch (fs64_mediatype (temp))
      {
      case media_DHD:
	{
	  fs64_filesystem fs;
	  uchar block[256];
	  fs.fsfile = 0;

	  if (fs_pathtofilesystem (&fs, temp))
	    return (-1);
	  if (readts (&fs, *dirtrack, *dirsect, block))
	  {
	    fclose (fs.fsfile);
	    fs.fsfile = 0;
	    return (-1);
	  }
	  fclose (fs.fsfile);
	  fs.fsfile = 0;
	  if (block[34])
	  {
	    if ((*dirtrack != block[34]) || (*dirsect != block[35]))
	    {
	      /* there is a parent */
	      *dirtrack = block[34];
	      *dirsect = block[35];
	      break;
	    }
	  }
	  /* fall through */
	}
      default:
	if (strcmp ((char*)temp, (char*)partn_dirs[last_unit][curr_par[last_unit]]))
	{
	  /* CD != root dir */
	  /* so trim temp back to next '/' */
	  temp[strlen (temp) - 1] = 0;
	  while (temp[strlen (temp) - 1] != '/')
	    temp[strlen (temp) - 1] = 0;
	  /* set dir track & sector back to default */
	  *dirtrack = -1;
	  *dirsect = -1;
	}
	break;
      }
    }
    else
    {
      strcpy ((char*)temp, (char*)curr_dir[last_unit][atol (partition)]);
      *dirtrack = curr_dirtracks[last_unit][atol (partition)];
      *dirsect = curr_dirsectors[last_unit][atol (partition)];
      if (strcmp ((char*)temp, (char*)partn_dirs[last_unit][atol (partition)]))
      {
	/* CD != root dir */
	/* so trim temp back to next '/' */
	temp[strlen (temp) - 1] = 0;
	while (temp[strlen (temp) - 1] != '/')
	  temp[strlen (temp) - 1] = 0;
      }
    }
  }
  else
  {
    /* not <- */
    /* This is the changed section of code, which checks each section */
	  i=strlen(path);
    if(strcmp((char*)path,"/../")==0 
     || strcmp((char*)path,"/./")==0
     || strcmp((char*)path,"/~/")==0
     || strcmp((char*)path,"/*")==0
     || strcmp((char*)path,"/*/")==0
     || strcmp((char*)path,"/.*")==0
     || strcmp((char*)path,"/.*/")==0
     || strcmp((char*)path,"/.?/")==0
     || strcmp((char*)path,"/?./")==0
//   || strcmp((char*)path,"/??/")==0
     || strcmp((char*)path,"/..*/")==0
     || (i>=2 && path[i-1]=='.' && path[i-2]=='/')
     || (i>=3 && path[i-1]=='.' && path[i-2]=='.' && path[i-3]=='/')
     || (i>=2 && path[i-1]=='?' && path[i-2]=='/')
     || (i>=3 && path[i-1]=='?' && path[i-2]=='.' && path[i-3]=='/')
     || (i>=2 && path[i-1]=='*' && path[i-2]=='/')
     || (i>=2 && path[i-1]=='~' && path[i-2]=='/')
     
     || (i>=2 && path[0]=='.' && path[1]=='/')
     || (i>=3 && path[0]=='.' && path[1]=='.' && path[2]=='/')
     || (i>=2 && path[0]=='.' && path[1]=='*')
     || (i>=2 && path[0]=='.' && path[1]=='?')
     || (i>=2 && path[0]=='?' && path[1]=='?')
     || (i>=3 && path[0]=='.' && path[1]=='.' && path[2]=='*')
     || (i>=1 && path[0]=='*')
     || (i>=2 && path[0]=='~' && path[1]=='/'))
     {

//     || strcmp(path,"./")==0
//     || strcmp(path,"../")==0
//     || strcmp(path,"~/")==0
//     || strcmp(path,"/.")==strlen(path)-2
//     || strcmp(path,"/~")==strlen(path)-2
      debug_msg("Illegal path\n");
      set_error (71, 0, 0);
      return (-1);
    }		  
    if (path[0] == '/')
    {
      /* absolute path */
      /* dont copy / as the curr_dir is ended with one */
      if (partition[0] != 'n') {
	if (!atol (partition))
	{
	  strcpy ((char*)temp, (char*)partn_dirs[last_unit][curr_par[last_unit]]);
	  *dirtrack = curr_dirtracks[last_unit][curr_par[last_unit]];
	  *dirsect = curr_dirsectors[last_unit][curr_par[last_unit]];
	}
	else
	{
	  strcpy ((char*)temp, (char*)partn_dirs[last_unit][atol (partition)]);
	  *dirtrack = curr_dirtracks[last_unit][atol (partition)];
	  *dirsect = curr_dirsectors[last_unit][atol (partition)];
	}
      }
      strcpy ((char*)path, (char*)&path[1]);
    }
    else
    {
      /* relative path */
      if (partition[0] != 'n')
	if (!atol (partition))
	{
	  strcpy ((char*)temp, (char*)curr_dir[last_unit][curr_par[last_unit]]);
	  *dirtrack = curr_dirtracks[last_unit][curr_par[last_unit]];
	  *dirsect = curr_dirsectors[last_unit][curr_par[last_unit]];
	}
	else
	{
	  strcpy ((char*)temp, (char*)curr_dir[last_unit][atol (partition)]);
	  *dirtrack = curr_dirtracks[last_unit][atol (partition)];
	  *dirsect = curr_dirsectors[last_unit][atol (partition)];
	}
      else
	strcpy ((char*)temp, "");
    }

    /* now check each path element, and add the first match */
    /* I think a state diagram would be shiny here */
    if (strlen (path))
    {
      int state = 1, i = 0, j;
      char pathelement[256];
      fs64_direntry de;

      de.dir = 0;
      pathelement[0] = 0;

      while (state)
      {
	switch (state)
	{
	case 1:		/* waiting for slash */
	  if (i >= strlen (path))
	  {
	    state = 2;
	    break;
	  }
	  switch (path[i])
	  {
	  case '/':		/* end of path element */
	    {
	      i++;
	      state = 2;	/* parse element */
	    };
	    break;
	  case 0:		/* end of path */
	    {
	      state = 2;	/* parse element */
	    };
	    break;
	  default:		/* concat char to path element */
	    {
	      if (i == strlen (path))
	      {
		state = 2;
		i++;
	      }
	      else
	      {
		pathelement[strlen (pathelement) + 1] = 0;
		pathelement[strlen (pathelement)] = path[i++];
	      }
	    };
	    break;
	  };
	  break;
	case 2:		/* pathelement has something in it to be parsed */
	  {
	    if (!strlen (pathelement))
	    {
	      /* null path element cant be matched */
	      debug_msg("Error Explanation: Null path element\n");
	      set_error (39, 0, 0);
	      return (-1);
	    }
	    else
	    {
	      uchar glob[1024];
	      /* match the glob */
	      sprintf ((char*)glob, "%s,D", pathelement);
	      if (fs64_findfirst_g (temp, glob, &de, dirtrack, dirsect))
	      {
		/* dir not found */
		debug_msg("Error reason: fs64_findfirst_g(%s,%s,&de,%d,%d) failed\n",
			  temp,glob,*dirtrack,*dirsect);
		set_error (39, 0, 0);
		fs64_closefind_g (&de);
		return (-1);
	      }
	      else
	      {
		/* before we go any futher, lets close the find,
		   and not leak memory */
		fs64_closefind_g (&de);
		/* match.. now, is it a dir */
		if ((de.filetype & 0x0f) != cbm_DIR)
		{
		  /* not a dir */
		  debug_msg("Error reason: de.filetype& 0x0f (= %d) != cbm_DIR\n",
			    de.filetype & 0x0f);
		  set_error (64, 0, 0);
		  return (-1);
		}
		/* bingo! */
		*dirtrack = de.first_track;
		*dirsect = de.first_sector;
	      }
	      /* do a media check */
	      j = fs64_mediatype (de.realname);
	      switch (j)
	      {
	      case media_BAD:
	      case media_NOTFS:
		/* no sir, it isnt it */
		debug_msg("Error reason: fs64_mediatype(%s) returned media_BAD or media_NOTFS\n",de.realname);
		set_error (39, 0, 0);
		return (-1);
	      default:
		strcpy ((char*)temp, (char*)de.realname);
		/* make sure path ends in a "/" */
		if ((temp[strlen (temp) - 1] != '/')
		    && (fs64_mediatype (temp) == media_UFS))
		  strcat ((char*)temp, "/");
		if (strlen (path) > i)
		  state = 1;
		else
		  state = 0;
		pathelement[0] = 0;
	      }
	    }
	  };
	  break;		/* end of case 2: (process path element) */
	}			/* end of while(state); */
      }
    }

  }				/* end of not <- */

  /* make sure ends in '/' if needed */
  if ((temp[strlen (temp) - 1] != '/')
      && (fs64_mediatype (temp) == media_UFS))
    strcat ((char*)temp, "/");

  /* copy temp back into path */
  strcpy ((char*)path, (char*)temp);

  return (0);

}

int
fs64_parse_filespec (uchar *filespec2, uchar *path,
		     uchar *glob, int *dirflag, int *mode,
		     int *replace, int *par, int *dirtrack, int *dirsect)
{
  /* take a file file reference string, and seperate it in to the path, glob
     string and a few other things (eg whether the spec refers to a file or a
     directory etc
     Also, glob the path part of this */

  int i, j;
  uchar *tmp;
  uchar partition[256];
  uchar filespec_snoz[1024];
  uchar *filespec = filespec_snoz;

  /* dont modify original */
  memset (filespec, 0, 1024);
  strcpy ((char*)filespec, (char*)filespec2);

  /* strip linefeeds left by lazy routines */
  if ((filespec[strlen (filespec) - 1] == 0x0a) || (filespec[strlen (filespec) - 1] == 0x0d))
    filespec[strlen (filespec) - 1] = 0;

  /* strip '0:' off the filespec completely as it confuses the filesearch */

  tmp = (uchar*)strstr((char*)filespec,"0:");
  if (tmp!=NULL) {
    strcpy ((char*)tmp, (char*)&tmp[2]);
  }

  /* moreover, due to 1541 compatibility: $0: and $0 are also legal and mean current directory */
  if ((filespec[0]=='$') && filespec[1]=='0' && ((filespec[2]=='\0') || (filespec[2]==':'))) {
	filespec[1]='$';
	filespec = &filespec[1];
  }

  /* set some defaults */
  *dirflag = 0;
  *mode = mode_READ;
  *replace = 0;
  path[0] = 0;
  glob[0] = 0;

  /* STEP 1 - check out the first char */
  switch (filespec[0])
  {
  case '$':
    /* its a directory */
    *dirflag = 1;
    filespec++;
    break;
  case '@':
    /* save with replace */
    *replace = 1;
    filespec++;
    break;
  default:
    /* its just a char */
    break;
  }

  /* STEP 2 - look for ':' (or '<-') for partition reference */
  /* and resolve path and partition */

  for (i = 0; i < strlen (filespec); i++)
    if ((filespec[i] == ':') || (filespec[i] == 95))
      break;

  if (i < strlen (filespec))
  {
    /* we have a partition reference */
    /* now, get partition name */

    for (j = 0; j < i; j++)
      if ((filespec[j] == '/') || (filespec[j] == 95))
	/* we have a path too! */
	break;
      else
	partition[j] = filespec[j];

    partition[j] = 0;

    /* get path */

    filespec += j;

    /* if the leftmost char is a '/' then strip it */
    if (filespec[0] == '/')
      filespec++;
    for (j = 0; j <= strlen (filespec); j++)
      if (filespec[j] == ':')
      {
	/* found end of path */
	path[j] = 0;
	break;
      }
      else
	path[j] = filespec[j];

    /* shift pointer to start of real filename */
    filespec += j + 1;



    /* now, resolve the partition name into a path, and concatenate it with 
       the path to give the absolute path to the directory in question */
    if (fs64_resolve_partition (partition, path, dirtrack, dirsect))
    {
      /* resolve failed */
      /* resolve will have set the error which occured */
      return (-1);
    }
    /* ok, path now has the full path name to the file system & file */
    debug_msg("fs64_resolve_partition said: path=%s, partition=%s\n",path,partition);
  }
  else
  {
    /* no partition reference */
    strcpy ((char*)partition, "0");
    if (fs64_resolve_partition ((uchar*)"0", path, dirtrack, dirsect))
    {
      /* cant resolve default partition & path */
      /* resolve will have set error appropriately */
      return (-1);
    }
  }

  *par = atol (partition);

  /* STEP 3 - Get glob string */
  strcpy ((char*)glob, (char*)filespec);

  /* one last thing, check for ,R and ,W in glob */
  /* BUGS: This does not account for file accesses on lf 0 or lf 1 which *force*
     read and write respectively. This may be fixed by the interfacer which should
     append ,R and ,W to the filespecs in these cases */

  /* BUGS: This will behave *exactly* bug-compatible with the 1541
     that takes the last ,R or ,W on a filespec to be the valid one */
  for (i = strlen (filespec) - 1; -1 < i; i--)
  {
    if ((filespec[i] == ',') && (filespec[i + 1] == 'R'))
    {
      debug_msg ("read\n");
      *mode = mode_READ;
      break;
    }
    if ((filespec[i] == ',') && (filespec[i + 1] == 'W'))
    {
      debug_msg ("write\n");
      *mode = mode_WRITE;
      break;
    }
  }

  /* STEP 3.5 - Glob path to allow things like LOAD"$/ST*:* */


  /* STEP 4- Were done! */
  return (0);
}


int
fs64_parse_path (uchar *filespec2, uchar *path, int *par, int *dirtrack, int *dirsect)
{
  /* Take a path reference string, and resolve the absolute path & dirblock
     indicated by it.  The partition number is also returned for 
     reference purpose only (eg Partition locking for GEOS, or anything
     like that

     Valid formats are:
     nnn
     nnn:
     nnn/foo:
     nnn/foo:bar
     nnn:foo
     :foo
     (Where foo and bar are valid paths)

     BUGS: Incomplete, does not parse all types (only parses to the : ) 
   */

  int i, j;
  uchar partition[256];
  uchar filespec_snoz[1024];
  uchar *filespec = filespec_snoz;

  /* dont modify original */
  strcpy ((char*)filespec, (char*)filespec2);

  /* strip linefeeds left by lazy routines */
  if (filespec[strlen (filespec) - 1] == 0x0a)
    filespec[strlen (filespec) - 1] = 0;

  /* set some defaults */
  path[0] = 0;

  /* STEP 2 - look for ':' (or '<-') for partition reference */
  /* and resolve path and partition */

  for (i = 0; i < strlen (filespec); i++)
    if ((filespec[i] == ':') || (filespec[i] == 95))
      break;

  if (i < strlen (filespec))
  {
    /* we have a partition reference */
    /* now, get partition name */

    for (j = 0; j < i; j++)
      if ((filespec[j] == '/') || (filespec[j] == 95))
	/* we have a path too! */
	break;
      else
	partition[j] = filespec[j];

    partition[j] = 0;

    /* get path */

    filespec += j;

    /* if the leftmost char is a '/' then strip it */
    if (filespec[0] == '/')
      filespec++;
    for (j = 0; j <= strlen (filespec); j++)
      if (filespec[j] == ':')
      {
	/* found end of path */
	path[j] = 0;
	break;
      }
      else
	path[j] = filespec[j];

    /* shift pointer to start of real filename */
    filespec += j + 1;

    *par = atol (partition);

    /* now, resolve the partition name into a path, and concatenate it with 
       the path to give the absolute path to the directory in question */
    if (fs64_resolve_partition (partition, path, dirtrack, dirtrack))
    {
      /* resolve failed */
      /* resolve will have set the error which occured */
      return (-1);
    }
    /* ok, path now has the full path name to the file system & file */
    return (0);
  }
  else
  {
    /* only partition reference */
    strcpy ((char*)partition, (char*)filespec);
    *par = atol (partition);
    path[0] = 0;
    if (fs64_resolve_partition (partition, path, dirtrack, dirsect))
    {
      /* cant resolve default partition & path */
      /* resolve will have set error appropriately */
      return (-1);
    }
    else
    {
      /* parsed okay, now return */
      return (0);
    }
  }

}

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <stdio.h>
#include <stdlib.h>
#define XA_BITMAPDEPTH 1

#include "15xx.xpm"

/* Generic definitions */

Display *display;
int screen, w_x, w_y;
Window win;
unsigned int width, height;
unsigned int border_width = 4;
unsigned int display_width, display_height;
char *window_name = "64NET/2 (C) Paul Gardner-Stephen 1996";
char *icon_name = "64NET/2 (C)1996 Paul Gardner-Stephen 1996";
Pixmap icon_pixmap, pm;
XSizeHints size_hints;
XEvent report;
GC gc;
XGCValues gcvalues;
XFontStruct *font_info;
XColor xcolour;
char *display_name = NULL;
char *fontname = "9x15";
unsigned long colours[4];

int blinkflag = 0, derror = 1, active = 0;

FILE *f = 0;

int 
main (argc, argv)
     int argc;
     char **argv;
{
  char temp[1024];

  if (argc < 2)
  {
    strcpy (temp, getenv ("HOME"));
    strcat (temp, "/.64net2.leds");
  }
  else
    strcpy (temp, argv[1]);

  if ((f = fopen (temp, "r")) == NULL)
  {
    printf ("x64net: Can't open %s to get 64net status - barf!\n"
	    ,temp);
    exit (3);
  }

  /* Connect to XServer */
  if ((display = XOpenDisplay (display_name)) == NULL)
  {
    (void) fprintf (stderr, "Doh! cant connect to XServer!");
    XDisplayName (display_name);
    exit (-1);
  }

  screen = DefaultScreen (display);

  display_width = DisplayWidth (display, screen);
  display_height = DisplayHeight (display, screen);

  /* create GC for text and drawing */
  gcvalues.foreground = BlackPixel (display, screen);
  gcvalues.background = WhitePixel (display, screen);
  gc = XCreateGC (display, RootWindow (display, screen),
		  (GCForeground | GCBackground), &gcvalues);


  w_x = 100;
  w_y = 100;
  width = 400;
  height = 150;

  /* create the main application window */
  /* (with black background instead of white) */
  win = XCreateSimpleWindow (display, RootWindow (display, screen),
			     w_x, w_y, width, height, border_width,
			     WhitePixel (display, screen),
			     BlackPixel (display, screen));

  /* create pixmap */
  pm = XCreatePixmap (display, win, width, height, DefaultDepth (display, screen));
  /* allocate colours */
  if (!XAllocColorCells (display, DefaultColormap (display, screen), 0,
			 (unsigned long *) &colours, 0,
			 (unsigned long *) &colours, 4))
  {
    printf ("x64net: Cannot allocate colours - barfing\n");
    exit (2);
  }
  else
  {
    xcolour.pixel = colours[0];
    xcolour.flags = DoRed | DoBlue | DoGreen;
    xcolour.red = 0xc3c3;
    xcolour.green = 0xb5b5;
    xcolour.blue = 0x7e7e;
    XStoreColor (display, DefaultColormap (display, screen), &xcolour);

    xcolour.pixel = colours[1];
    xcolour.flags = DoRed | DoBlue | DoGreen;
    xcolour.red = 0xaeae;
    xcolour.green = 0xa1a1;
    xcolour.blue = 0x7070;
    XStoreColor (display, DefaultColormap (display, screen), &xcolour);

    xcolour.pixel = colours[2];
    xcolour.flags = DoRed | DoBlue | DoGreen;
    xcolour.red = 0xffff;
    xcolour.green = 0x0;
    xcolour.blue = 0x0;
    XStoreColor (display, DefaultColormap (display, screen), &xcolour);

    xcolour.pixel = colours[3];
    xcolour.flags = DoRed | DoBlue | DoGreen;
    xcolour.red = 0x0;
    xcolour.green = 0x4000;
    xcolour.blue = 0x0;
    XStoreColor (display, DefaultColormap (display, screen), &xcolour);

  }
  /* load pixmap data */
  read_pixmap ();


  /* initialize size hints property for window managers */
  size_hints.flags = PPosition | PSize | PMinSize;
  size_hints.x = w_x;
  size_hints.y = w_y;
  size_hints.width = width;
  size_hints.height = height;
  size_hints.min_width = width;
  size_hints.min_height = height;

  /* set properties for window manager (always before mapping) */
  XSetStandardProperties (display, win, window_name, icon_name,
			  pm, argv, argc, &size_hints);

  /* Select Event types wanted */
  XSelectInput (display, win, ExposureMask | KeyPressMask | ButtonPressMask
		| StructureNotifyMask);

  /* Display window */
  XMapWindow (display, win);

  /* get events */
  /* use first exposure event to triger displaying window muck */

  while (1)
  {
    while (XCheckWindowEvent (display, win,
			      ExposureMask | KeyPressMask | ButtonPressMask
			      | StructureNotifyMask, &report))
    {
      switch (report.type)
      {
      case Expose:
	/* nuke any other exposure events which are waiting */
	XCopyArea (display, pm, win, gc,
		   report.xexpose.x,
		   report.xexpose.y,
		   report.xexpose.width,
		   report.xexpose.height,
		   report.xexpose.x,
		   report.xexpose.y);
	break;
      case KeyPress:
      case ButtonPress:
/*      XFreeGC(display,gc); 
   XCloseDisplay(display);
   exit(1);
 */
      default:
	break;
      }				/* end switch */
    }
    usleep (200000);
    check64net ();
    blink ();
  }				/* end while */

    return EXIT_SUCCESS;
}

int 
draw_drive ()
{
  /* draw 15xx looking drive */
  XCopyArea (display, pm, win, gc, 0, 0, width, height, 0, 0);
}

int 
read_pixmap ()
{
  int x, y, c, lc = -1;

  for (x = 0; x < 400; x++)
    for (y = 0; y < 150; y++)
    {
      c = p15xx_xpm[y + 7][x];
      if (c != lc)
      {
	lc = c;
	switch (c)
	{
	case ' ':
	  XSetForeground (display, gc, BlackPixel (display, screen));
	  break;
	case '+':
	  XSetForeground (display, gc, WhitePixel (display, screen));
	  break;
	case '.':
	  XSetForeground (display, gc, colours[0]);
	  break;
	case 'X':
	  XSetForeground (display, gc, colours[1]);
	  break;
	case 'o':
	  XSetForeground (display, gc, colours[2]);
	  break;
	case 'O':
	  XSetForeground (display, gc, colours[3]);
	  break;
	}
      }
      XDrawPoint (display, pm, gc, x, y);
    }

  return (0);
}

int 
blink ()
{
  /* update led status */

  blinkflag ^= 1;
  if (derror && blinkflag)
  {
    /* power led dim */
    xcolour.red = 0x4000;
  }
  else
  {
    /* power led bright */
    xcolour.red = 0xffff;
  }
  xcolour.pixel = colours[2];
  xcolour.flags = DoRed;
  XStoreColor (display, DefaultColormap (display, screen), &xcolour);

  if (active)
    xcolour.green = 0xffff;
  else
    xcolour.green = 0x4000;
  xcolour.pixel = colours[3];
  xcolour.flags = DoGreen;
  XStoreColor (display, DefaultColormap (display, screen), &xcolour);

}

int 
check64net ()
{
  /* check the $HOME/.64net2state file */
#ifndef LINUX
  fpurge (f);
#endif
  fseek (f, 0, SEEK_SET);
  derror = fgetc (f);
  active = fgetc (f);
}

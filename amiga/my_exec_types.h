#ifndef	EXEC_TYPES_H
#define	EXEC_TYPES_H
/*
**	$VER: types.h 40.1 (10.8.93)
**	Includes Release 40.15
**
**	Data typing.  Must be included before any other Amiga include.
**
**	(C) Copyright 1985-1993 Commodore-Amiga, Inc.
**	    All Rights Reserved
*/

/*
	Why the heck doesn't GCC like bits of this file!!!
	See bits marked with DJO
	*/

#define INCLUDE_VERSION	40 /* Version of the include files in use. (Do not
			      use this label for OpenLibrary() calls!) */


#define GLOBAL	extern	    /* the declaratory use of an external */
#define IMPORT	extern	    /* reference to an external */
#define STATIC	static	    /* a local static variable */
#define REGISTER register   /* a (hopefully) register variable */


#ifndef VOID
#define VOID		void
#endif


  /*  WARNING: APTR was redefined for the V36 Includes!  APTR is a   */
 /*  32-Bit Absolute Memory Pointer.  C pointer math will not	    */
/*  operate on APTR --	use "ULONG *" instead.			   */
#ifndef APTR_TYPEDEF
#define APTR_TYPEDEF
typedef void	       *APTR;	    /* 32-bit untyped pointer */
#endif
typedef long		LONG;	    /* signed 32-bit quantity */
typedef unsigned long	ULONG;	    /* unsigned 32-bit quantity */
/* typedef unsigned long	LONGBITS; Why doesn't GCC like this line?!?! DJO */   /* 32 bits manipulated individually */
typedef short		WORD;	    /* signed 16-bit quantity */
typedef unsigned short	UWORD;	    /* unsigned 16-bit quantity */
typedef unsigned short	WORDBITS;   /* 16 bits manipulated individually */
#if __STDC__
typedef signed char	BYTE;	    /* signed 8-bit quantity */
#else
typedef char		BYTE;	    /* signed 8-bit quantity */
#endif
typedef unsigned char	UBYTE;	    /* unsigned 8-bit quantity */
typedef unsigned char	BYTEBITS;   /* 8 bits manipulated individually */
typedef unsigned short	RPTR;	    /* signed relative pointer */

#ifdef __cplusplus
typedef char	       *STRPTR;     /* string pointer (NULL terminated) */
#else
typedef unsigned char  *STRPTR;     /* string pointer (NULL terminated) */
#endif


/* For compatibility only: (don't use in new code) */
typedef short		SHORT;	    /* signed 16-bit quantity (use WORD) */
typedef unsigned short	USHORT;     /* unsigned 16-bit quantity (use UWORD) */
typedef short		COUNT;
typedef unsigned short	UCOUNT;
typedef ULONG		CPTR;


/* Types with specific semantics */
typedef float		FLOAT;
typedef double		DOUBLE;
typedef short		BOOL;
typedef unsigned char	TEXT;

#ifndef TRUE
#define TRUE		1
#endif
#ifndef FALSE
#define FALSE		0
#endif
#ifndef NULL
#define NULL		0L
#endif


#define BYTEMASK	0xFF


 /* #define LIBRARY_VERSION is now obsolete.  Please use LIBRARY_MINIMUM */
/* or code the specific minimum library version you require.		*/
#define LIBRARY_MINIMUM	33 /* Lowest version supported by Commodore-Amiga */


#endif	/* EXEC_TYPES_H */

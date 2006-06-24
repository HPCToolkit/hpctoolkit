/* $Id$ */
/* -*-C-*- */
/* * BeginRiceCopyright *****************************************************
 * 
 * Copyright ((c)) 2002, Rice University 
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 
 * * Neither the name of Rice University (RICE) nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 * 
 * This software is provided by RICE and contributors "as is" and any
 * express or implied warranties, including, but not limited to, the
 * implied warranties of merchantability and fitness for a particular
 * purpose are disclaimed. In no event shall RICE or contributors be
 * liable for any direct, indirect, incidental, special, exemplary, or
 * consequential damages (including, but not limited to, procurement of
 * substitute goods or services; loss of use, data, or profits; or
 * business interruption) however caused and on any theory of liability,
 * whether in contract, strict liability, or tort (including negligence
 * or otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage. 
 * 
 * ******************************************************* EndRiceCopyright */

/************************************************************************/
/*									*/
/* (C/C++) Language variant independent definitions			*/
/*			      						*/
/************************************************************************/

/************************************************************************/
/*									*/
/*			      						*/
/* 									*/
/* This include file provides macros to be used in files (usually	*/
/* include files) that are to be compatible with different varieties of	*/
/* the C/C++ programming language.  Currently, Kernighan & Ritchie C,	*/
/* ANSI C, and C++ are supported.  Support has also been added for our	*/
/* own special lint wrapper, rnlint, to take advantage of the		*/
/* prototyping information in include files, even though during normal	*/
/* compilation of Kernighan & Ritchie C would normally ignore such	*/
/* information.								*/
/*								 	*/
/* The macros in this file deal mostly with handling function		*/
/* prototype information properly in each environment.  There are	*/
/* three situations were these macros should be used.  See below.  In	*/
/* all of these cases, arguments and types must be given, although	*/
/* such information is thrown away under some circumstances.		*/
/*								 	*/
/*								 	*/
/* EXTERN--defines an externally scoped function declaration.  All	*/
/* functions to be shared between different varieties of the C		*/
/* language must be declared with the EXTERN macro.  The macro		*/
/* takes three arguments.  The first argument is the return type of	*/
/* the function.  The second argument is the function name.  The	*/
/* second argument is a parenthesized list of comma-separated		*/
/* arguments, each preceded by its type.  For example, if you were	*/
/* to declare the function "max" that takes two integer arguments	*/
/* and returns an integer argument, you'd declare it as follows:	*/
/*								 	*/
/* 	extern int max  (int a, int b);				*/
/*								 	*/
/* If your function takes no arguments, EXTERN's third argument		*/
/* should be "(void)".  For example:					*/
/*								 	*/
/* 	extern int func1  (void);					*/
/*								 	*/
/* Note that if the function is declared with a variable number of	*/
/* arguments, the types and names of the fixed arguments must be	*/
/* listed and then a "..." should be given as the last element in the	*/
/* argument list.  There must be at least one fixed argument given.	*/
/* For example:								*/
/* 									*/
/* 	extern char* sprintf  (char *fmt, ...);			        */
/*								 	*/
/* 									*/
/* STATIC defines a statically scoped function declaration.		*/
/* Usually STATIC declarations occur in C files that might be		*/
/* compiled either with an ANSI C compiler or a Kernighan & Ritchie	*/
/* C compiler.  The STATIC declaration takes the place of a forward	*/
/* declaration of the a static function in that file.  When the		*/
/* function is actually defined, it is done with the K & R style	*/
/* mentioning the names of the arguments in parenthesis and a list	*/
/* of arguments and types following.  The STATIC macro is given the	*/
/* same arguments as EXTERN, above.  For example, say you have a	*/
/* static function for square root in a file.  You'd declare it		*/
/* early in the file as follows:					*/
/* 									*/
/* 	static double square_root  (double x);			        */
/* 									*/
/* Later, you'd define it:						*/
/* 									*/
/* 	static								*/
/* 	double square_root(x)						*/
/* 	double x;							*/
/* 	{								*/
/*		...							*/
/* 	}								*/
/* 									*/
/* 									*/
/* FUNCTION_POINTER defines a function pointer type with argument	*/
/* types.  This construct is most often used to define the types of	*/
/* callbacks or structures of functions.  If you are declaring		*/
/* function types, be sure you choose an obscure namespace for your	*/
/* type (just like global functions).  Also, suffix the type with the	*/
/* word "callback" or "func".  This macro takes three arguments, the	*/
/* first two of which are simply the function return type and		*/
/* function pointer type name separated by a comma.  The third		*/
/* argument is analagous to the EXTERN macro, above.  For example:	*/
/* 									*/
/* 	typedef FUNCTION_POINTER(void, cfg_create_callback,		*/
/* 					(int a, int b));		*/
/* 									*/
/* The FUNCTION_POINTER() macro can be used directly in structures,	*/
/* but it is _considerably_ less useful as it prevents the client	*/
/* from casting function to your type for the sake of argument type	*/
/* casting:								*/
/* 									*/
/* 	---Undesirable usage---						*/
/* 	typedef struct cfg_client_struct {				*/
/* 		Generic     handle;					*/
/* 		FUNCTION_POINTER(void, create_instance,			*/
/* 				(Generic handle, CfgInstance cfg));	*/
/* 		FUNCTION_POINTER(void, destroy_instance,		*/
/* 				(Generic handle, CfgInstance cfg));	*/
/* 		FUNCTION_POINTER(void, dump_instance,			*/
/* 				(Generic handle, CfgInstance cfg));	*/
/* 		struct cfg_client_struct *next;				*/
/* 	} *CfgClient;							*/
/* 									*/
/************************************************************************/

#ifndef LangVarIndDefs_h
#define LangVarIndDefs_h

/****************************************************************************/

#ifdef NO_STD_CHEADERS
# include <stddef.h>
# include <stdlib.h>
#else
# include <cstddef> // for 'NULL'
# include <cstdlib>
using std::abort; // For compatibility with non-std C headers
using std::exit;
#endif

/****************************************************************************/

/* Commonly used types */

/* TraversalOrder: used for a number of iterators.  IMO, it would be
   preferable to use (class) namespaces and have separate types as
   necessary, but it is not worth the effort to change at the
   moment. */
typedef enum { Unordered, PreOrder, PostOrder,
	       ReversePreOrder, ReversePostOrder, 
	       PreAndPostOrder }
  TraversalOrder;

/****************************************************************************/

/* Commonly used function definitions and Macros */

#undef MIN
#undef MAX
#define	MIN(a,b) (((a) < (b)) ? (a) : (b))
#define	MAX(a,b) (((a) > (b)) ? (a) : (b))

/****************************************************************************/

#ifdef __cplusplus

/* C++ Definitions */

#define EXTERN(rettype, name, arglist) extern "C" { rettype name arglist; }
#define STATIC(rettype, name, arglist) static rettype name arglist
#define FUNCTION_POINTER(rettype, name, arglist) rettype(*name)arglist

#elif __STDC__

/* ANSI C Definitions */

#define EXTERN(rettype, name, arglist) rettype name arglist
#define STATIC(rettype, name, arglist) static rettype name arglist
#define FUNCTION_POINTER(rettype, name, arglist) rettype(*name)arglist

#else

/* Kernighan & Ritchie  C Definitions */

#ifdef RN_LINT_LIBRARY /* RN_LINT_LIBRARY */
#define EXTERN(rettype, name, arglist) rettype name ~ arglist ~
#else
#define EXTERN(rettype, name, arglist) rettype name()
#endif /* RN_LINT_LIBRARY */
 
#define STATIC(rettype, name, arglist) static rettype name()
#define FUNCTION_POINTER(rettype, name, arglist) rettype(*name)()
 
#endif
 
    /***** OBSOLETE--do not use in new stuff (K & R C only!) ******/

/* 
   typedef FUNCTION_POINTER (void, PFV, ());
   typedef FUNCTION_POINTER (int, PFI, ());
   typedef FUNCTION_POINTER (char*, PFS, ());
*/

/****************************************************************************/

#endif  /* LangVarIndDefs_h */

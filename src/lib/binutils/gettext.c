//
// Copyright ((c)) 2002-2020, Rice University
// All rights reserved.
//
// See file README.License for details.
//
//----------------------------------------------------------------------
//
// This file provides a stub version of the gettext functions for
// lib/binutils.  libbfd doesn't actually use these functions, but it
// generates undefined references.
//
// Compile this as a separate library to get the order right.
//
// Note: add the library to the programs that use binutils (hpcstruct,
// etc), not binutils itself.
//
//----------------------------------------------------------------------
//

#include <stdio.h>

#define GETTEXT_DEBUG  0

char *
libintl_dgettext(char * domain, char * mesgid)
{
#if GETTEXT_DEBUG
  fprintf(stderr, "libintl_dgettext: mesgid = '%s'\n", mesgid);
#endif

  return mesgid;
}


char *
libintl_dngettext(char * domain, char * mesgid1, char * mesgid2, long n)
{
#if GETTEXT_DEBUG
  fprintf(stderr, "libintl_dgettext: mesgid = '%s'\n", mesgid1);
#endif

  return mesgid1;
}

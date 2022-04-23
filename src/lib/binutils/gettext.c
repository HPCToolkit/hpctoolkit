// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2022, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

//
// Copyright ((c)) 2002-2022, Rice University
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

#define GETTEXT_DEBUG 0

char* libintl_dgettext(char* domain, char* mesgid) {
#if GETTEXT_DEBUG
  fprintf(stderr, "libintl_dgettext: mesgid = '%s'\n", mesgid);
#endif

  return mesgid;
}

char* libintl_dngettext(char* domain, char* mesgid1, char* mesgid2, long n) {
#if GETTEXT_DEBUG
  fprintf(stderr, "libintl_dgettext: mesgid = '%s'\n", mesgid1);
#endif

  return mesgid1;
}

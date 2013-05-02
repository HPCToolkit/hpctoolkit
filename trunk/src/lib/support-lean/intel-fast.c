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
// Copyright ((c)) 2002-2013, Rice University
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

// This file contains default, weak definitions of some Intel fast
// string functions.  The latest XED for MIC (rev 58423) is compiled
// with Intel icc and contains references to some optimized Intel
// string functions.  The Intel compiler would supply these functions
// automatically, but they result in undefined references when linked
// with GNU gcc.
//
// Define these wrappers as weak symbols so as not to conflict with
// the Intel functions if compiled with icc.
//
// Any HPCToolkit application that uses XED should include
// libHPCsupport-lean.la to work with the latest XED.  See the
// fnbounds Makefile.am for how to do this.

//***************************************************************************

#include <sys/types.h>
#include <string.h>


int __attribute__ ((weak))
_intel_fast_memcmp(const void *s1, const void *s2, size_t n)
{
  return memcmp(s1, s2, n);
}


void * __attribute__ ((weak))
_intel_fast_memcpy(void *dest, const void *src, size_t n)
{
  return memcpy(dest, src, n);
}


size_t __attribute__ ((weak))
__intel_sse2_strlen(const char *s)
{
  return strlen(s);
}


char * __attribute__ ((weak))
__intel_sse2_strncat(char *dest, const char *src, size_t n)
{
  return strncat(dest, src, n);
}

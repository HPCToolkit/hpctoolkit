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
// Copyright ((c)) 2002-2024, Rice University
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

#include "foil.h"
#include "../sample-sources/memleak-overrides.h"
#include <unistd.h>
#include <malloc.h>

extern typeof(memalign) __libc_memalign;
extern typeof(valloc) __libc_valloc;
extern typeof(malloc) __libc_malloc;
extern typeof(free) __libc_free;
extern typeof(realloc) __libc_realloc;

HPCRUN_EXPOSED int posix_memalign(void **memptr, size_t alignment, size_t bytes) {
  LOOKUP_FOIL_BASE(base, posix_memalign);
  return base(__libc_memalign, __libc_malloc, memptr, alignment, bytes);
}

HPCRUN_EXPOSED void *memalign(size_t boundary, size_t bytes) {
  LOOKUP_FOIL_BASE(base, memalign);
  return base(__libc_memalign, __libc_malloc, boundary, bytes);
}

HPCRUN_EXPOSED void *valloc(size_t bytes) {
  LOOKUP_FOIL_BASE(base, valloc);
  return base(__libc_memalign, __libc_malloc, __libc_valloc, bytes);
}

HPCRUN_EXPOSED void *malloc(size_t bytes) {
  LOOKUP_FOIL_BASE(base, malloc);
  return base(__libc_memalign, __libc_malloc, bytes);
}

HPCRUN_EXPOSED void *calloc(size_t nmemb, size_t bytes) {
  LOOKUP_FOIL_BASE(base, calloc);
  return base(__libc_memalign, __libc_malloc, nmemb, bytes);
}

HPCRUN_EXPOSED void free(void *ptr) {
  LOOKUP_FOIL_BASE(base, free);
  return base(__libc_free, ptr);
}

HPCRUN_EXPOSED void *realloc(void *ptr, size_t bytes) {
  LOOKUP_FOIL_BASE(base, realloc);
  return base(__libc_memalign, __libc_malloc, __libc_realloc, __libc_free, ptr, bytes);
}

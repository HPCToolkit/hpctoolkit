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


#ifndef __MEMLEAK_OVERRIDES_H__
#define __MEMLEAK_OVERRIDES_H__

#include <stddef.h>

typedef void *memalign_fcn(size_t, size_t);
typedef void *valloc_fcn(size_t);
typedef void *malloc_fcn(size_t);
typedef void  free_fcn(void *);
typedef void *realloc_fcn(void *, size_t);

extern int foilbase_posix_memalign(memalign_fcn*, malloc_fcn*, void **memptr, size_t alignment, size_t bytes);
extern void *foilbase_memalign(memalign_fcn*, malloc_fcn*, size_t boundary, size_t bytes);
extern void *foilbase_valloc(memalign_fcn*, malloc_fcn*, valloc_fcn*, size_t bytes);
extern void *foilbase_malloc(memalign_fcn*, malloc_fcn*, size_t bytes);
extern void *foilbase_calloc(memalign_fcn*, malloc_fcn*, size_t nmemb, size_t bytes);
extern void foilbase_free(free_fcn*, void *ptr);
extern void *foilbase_realloc(memalign_fcn*, malloc_fcn*, realloc_fcn*, free_fcn*, void *ptr, size_t bytes);

#endif

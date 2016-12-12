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
// Copyright ((c)) 2002-2012, Rice University
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

#include <sys/mman.h>
#include <unistd.h>

#include "thread_data.h"
#include "memory-overrides.h"

void
first_touch(void *sys_ptr, mem_info_t *info_ptr)
{
  // support for first touch analysis

  // Change the permission for the newly allocated memory ranges (rounded to 
  // the page boundary). Will segfault at the first touch of the memory range.
  int pagesize = getpagesize();
  // round to the upper-bound of the next page
  void *p = (void *)(((uint64_t)(uintptr_t) sys_ptr + pagesize-1) & ~(pagesize-1));
  // monitor all pages of the allocation (expect the page of splay section)
  uint64_t msize = (uint64_t)(uintptr_t) info_ptr-(uint64_t)(uintptr_t) p - pagesize;
  if (msize <= 0 ) msize = pagesize - 1;
  mprotect (p, msize, PROT_NONE);
  // change the splay node section to be read/write.
  // make sure pages with the splay section have correct permissions
  p = (void *)((uint64_t)(uintptr_t) info_ptr & ~(pagesize - 1));
  int mem_info_size = sizeof(struct mem_info_s);
  mprotect (p, (uint64_t)(uintptr_t) info_ptr + mem_info_size - (uint64_t)(uintptr_t) p, PROT_READ | PROT_WRITE);
}

sample_val_t
data_sample_callpath(ucontext_t *uc, size_t bytes)
{
  TD_GET(mem_data.in_malloc) = 1;
  sample_val_t smpl =
    hpcrun_sample_callpath(uc, hpcrun_datacentric_alloc_id(), bytes, 0, 1);
  TD_GET(mem_data.in_malloc) = 0;
  return smpl;
}

// ----------------------------------------------
// API has to be implemented by external plugin
// ----------------------------------------------

void 
mo_external_init()
{
  mem_registry_t mem_item;
  mem_item.byte_threshold = 1024 * 8;
  mem_item.action_fcn     = first_touch;
  mem_item.sample_fcn	  = data_sample_callpath;

  add_mem_registry(mem_item);
}


int 
mo_external_active()
{
  return hpcrun_datacentric_active();
}


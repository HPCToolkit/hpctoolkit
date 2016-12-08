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
// Copyright ((c)) 2002-2016, Rice University
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

// MEMLEAK overrides the malloc family of functions and provides two
// metrics: number of bytes allocated and number of bytes freed per
// dynamic context.  Subtracting these two values is a way to find
// memory leaks.
//
// Override functions:
// posix_memalign, memalign, valloc
// malloc, calloc, free, realloc

/******************************************************************************
 * standard include files
 *****************************************************************************/

/******************************************************************************
 * local include files
 *****************************************************************************/
#include "memory-overrides.h"
#include "memleak.h"


sample_val_t 
callpath_sample(ucontext_t * uc, size_t bytes)
{
  return hpcrun_sample_callpath(uc, hpcrun_memleak_alloc_id(), bytes, 0, 1);
}

void 
mo_external_init()
{
  mem_registry_t mem_item;
  mem_item.byte_threshold = 1;
  mem_item.action_fcn     = NULL;
  mem_item.sample_fcn     = callpath_sample;
  add_mem_registry(mem_item);
}



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

// This overrides the malloc family of functions and provides two
// metrics: number of bytes allocated and number of bytes freed per
// dynamic context.  
//
// Override functions:
// posix_memalign, memalign, valloc
// malloc, calloc, free, realloc

#ifndef __MEMORY_OVERRIDES_H__
#define __MEMORY_OVERRIDES_H__

#include <stdlib.h>
#include "sample_event.h"
#include "cct_insert_backtrace.h"

typedef struct mem_info_s {
  long magic;
  cct_node_t *context;
  size_t bytes;
  void *memblock;
  void *rmemblock;
  struct mem_info_s *left;
  struct mem_info_s *right;
} mem_info_t;

typedef void mem_action_fcn(void *, mem_info_t *);
typedef sample_val_t sample_callpath(ucontext_t *, size_t);

typedef struct mem_registry_s {
  // Interface to add restriction for the minimum threshold
  // of allocated bytes we want to collect
  // If the allocation function allocates less than the threshold, 
  // we will not collect the samples
  size_t byte_threshold;
  
  mem_action_fcn  *action_fcn;
  sample_callpath *sample_fcn;

} mem_registry_t;

int add_mem_registry(mem_registry_t mem_item);

// ----------------------------------------------
// API has to be implemented by external plugin
// ----------------------------------------------

// function needs to be implemented by the plugin
// this function will be called by memory-overrides.c during the initialiation

void mo_external_init();

// function needs to be implemented by the plugin
// this function returns true if the external is active.
//  false otherwise

int mo_external_active();

int get_free_metric_id();

#endif

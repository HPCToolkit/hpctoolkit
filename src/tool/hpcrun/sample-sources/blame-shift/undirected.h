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

#ifndef __undirected_h__
#define __undirected_h__

#include "hpcrun/cct/cct.h"

#include "lib/prof-lean/stdatomic.h"

typedef int* (*undirected_idle_cnt_ptr_fn)(void);
typedef _Bool (*undirected_predicate_fn)(void);

typedef struct undirected_blame_info_t {
  atomic_uintptr_t active_worker_count;
  atomic_uintptr_t total_worker_count;

  undirected_idle_cnt_ptr_fn get_idle_count_ptr;
  undirected_predicate_fn participates;
  undirected_predicate_fn working;

  int work_metric_id;
  int idle_metric_id;

  int levels_to_skip;
} undirected_blame_info_t;

void undirected_blame_thread_start(undirected_blame_info_t* bi);

void undirected_blame_thread_end(undirected_blame_info_t* bi);

void undirected_blame_idle_begin(undirected_blame_info_t* bi);

void undirected_blame_idle_end(undirected_blame_info_t* bi);

void undirected_blame_sample(void* arg, int metric_id, cct_node_t* node, int metric_incr);

#endif  // __undirected_h__

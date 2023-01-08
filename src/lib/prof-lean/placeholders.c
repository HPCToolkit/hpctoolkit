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

#include "placeholders.h"

//*****************************************************************************
// global includes
//*****************************************************************************

#include <hpctoolkit-config.h>

#include <stdlib.h>

//*****************************************************************************
// local variables
//*****************************************************************************

static const char phname_program_root[] = "<program root>";
static const char phname_thread_root[]  = "<thread root>";

static const char phname_omp_idle[]              = "<omp idle>";
static const char phname_omp_overhead[]          = "<omp overhead>";
static const char phname_omp_barrier_wait[]      = "<omp barrier wait>";
static const char phname_omp_task_wait[]         = "<omp task wait>";
static const char phname_omp_mutex_wait[]        = "<omp mutex wait>";
static const char phname_omp_region_unresolved[] = "<omp region unresolved>";

static const char phname_omp_tgt_copyin[]  = "<omp tgt copyin>";
static const char phname_omp_tgt_copyout[] = "<omp tgt copyout>";
static const char phname_omp_tgt_alloc[]   = "<omp tgt alloc>";
static const char phname_omp_tgt_delete[]  = "<omp tgt delete>";
static const char phname_omp_tgt_kernel[]  = "<omp tgt kernel>";

static const char phname_gpu_copy[]    = "<gpu copy>";
static const char phname_gpu_copyin[]  = "<gpu copyin>";
static const char phname_gpu_copyout[] = "<gpu copyout>";
static const char phname_gpu_alloc[]   = "<gpu alloc>";
static const char phname_gpu_delete[]  = "<gpu delete>";
static const char phname_gpu_sync[]    = "<gpu sync>";
static const char phname_gpu_kernel[]  = "<gpu kernel>";
static const char phname_gpu_memset[]  = "<gpu memset>";

//*****************************************************************************
// interface operations
//*****************************************************************************

const char *
get_placeholder_name(uint64_t placeholder) {
  // Cast to enum to generate warnings when cases aren't covered
  switch((enum hpcrun_placeholder)placeholder) {
  case hpcrun_placeholder_unnormalized_ip:
  case hpcrun_placeholder_root_primary:
  case hpcrun_placeholder_root_partial:
  case hpcrun_placeholder_no_activity:
    // These are special cases among the special cases
    return NULL;
  case hpcrun_placeholder_fence_main:
    return phname_program_root;
  case hpcrun_placeholder_fence_thread:
    return phname_thread_root;
  case hpcrun_placeholder_ompt_idle_state:
    return phname_omp_idle;
  case hpcrun_placeholder_ompt_overhead_state:
    return phname_omp_overhead;
  case hpcrun_placeholder_ompt_barrier_wait_state:
    return phname_omp_barrier_wait;
  case hpcrun_placeholder_ompt_task_wait_state:
    return phname_omp_task_wait;
  case hpcrun_placeholder_ompt_mutex_wait_state:
    return phname_omp_mutex_wait;
  case hpcrun_placeholder_gpu_alloc:
    return phname_gpu_alloc;
  case hpcrun_placeholder_ompt_tgt_alloc:
    return phname_omp_tgt_alloc;
  case hpcrun_placeholder_gpu_delete:
    return phname_gpu_delete;
  case hpcrun_placeholder_ompt_tgt_delete:
    return phname_omp_tgt_delete;
  case hpcrun_placeholder_gpu_copyin:
    return phname_gpu_copyin;
  case hpcrun_placeholder_ompt_tgt_copyin:
    return phname_omp_tgt_copyin;
  case hpcrun_placeholder_gpu_copyout:
    return phname_gpu_copyout;
  case hpcrun_placeholder_ompt_tgt_copyout:
    return phname_omp_tgt_copyout;
  case hpcrun_placeholder_gpu_kernel:
    return phname_gpu_kernel;
  case hpcrun_placeholder_ompt_tgt_kernel:
    return phname_omp_tgt_kernel;
  case hpcrun_placeholder_gpu_copy:
    return phname_gpu_copy;
  case hpcrun_placeholder_gpu_memset:
    return phname_gpu_memset;
  case hpcrun_placeholder_gpu_sync:
    return phname_gpu_sync;
  case hpcrun_placeholder_gpu_trace:
    return phname_gpu_kernel;
  case hpcrun_placeholder_ompt_tgt_none:
    // Not in NameMappings.cpp
    return NULL;
  case hpcrun_placeholder_ompt_region_unresolved:
    return phname_omp_region_unresolved;
  }
  return NULL;
}

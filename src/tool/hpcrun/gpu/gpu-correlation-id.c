// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2020, Rice University
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

//******************************************************************************
// local includes
//******************************************************************************

#include <lib/prof-lean/stdatomic.h>
#include <lib/prof-lean/spinlock.h>

#include "gpu-correlation-id.h"



//******************************************************************************
// macros
//******************************************************************************

#define DEBUG 0

#define GPU_RANGE_COUNT_LIMIT 2000

#define GPU_CORRELATION_ID_START 0x8000000000000001

#define GPU_CORRELATION_ID_MASK 0x7FFFFFFFFFFFFFFF

#define GPU_CORRELATION_ID_UNMASK(x) (x & GPU_CORRELATION_ID_MASK)

#include "gpu-print.h"


//******************************************************************************
// private data
//******************************************************************************

static atomic_ullong gpu_correlation_id_value = 
  ATOMIC_VAR_INIT(GPU_CORRELATION_ID_START);
static atomic_uint thread_id = ATOMIC_VAR_INIT(1);
static atomic_uint thread_id_lead = ATOMIC_VAR_INIT(0);
static atomic_bool wait = ATOMIC_VAR_INIT(false);

static __thread uint64_t thread_range_id = 0;
static __thread uint32_t thread_id_local = 0;

static spinlock_t count_lock = SPINLOCK_UNLOCKED;

//******************************************************************************
// interface operations
//******************************************************************************

uint64_t 
gpu_correlation_id
(
 void
)
{
  uint64_t correlation_id = atomic_fetch_add(&gpu_correlation_id_value, 1);
  return correlation_id;
}


uint64_t
gpu_range_id
(
)
{
  return thread_range_id;
}


void
gpu_range_enter
(
 uint64_t correlation_id
)
{
  thread_range_id = (GPU_CORRELATION_ID_UNMASK(correlation_id) - 1) / GPU_RANGE_COUNT_LIMIT;

  if (thread_id_local == 0) {
    thread_id_local = atomic_fetch_add(&thread_id, 1);
  }

  spinlock_lock(&count_lock);

  if (GPU_CORRELATION_ID_UNMASK(correlation_id) % GPU_RANGE_COUNT_LIMIT == 0) {
    // The last call in this range
    atomic_store(&thread_id_lead, thread_id_local);
    atomic_store(&wait, true);
  }

  spinlock_unlock(&count_lock);

  while (atomic_load(&thread_id_lead) != thread_id_local && atomic_load(&wait) == true) {}
}


void
gpu_range_exit
(
)
{
  if (gpu_range_is_lead()) {
    atomic_store(&thread_id_lead, 0);
    atomic_store(&wait, false);
  }
}


bool
gpu_range_is_lead
(
)
{
  return atomic_load(&thread_id_lead) == thread_id_local;
}

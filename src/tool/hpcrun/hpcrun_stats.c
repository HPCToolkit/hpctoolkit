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
// Copyright ((c)) 2002-2017, Rice University
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


//***************************************************************************
// local include files
//***************************************************************************
#include "sample_event.h"
#include "disabled.h"

#include <memory/hpcrun-malloc.h>
#include <messages/messages.h>

#include <lib/prof-lean/stdatomic.h>
#include <lib/prof-lean/hpcrun-fmt.h>
#include <unwind/common/validate_return_addr.h>


//***************************************************************************
// local variables
//***************************************************************************

static atomic_long num_samples_total = ATOMIC_VAR_INIT(0);
static atomic_long num_samples_attempted = ATOMIC_VAR_INIT(0);
static atomic_long num_samples_blocked_async = ATOMIC_VAR_INIT(0);
static atomic_long num_samples_blocked_dlopen = ATOMIC_VAR_INIT(0);
static atomic_long num_samples_dropped = ATOMIC_VAR_INIT(0);
static atomic_long num_samples_segv = ATOMIC_VAR_INIT(0);
static atomic_long num_samples_partial = ATOMIC_VAR_INIT(0);
static atomic_long num_samples_yielded = ATOMIC_VAR_INIT(0);

static atomic_long num_unwind_intervals_total = ATOMIC_VAR_INIT(0);
static atomic_long num_unwind_intervals_suspicious = ATOMIC_VAR_INIT(0);

static atomic_long trolled = ATOMIC_VAR_INIT(0);
static atomic_long frames_total = ATOMIC_VAR_INIT(0);
static atomic_long trolled_frames = ATOMIC_VAR_INIT(0);

//***************************************************************************
// interface operations
//***************************************************************************

void
hpcrun_stats_reinit(void)
{
  atomic_store_explicit(&num_samples_total, 0, memory_order_relaxed);
  atomic_store_explicit(&num_samples_attempted, 0, memory_order_relaxed);
  atomic_store_explicit(&num_samples_blocked_async, 0, memory_order_relaxed);
  atomic_store_explicit(&num_samples_blocked_dlopen, 0, memory_order_relaxed);
  atomic_store_explicit(&num_samples_dropped, 0, memory_order_relaxed);
  atomic_store_explicit(&num_samples_segv, 0, memory_order_relaxed);
  atomic_store_explicit(&num_unwind_intervals_total, 0, memory_order_relaxed);
  atomic_store_explicit(&num_unwind_intervals_suspicious, 0, memory_order_relaxed);
  atomic_store_explicit(&trolled, 0, memory_order_relaxed);
  atomic_store_explicit(&frames_total, 0, memory_order_relaxed);
  atomic_store_explicit(&trolled_frames, 0, memory_order_relaxed);
}


//-----------------------------
// samples total 
//-----------------------------

void
hpcrun_stats_num_samples_total_inc(void)
{
  atomic_fetch_add_explicit(&num_samples_total, 1L, memory_order_relaxed);
}


long
hpcrun_stats_num_samples_total(void)
{
  return atomic_load_explicit(&num_samples_total, memory_order_relaxed);
}



//-----------------------------
// samples attempted 
//-----------------------------

void
hpcrun_stats_num_samples_attempted_inc(void)
{
  atomic_fetch_add_explicit(&num_samples_attempted, 1L, memory_order_relaxed);
}


long
hpcrun_stats_num_samples_attempted(void)
{
  return atomic_load_explicit(&num_samples_attempted, memory_order_relaxed);
}



//-----------------------------
// samples blocked async 
//-----------------------------

// The async blocks happen in the signal handlers, without getting to
// hpcrun_sample_callpath, so also increment the total count here.
void
hpcrun_stats_num_samples_blocked_async_inc(void)
{
  atomic_fetch_add_explicit(&num_samples_blocked_async, 1L, memory_order_relaxed);
  atomic_fetch_add_explicit(&num_samples_total, 1L, memory_order_relaxed);
}


long
hpcrun_stats_num_samples_blocked_async(void)
{
  return atomic_load_explicit(&num_samples_blocked_async, memory_order_relaxed);
}



//-----------------------------
// samples blocked dlopen 
//-----------------------------

void
hpcrun_stats_num_samples_blocked_dlopen_inc(void)
{
  atomic_fetch_add_explicit(&num_samples_blocked_dlopen, 1L, memory_order_relaxed);
}


long
hpcrun_stats_num_samples_blocked_dlopen(void)
{
  return atomic_load_explicit(&num_samples_blocked_dlopen, memory_order_relaxed);
}



//-----------------------------
// samples dropped
//-----------------------------

void
hpcrun_stats_num_samples_dropped_inc(void)
{
  atomic_fetch_add_explicit(&num_samples_dropped, 1L, memory_order_relaxed);
}

long
hpcrun_stats_num_samples_dropped(void)
{
  return atomic_load_explicit(&num_samples_dropped, memory_order_relaxed);
}

//----------------------------
// partial unwinds
//----------------------------

void
hpcrun_stats_num_samples_partial_inc(void)
{
  atomic_fetch_add_explicit(&num_samples_partial, 1L, memory_order_relaxed);
}

long
hpcrun_stats_num_samples_partial(void)
{
  return atomic_load_explicit(&num_samples_partial, memory_order_relaxed);
}

//-----------------------------
// samples segv
//-----------------------------

void
hpcrun_stats_num_samples_segv_inc(void)
{
  atomic_fetch_add_explicit(&num_samples_segv, 1L, memory_order_relaxed);
}


long
hpcrun_stats_num_samples_segv(void)
{
  return atomic_load_explicit(&num_samples_segv, memory_order_relaxed);
}




//-----------------------------
// unwind intervals total
//-----------------------------

void
hpcrun_stats_num_unwind_intervals_total_inc(void)
{
  atomic_fetch_add_explicit(&num_unwind_intervals_total, 1L, memory_order_relaxed);
}


long
hpcrun_stats_num_unwind_intervals_total(void)
{
  return atomic_load_explicit(&num_unwind_intervals_total, memory_order_relaxed);
}



//-----------------------------
// unwind intervals suspicious
//-----------------------------

void
hpcrun_stats_num_unwind_intervals_suspicious_inc(void)
{
  atomic_fetch_add_explicit(&num_unwind_intervals_suspicious, 1L, memory_order_relaxed);
}


long
hpcrun_stats_num_unwind_intervals_suspicious(void)
{
  return atomic_load_explicit(&num_unwind_intervals_suspicious, memory_order_relaxed);
}

//------------------------------------------------------
// samples that include 1 or more successful troll steps
//------------------------------------------------------

void
hpcrun_stats_trolled_inc(void)
{
  atomic_fetch_add_explicit(&trolled, 1L, memory_order_relaxed);
}

long
hpcrun_stats_trolled(void)
{
  return atomic_load_explicit(&trolled, memory_order_relaxed);
}

//------------------------------------------------------
// total number of (unwind) frames in sample set
//------------------------------------------------------

void
hpcrun_stats_frames_total_inc(long amt)
{
  atomic_fetch_add_explicit(&frames_total, amt, memory_order_relaxed);
}

long
hpcrun_stats_frames_total(void)
{
  return atomic_load_explicit(&frames_total, memory_order_relaxed);
}

//---------------------------------------------------------------------
// total number of (unwind) frames in sample set that employed trolling
//---------------------------------------------------------------------

void
hpcrun_stats_trolled_frames_inc(long amt)
{
  atomic_fetch_add_explicit(&trolled_frames, amt, memory_order_relaxed);
}

long
hpcrun_stats_trolled_frames(void)
{
  return atomic_load_explicit(&trolled_frames, memory_order_relaxed);
}

//----------------------------
// samples yielded due to deadlock prevention
//----------------------------

void
hpcrun_stats_num_samples_yielded_inc(void)
{
  atomic_fetch_add_explicit(&num_samples_yielded, 1L, memory_order_relaxed);
}

long
hpcrun_stats_num_samples_yielded(void)
{
  return atomic_load_explicit(&num_samples_yielded, memory_order_relaxed);
}

//-----------------------------
// print summary
//-----------------------------

void
hpcrun_stats_print_summary(void)
{
  long blocked = atomic_load_explicit(&num_samples_blocked_async, memory_order_relaxed) +
    atomic_load_explicit(&num_samples_blocked_dlopen, memory_order_relaxed);
  long errant = atomic_load_explicit(&num_samples_dropped, memory_order_relaxed);
  long soft = atomic_load_explicit(&num_samples_dropped, memory_order_relaxed) -
    atomic_load_explicit(&num_samples_segv, memory_order_relaxed);
  long valid = atomic_load_explicit(&num_samples_attempted, memory_order_relaxed);
  if (ENABLED(NO_PARTIAL_UNW)) {
    valid = atomic_load_explicit(&num_samples_attempted, memory_order_relaxed) - errant;
  }

  hpcrun_memory_summary();

  AMSG("SAMPLE ANOMALIES: blocks: %ld (async: %ld, dlopen: %ld), "
       "errors: %ld (segv: %ld, soft: %ld)",
       blocked, num_samples_blocked_async, num_samples_blocked_dlopen,
       errant, num_samples_segv, soft);

  AMSG("SUMMARY: samples: %ld (recorded: %ld, blocked: %ld, errant: %ld, trolled: %ld, yielded: %ld),\n"
       "         frames: %ld (trolled: %ld)\n"
       "         intervals: %ld (suspicious: %ld)",
       num_samples_total, valid, blocked, errant, trolled, num_samples_yielded,
       frames_total, trolled_frames,
       num_unwind_intervals_total,  num_unwind_intervals_suspicious);

  if (hpcrun_get_disabled()) {
    AMSG("SAMPLING HAS BEEN DISABLED");
  }

  // logs, retentions || adj.: recorded, retained, written

  if (ENABLED(UNW_VALID)) {
    hpcrun_validation_summary();
  }
}


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


//***************************************************************************
// local include files
//***************************************************************************
#include "sample_event.h"
#include "disabled.h"

#include <memory/hpcrun-malloc.h>
#include <messages/messages.h>

#include <lib/prof-lean/atomic-op.h>
#include <lib/prof-lean/hpcrun-fmt.h>
#include <unwind/common/validate_return_addr.h>


//***************************************************************************
// local variables
//***************************************************************************

static long num_samples_total = 0;
static long num_samples_attempted = 0;
static long num_samples_blocked_async = 0;
static long num_samples_blocked_dlopen = 0;
static long num_samples_dropped = 0;
static long num_samples_segv = 0;
static long num_samples_partial = 0;
static long num_samples_yielded = 0;

static long num_unwind_intervals_total = 0;
static long num_unwind_intervals_suspicious = 0;

static long trolled = 0;
static long frames_total = 0;
static long trolled_frames = 0;

//***************************************************************************
// interface operations
//***************************************************************************

void
hpcrun_stats_reinit(void)
{
  num_samples_total = 0;
  num_samples_attempted = 0;
  num_samples_blocked_async = 0;
  num_samples_blocked_dlopen = 0;
  num_samples_dropped = 0;
  num_samples_segv = 0;
  num_unwind_intervals_total = 0;
  num_unwind_intervals_suspicious = 0;
  trolled = 0;
  frames_total = 0;
  trolled_frames = 0;
}


//-----------------------------
// samples total 
//-----------------------------

void
hpcrun_stats_num_samples_total_inc(void)
{
  atomic_add_i64(&num_samples_total, 1L);
}


long
hpcrun_stats_num_samples_total(void)
{
  return num_samples_total;
}



//-----------------------------
// samples attempted 
//-----------------------------

void
hpcrun_stats_num_samples_attempted_inc(void)
{
  atomic_add_i64(&num_samples_attempted, 1L);
}


long
hpcrun_stats_num_samples_attempted(void)
{
  return num_samples_attempted;
}



//-----------------------------
// samples blocked async 
//-----------------------------

// The async blocks happen in the signal handlers, without getting to
// hpcrun_sample_callpath, so also increment the total count here.
void
hpcrun_stats_num_samples_blocked_async_inc(void)
{
  atomic_add_i64(&num_samples_blocked_async, 1L);
  atomic_add_i64(&num_samples_total, 1L);
}


long
hpcrun_stats_num_samples_blocked_async(void)
{
  return num_samples_blocked_async;
}



//-----------------------------
// samples blocked dlopen 
//-----------------------------

void
hpcrun_stats_num_samples_blocked_dlopen_inc(void)
{
  atomic_add_i64(&num_samples_blocked_dlopen, 1L);
}


long
hpcrun_stats_num_samples_blocked_dlopen(void)
{
  return num_samples_blocked_dlopen;
}



//-----------------------------
// samples dropped
//-----------------------------

void
hpcrun_stats_num_samples_dropped_inc(void)
{
  atomic_add_i64(&num_samples_dropped, 1L);
}

long
hpcrun_stats_num_samples_dropped(void)
{
  return num_samples_dropped;
}

//----------------------------
// partial unwinds
//----------------------------

void
hpcrun_stats_num_samples_partial_inc(void)
{
  atomic_add_i64(&num_samples_partial, 1L);
}

long
hpcrun_stats_num_samples_partial(void)
{
  return num_samples_partial;
}

//-----------------------------
// samples segv
//-----------------------------

void
hpcrun_stats_num_samples_segv_inc(void)
{
  atomic_add_i64(&num_samples_segv, 1L);
}


long
hpcrun_stats_num_samples_segv(void)
{
  return num_samples_segv;
}




//-----------------------------
// unwind intervals total
//-----------------------------

void
hpcrun_stats_num_unwind_intervals_total_inc(void)
{
  atomic_add_i64(&num_unwind_intervals_total, 1L);
}


long
hpcrun_stats_num_unwind_intervals_total(void)
{
  return num_unwind_intervals_total;
}



//-----------------------------
// unwind intervals suspicious
//-----------------------------

void
hpcrun_stats_num_unwind_intervals_suspicious_inc(void)
{
  atomic_add_i64(&num_unwind_intervals_suspicious, 1L);
}


long
hpcrun_stats_num_unwind_intervals_suspicious(void)
{
  return num_unwind_intervals_suspicious;
}

//------------------------------------------------------
// samples that include 1 or more successful troll steps
//------------------------------------------------------

void
hpcrun_stats_trolled_inc(void)
{
  atomic_add_i64(&trolled, 1L);
}

long
hpcrun_stats_trolled(void)
{
  return trolled;
}

//------------------------------------------------------
// total number of (unwind) frames in sample set
//------------------------------------------------------

void
hpcrun_stats_frames_total_inc(long amt)
{
  atomic_add_i64(&frames_total, amt);
}

long
hpcrun_stats_frames_total(void)
{
  return frames_total;
}

//---------------------------------------------------------------------
// total number of (unwind) frames in sample set that employed trolling
//---------------------------------------------------------------------

void
hpcrun_stats_trolled_frames_inc(long amt)
{
  atomic_add_i64(&trolled_frames, amt);
}

long
hpcrun_stats_trolled_frames(void)
{
  return trolled_frames;
}

//----------------------------
// samples yielded due to deadlock prevention
//----------------------------

void
hpcrun_stats_num_samples_yielded_inc(void)
{
  atomic_add_i64(&num_samples_yielded, 1L);
}

long
hpcrun_stats_num_samples_yielded(void)
{
  return num_samples_yielded;
}

//-----------------------------
// print summary
//-----------------------------

void
hpcrun_stats_print_summary(void)
{
  long blocked = num_samples_blocked_async + num_samples_blocked_dlopen;
  long errant = num_samples_dropped;
  long soft = num_samples_dropped - num_samples_segv;
  long valid = num_samples_attempted;
  if (ENABLED(NO_PARTIAL_UNW)) {
    valid = num_samples_attempted - errant;
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


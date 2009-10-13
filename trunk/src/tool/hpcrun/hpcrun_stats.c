// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/trunk/src/tool/hpcrun/sample_event.c $
// $Id: sample_event.c 2590 2009-10-10 23:46:44Z johnmc $
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2009, Rice University 
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
static long num_samples_filtered = 0;
static long num_samples_segv = 0;

static long num_unwind_intervals_total = 0;
static long num_unwind_intervals_suspicious = 0;



//***************************************************************************
// interface operations
//***************************************************************************


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

void
hpcrun_stats_num_samples_blocked_async_inc(void)
{
  atomic_add_i64(&num_samples_blocked_async, 1L);
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




//-----------------------------
// samples filtered
//-----------------------------

void
hpcrun_stats_num_samples_filtered_inc(void)
{
  atomic_add_i64(&num_samples_filtered, 1L);
}


long
hpcrun_stats_num_samples_filtered(void)
{
  return num_samples_filtered;
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



//-----------------------------
// print summary
//-----------------------------

void
hpcrun_stats_print_summary(void)
{
  long blocked = num_samples_blocked_async + num_samples_blocked_dlopen;
  long errant = num_samples_dropped + num_samples_filtered;
  long other = num_samples_dropped - num_samples_segv;
  long valid = num_samples_attempted - errant;

  hpcrun_memory_summary();

  AMSG("SAMPLE ANOMALIES: blocks: %ld (async: %ld, dlopen: %ld), "
       "errors: %ld (filtered: %ld, segv: %d, other: %ld)",
       blocked, num_samples_blocked_async, num_samples_blocked_dlopen,
       errant, num_samples_filtered, num_samples_segv, other);

  AMSG("SUMMARY: samples: %ld (recorded: %ld, blocked: %ld, errant: %ld), "
       "intervals: %ld (suspicious: %ld)%s",
       num_samples_total, valid, blocked, errant,
       num_unwind_intervals_total,  num_unwind_intervals_suspicious,
       hpcrun_is_sampling_disabled() ? " SAMPLING WAS DISABLED" : "");
  // logs, retentions || adj.: recorded, retained, written

  if (ENABLED(UNW_VALID)) {
    hpcrun_validation_summary();
  }
}

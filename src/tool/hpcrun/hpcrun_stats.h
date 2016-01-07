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
// interface operations
//***************************************************************************

void hpcrun_stats_reinit(void);

//-----------------------------
// samples total 
//-----------------------------

void hpcrun_stats_num_samples_total_inc(void);
long hpcrun_stats_num_samples_total(void);


//-----------------------------
// samples attempted 
//-----------------------------

void hpcrun_stats_num_samples_attempted_inc(void);
long hpcrun_stats_num_samples_attempted(void);


//-----------------------------
// samples blocked async 
//-----------------------------

void hpcrun_stats_num_samples_blocked_async_inc(void);
long hpcrun_stats_num_samples_blocked_async(void);


//-----------------------------
// samples blocked dlopen 
//-----------------------------

void hpcrun_stats_num_samples_blocked_dlopen_inc(void);
long hpcrun_stats_num_samples_blocked_dlopen(void);


//-----------------------------
// samples dropped
//-----------------------------

void hpcrun_stats_num_samples_dropped_inc(void);
long hpcrun_stats_num_samples_dropped(void);


//-----------------------------
// partial unwind samples
//-----------------------------

void hpcrun_stats_num_samples_partial_inc(void);
long hpcrun_stats_num_samples_partial(void);

//----------------------------
// samples yielded due to deadlock prevention
//----------------------------

extern void hpcrun_stats_num_samples_yielded_inc(void);
extern long hpcrun_stats_num_samples_yielded(void);

//-----------------------------
// samples filtered
//-----------------------------

void hpcrun_stats_num_samples_filtered_inc(void);
long hpcrun_stats_num_samples_filtered(void);


//-----------------------------
// samples segv
//-----------------------------

void hpcrun_stats_num_samples_segv_inc(void);
long hpcrun_stats_num_samples_segv(void);


//-----------------------------
// unwind intervals total
//-----------------------------

void hpcrun_stats_num_unwind_intervals_total_inc(void);
long hpcrun_stats_num_unwind_intervals_total(void);


//-----------------------------
// unwind intervals suspicious
//-----------------------------

void hpcrun_stats_num_unwind_intervals_suspicious_inc(void);
long hpcrun_stats_num_unwind_intervals_suspicious(void);


//------------------------------------------------------
// samples that include 1 or more successful troll steps
//------------------------------------------------------

void hpcrun_stats_trolled_inc(void);
long hpcrun_stats_trolled(void);

//------------------------------------------------------
// total number of (unwind) frames in sample set
//------------------------------------------------------

void hpcrun_stats_frames_total_inc(long amt);
long hpcrun_stats_frames_total(void);

//---------------------------------------------------------------------
// total number of (unwind) frames in sample set that employed trolling
//---------------------------------------------------------------------

void hpcrun_stats_trolled_frames_inc(long amt);
long hpcrun_stats_trolled_frames(void);

//-----------------------------
// print summary
//-----------------------------

void hpcrun_stats_print_summary(void);

// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

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
// acc samples recorded
//-----------------------------
void hpcrun_stats_acc_samples_add(long value);
long hpcrun_stats_acc_samples(void);


//-----------------------------
// acc samples dropped
//-----------------------------
//
void hpcrun_stats_acc_samples_dropped_add(long value);
long hpcrun_stats_acc_samples_dropped(void);


//-----------------------------
// acc trace records
//-----------------------------
void hpcrun_stats_acc_trace_records_add(long value);
long hpcrun_stats_acc_trace_records(void);


//-----------------------------
// acc trace records dropped
//-----------------------------
//
void hpcrun_stats_acc_trace_records_dropped_add(long value);
long hpcrun_stats_acc_trace_records_dropped(void);


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

//------------------------------------------------------
// number of (unwind) frames in where libunwind failed
//------------------------------------------------------

void hpcrun_stats_frames_libfail_total_inc(long amt);
long hpcrun_stats_frames_libfail_total(void);

//---------------------------------------------------------------------
// total number of (unwind) frames in sample set that employed trolling
//---------------------------------------------------------------------

void hpcrun_stats_trolled_frames_inc(long amt);
long hpcrun_stats_trolled_frames(void);

//-----------------------------
// print summary
//-----------------------------

void hpcrun_stats_print_summary(void);

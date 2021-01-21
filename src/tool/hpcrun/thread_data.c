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
// Copyright ((c)) 2002-2021, Rice University
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

//
//

//************************* System Include Files ****************************

#include <assert.h>

#ifndef _GNU_SOURCE
// needed for pthread_getaffinity_np
#define _GNU_SOURCE
#endif

#include <pthread.h>
#include <sched.h>

#include <stdlib.h>
#include <unistd.h>

//************************ libmonitor Include Files *************************

#include <monitor.h>

//*************************** User Include Files ****************************

#include "newmem.h"
#include "epoch.h"
#include "handling_sample.h"

#include "rank.h"
#include "thread_data.h"
#include "trace.h"

#include <lush/lush-pthread.h>
#include <messages/messages.h>
#include <trampoline/common/trampoline.h>
#include <memory/mmap.h>
#include <lib/prof-lean/id-tuple.h>

//***************************************************************************
// macros
//***************************************************************************

#define DEBUG_CPUSET 0



//***************************************************************************
// types
//***************************************************************************

enum _local_int_const {
  BACKTRACE_INIT_SZ     = 32,
  NEW_BACKTRACE_INIT_SZ = 32
};



//***************************************************************************
// forward declarations
//***************************************************************************

static int
hpcrun_thread_core_bindings
(
 void
);



//***************************************************************************
// data 
//***************************************************************************

#ifdef USE_GCC_THREAD
__thread int monitor_tid = -1;
#endif // USE_GCC_THREAD

static thread_data_t _local_td;
static pthread_key_t _hpcrun_key;
static int use_getspecific = 0;


void
hpcrun_init_pthread_key(void)
{
  TMSG(THREAD_SPECIFIC,"creating _hpcrun_key");
  int bad = pthread_key_create(&_hpcrun_key, NULL);
  if (bad){
    EMSG("pthread_key_create returned non-zero = %d",bad);
  }
  use_getspecific = 1;
}


void
hpcrun_set_thread0_data(void)
{
  TMSG(THREAD_SPECIFIC,"set thread0 data");
  hpcrun_set_thread_data(&_local_td);
}


void
hpcrun_set_thread_data(thread_data_t *td)
{
  TMSG(THREAD_SPECIFIC,"setting td");
  pthread_setspecific(_hpcrun_key, (void *) td);
}


//***************************************************************************

static thread_data_t*
hpcrun_get_thread_data_local(void)
{
  return &_local_td;
}


static bool
hpcrun_get_thread_data_local_avail(void)
{
  return true;
}


thread_data_t*
hpcrun_safe_get_td(void)
{
  if (use_getspecific) {
    return (thread_data_t *) pthread_getspecific(_hpcrun_key);
  }
  else {
    return hpcrun_get_thread_data_local();
  }
}

static thread_data_t*
hpcrun_get_thread_data_specific(void)
{
  thread_data_t *ret = (thread_data_t *) pthread_getspecific(_hpcrun_key);
  if (!ret){
    monitor_real_abort();
  }
  return ret;
}

static bool
hpcrun_get_thread_data_specific_avail(void)
{
  thread_data_t *ret = (thread_data_t *) pthread_getspecific(_hpcrun_key);
  return !(ret == NULL);
}


thread_data_t* (*hpcrun_get_thread_data)(void) = &hpcrun_get_thread_data_local;
bool           (*hpcrun_td_avail)(void)        = &hpcrun_get_thread_data_local_avail;

#if 0
static inline
thread_data_t*
hpcrun_get_thread_data()
{
  if (hpcrun_use_thread_data_local) {
    return hpcrun_get_thread_data_local();
  }
  else {
    return hpcrun_get_thread_data_specific();
  }
}
#endif


void
hpcrun_unthreaded_data(void)
{
  hpcrun_get_thread_data = &hpcrun_get_thread_data_local;
  hpcrun_td_avail        = &hpcrun_get_thread_data_local_avail;
}


void
hpcrun_threaded_data(void)
{
  assert(hpcrun_get_thread_data == &hpcrun_get_thread_data_local);
  hpcrun_get_thread_data = &hpcrun_get_thread_data_specific;
  hpcrun_td_avail        = &hpcrun_get_thread_data_specific_avail;
}


//***************************************************************************
// 
//***************************************************************************

thread_data_t*
hpcrun_allocate_thread_data(int id)
{
  TMSG(THREAD_SPECIFIC,"malloc thread data for thread %d", id);
  return hpcrun_mmap_anon(sizeof(thread_data_t));
}

static inline void
core_profile_trace_data_init(core_profile_trace_data_t * cptd, int id, cct_ctxt_t* thr_ctxt) 
{
  // ----------------------------------------
  // id
  // ----------------------------------------
  cptd->id = id;
  // ----------------------------------------
  // id_tuple
  // ----------------------------------------
  cptd->id_tuple.length = 0;
  cptd->id_tuple.ids_length = 0;
  // ----------------------------------------
  // epoch: loadmap + cct + cct_ctxt
  // ----------------------------------------

  // ----------------------------------------
  cptd->epoch = hpcrun_malloc(sizeof(epoch_t));
  cptd->epoch->csdata_ctxt = copy_thr_ctxt(thr_ctxt);

  // ----------------------------------------
  // cct2metrics map: associate a metric_set with
  //                  a cct node
  hpcrun_cct2metrics_init(&(cptd->cct2metrics_map));

  // ----------------------------------------
  // tracing
  // ----------------------------------------
  cptd->trace_min_time_us = 0;
  cptd->trace_max_time_us = 0;
  cptd->traceOrdered = true;

  // ----------------------------------------
  // IO support
  // ----------------------------------------
  cptd->hpcrun_file  = NULL;
  cptd->trace_buffer = NULL;
  cptd->trace_outbuf = NULL;

  // ----------------------------------------
  // perf event support
  // ----------------------------------------
  cptd->perf_event_info   = NULL;

  // ----------------------------------------
  // ???
  // ----------------------------------------
  cptd->scale_fn = NULL;
}

#ifdef ENABLE_CUDA
static inline void gpu_data_init(gpu_data_t * gpu_data)
{
  gpu_data->is_thread_at_cuda_sync = false;
  gpu_data->overload_state = 0;
  gpu_data->accum_num_sync_threads = 0;
  gpu_data->accum_num_sync_threads = 0;
}
#endif

void
hpcrun_thread_data_init(int id, cct_ctxt_t* thr_ctxt, int is_child, size_t n_sources)
{
  hpcrun_meminfo_t memstore;
  thread_data_t* td = hpcrun_get_thread_data();

  // ----------------------------------------
  // memstore for hpcrun_malloc()
  // ----------------------------------------

  // Wipe the thread data with a bogus bit pattern, but save the
  // memstore so we can reuse it in the child after fork.  This must
  // come first.
  td->inside_hpcrun = 1;
  memstore = td->memstore;
  memset(td, 0xfe, sizeof(thread_data_t));
  td->inside_hpcrun = 1;
  td->memstore = memstore;
  hpcrun_make_memstore(&td->memstore, is_child);
  td->mem_low = 0;

  // ----------------------------------------
  // normalized thread id (monitor-generated)
  // ----------------------------------------
  core_profile_trace_data_init(&(td->core_profile_trace_data), id, thr_ctxt);

  // ----------------------------------------
  // blame shifting support
  // ----------------------------------------

  td->idle = 0;         // a thread begins in the working state 
  td->blame_target = 0; // initially, no target for directed blame

  td->last_sample = 0; 
  td->last_synch_sample = -1; 
 
  td->overhead = 0; // begin at not in overhead

  td->lockwait = 0;
  td->lockid = NULL;

  td->region_id = 0;

  td->outer_region_id = 0;
  td->outer_region_context = 0;

  td->defer_flag = 0;
  
  td->omp_task_context = 0;
  td->master = 0;
  td->team_master = 0;

  td->defer_write = 0;

  td->reuse = 0;

  td->add_to_pool = 0;

  td->omp_thread = 0;
  td->last_bar_time_us = 0;

  // ----------------------------------------
  // sample sources
  // ----------------------------------------


  // allocate ss_state, ss_info

  td->ss_state = hpcrun_malloc(n_sources * sizeof(source_state_t));
  td->ss_info  = hpcrun_malloc(n_sources * sizeof(source_info_t));

  // initialize ss_state,info

  memset(td->ss_state, UNINIT, n_sources * sizeof(source_state_t));
  memset(td->ss_info, 0, n_sources * sizeof(source_info_t));

  td->timer_init = false;
  td->last_time_us = 0;


  // ----------------------------------------
  // backtrace buffer
  // ----------------------------------------
  td->btbuf_cur = NULL;
  td->btbuf_beg = hpcrun_malloc(sizeof(frame_t) * BACKTRACE_INIT_SZ);
  td->btbuf_end = td->btbuf_beg + BACKTRACE_INIT_SZ;
  td->btbuf_sav = td->btbuf_end;  // FIXME: is this needed?

  hpcrun_bt_init(&(td->bt), NEW_BACKTRACE_INIT_SZ);

  td->uw_hash_table = uw_hash_new(1023, hpcrun_malloc);

  // ----------------------------------------
  // trampoline
  // ----------------------------------------
  td->tramp_present     = false;
  td->tramp_retn_addr   = NULL;
  td->tramp_loc         = NULL;
  td->cached_bt_buf_beg = hpcrun_malloc(sizeof(frame_t)
					* CACHED_BACKTRACE_SIZE);
  td->cached_bt_frame_beg = td->cached_bt_buf_beg + CACHED_BACKTRACE_SIZE;         
  td->cached_bt_buf_frame_end = td->cached_bt_frame_beg;
  td->tramp_frame       = NULL;
  td->tramp_cct_node    = NULL;

  // ----------------------------------------
  // exception stuff
  // ----------------------------------------
  td->current_jmp_buf = NULL;
  memset(&td->bad_interval, 0, sizeof(td->bad_interval));
  memset(&td->bad_unwind,   0, sizeof(td->bad_unwind));

  td->deadlock_drop = false;
  hpcrun_init_handling_sample(td, 0, id);
  td->fnbounds_lock = 0;

  // ----------------------------------------
  // Logical unwinding
  // ----------------------------------------
  lushPthr_init(&td->pthr_metrics);
  lushPthr_thread_init(&td->pthr_metrics);


  // ----------------------------------------
  // debug support
  // ----------------------------------------
  td->debug1 = false;

  // ----------------------------------------
  // miscellaneous
  // ----------------------------------------
  td->inside_dlfcn = false;

  // ----------------------------------------
  // gpu trace line support
  // ----------------------------------------
  td->gpu_trace_prev_time = 0;

#ifdef ENABLE_CUDA
  gpu_data_init(&(td->gpu_data));
#endif
}


//***************************************************************************
// 
//***************************************************************************

void
hpcrun_cached_bt_adjust_size(size_t n)
{
  thread_data_t *td = hpcrun_get_thread_data();
  if ((td->cached_bt_buf_frame_end - td->cached_bt_buf_beg) >= n) {
    return; // cached backtrace buffer is already big enough
  }

  frame_t* newbuf = hpcrun_malloc(n * sizeof(frame_t));
  size_t frameSize = td->cached_bt_buf_frame_end - td->cached_bt_frame_beg;
  memcpy(newbuf + n - frameSize, td->cached_bt_frame_beg, 
      (void*)td->cached_bt_buf_frame_end - (void*)td->cached_bt_frame_beg);
  td->cached_bt_buf_beg = newbuf;
  td->cached_bt_buf_frame_end = newbuf+n;
  td->cached_bt_frame_beg = newbuf + n - frameSize;
}


frame_t*
hpcrun_expand_btbuf(void)
{
  thread_data_t* td = hpcrun_get_thread_data();
  frame_t* unwind = td->btbuf_cur;

  /* how big is the current buffer? */
  size_t sz = td->btbuf_end - td->btbuf_beg;
  size_t newsz = sz*2;
  /* how big is the current backtrace? */
  size_t btsz = td->btbuf_end - td->btbuf_sav;
  /* how big is the backtrace we're recording? */
  size_t recsz = unwind - td->btbuf_beg;
  /* get new buffer */
  TMSG(EPOCH," epoch_expand_buffer");
  frame_t *newbt = hpcrun_malloc(newsz*sizeof(frame_t));

  if(td->btbuf_sav > td->btbuf_end) {
    EMSG("Invariant btbuf_sav > btbuf_end violated");
    monitor_real_abort();
  }

  /* copy frames from old to new */
  memcpy(newbt, td->btbuf_beg, recsz*sizeof(frame_t));
  memcpy(newbt+newsz-btsz, td->btbuf_end-btsz, btsz*sizeof(frame_t));

  /* setup new pointers */
  td->btbuf_beg = newbt;
  td->btbuf_end = newbt+newsz;
  td->btbuf_sav = newbt+newsz-btsz;

  /* return new unwind pointer */
  return newbt+recsz;
}


void
hpcrun_ensure_btbuf_avail(void)
{
  thread_data_t* td = hpcrun_get_thread_data();
  if (td->btbuf_cur == td->btbuf_end) {
    td->btbuf_cur = hpcrun_expand_btbuf();
    td->btbuf_sav = td->btbuf_end;
  }
}


void
hpcrun_id_tuple_cputhread
(
 thread_data_t *td
)
{
  int rank = hpcrun_get_rank();
  core_profile_trace_data_t *cptd = &(td->core_profile_trace_data);

  pms_id_t ids[IDTUPLE_MAXTYPES];
  id_tuple_t id_tuple;

  id_tuple_constructor(&id_tuple, ids, IDTUPLE_MAXTYPES);

  id_tuple_push_back(&id_tuple, IDTUPLE_NODE, gethostid()); 

  int core = hpcrun_thread_core_bindings();
  if (core >= 0) {
    id_tuple_push_back(&id_tuple, IDTUPLE_CORE, core); 
  }

  if (rank >= 0) {
    id_tuple_push_back(&id_tuple, IDTUPLE_RANK, rank); 
  }

  id_tuple_push_back(&id_tuple, IDTUPLE_THREAD, cptd->id); 

  id_tuple_copy(&cptd->id_tuple, &id_tuple, hpcrun_malloc);
}


static void
__attribute__((unused))
dump_cpuset
(
 cpu_set_t *cpuset
)
{
  int count = CPU_COUNT(cpuset);
  printf("cpu set count = %d\n", count);
  if (count > 0) {
    printf("cpu set ={ ");
    int i; 
    for (i = 0; i < CPU_SETSIZE; i++) {
      if (CPU_ISSET(i, cpuset)) {
	printf("%d ", i);
      }
    }
    printf("}\n");
  }
}


static bool
cpuset_dense_region
(
 cpu_set_t *cpuset,
 int first,
 int remaining_count
)
{
  int i; 
  for (i = first+1; i < CPU_SETSIZE && remaining_count--; i++) {
    if (!CPU_ISSET(i, cpuset)) {
      return false;
    }
  }
  return true;
}


static int
hpcrun_thread_core_bindings
(
 void
)
{
  int core_id = -1;
  pthread_t self = pthread_self();

  cpu_set_t cpuset;
  if (pthread_getaffinity_np(self, sizeof (cpuset), &cpuset) == 0) {
    // FIXME: this returns the first HW thread id for a dense set of bindings. 
    // this isn't always the right thing. one case that needs special handling is 
    // when HW threads on a core aren't adjacent. there are other cases as well.
    // the right way to do this is to compare with info from hwloc.
    int count = CPU_COUNT(&cpuset);
    if (count < 8) { // no CPU currently supports more than 8 SMT threads
      int i; 
      for (i = 0; i < CPU_SETSIZE; i++) {
	if (CPU_ISSET(i, &cpuset)) {
	  if (cpuset_dense_region(&cpuset, i, count - 1)) {
	    core_id = i;
	  } 
	  break;
	}
      }
    }
  }

  return core_id;
}

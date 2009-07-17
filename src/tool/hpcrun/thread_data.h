//
// $Id$
//

#ifndef THREAD_DATA_H
#define THREAD_DATA_H

// This is called "thread data", but it applies to process as well
// (there is just 1 thread).

#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>

#include "sample_sources_registered.h"
#include "newmem.h"
#include "state.h"
#include "lcp.h"

#include <lush/lush-pthread.i>

typedef struct {
  sigjmp_buf jb;
} sigjmp_buf_t;


typedef struct _td_t {
  int id;
  hpcrun_meminfo_t memstore;
  int              mem_low;
  csprof_state_t*  state;
  lcp_list_t*      lcp_list;     // reverse order: head-of-list = current
  FILE*            hpcrun_file;
  sigjmp_buf_t     bad_unwind;
  sigjmp_buf_t     mem_error;
  int              eventSet[MAX_POSSIBLE_SAMPLE_SOURCES];
  source_state_t   ss_state[MAX_POSSIBLE_SAMPLE_SOURCES];
  int              handling_sample;
  int              splay_lock;
  int              fnbounds_lock;
  int              suspend_sampling;
  FILE*            trace_file;
  uint64_t         last_time_us; // microseconds

  lushPthr_t      pthr_metrics;
} thread_data_t;

#define TD_GET(field) csprof_get_thread_data()->field

extern thread_data_t *(*csprof_get_thread_data)(void);
extern bool          (*csprof_td_avail)(void);
extern thread_data_t *csprof_allocate_thread_data(void);
extern void           csprof_init_pthread_key(void);

extern void           csprof_set_thread_data(thread_data_t *td);
extern void           csprof_set_thread0_data(void);
extern void           csprof_unthreaded_data(void);
extern void           csprof_threaded_data(void);

extern thread_data_t* hpcrun_thread_data_new(void);
extern void           hpcrun_thread_memory_init(void);
extern void           hpcrun_thread_data_init(int id, lush_cct_ctxt_t* thr_ctxt);

// utilities to match previous api
#define csprof_get_state()  TD_GET(state)

#endif // !defined(THREAD_DATA_H)

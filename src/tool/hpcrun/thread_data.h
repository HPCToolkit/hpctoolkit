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
#include "mem.h"
#include "state.h"

#include <lush/lush-pthread.h>

typedef struct {
  sigjmp_buf jb;
} sigjmp_buf_t;


typedef struct _td_t {
  int id;
  //  main memory store
  csprof_mem_t    _mem;
  csprof_mem_t    *memstore;
  // aux data store (mainly for data output when samples exhausted)
  csprof_mem_t    _mem2;
  csprof_mem_t    *memstore2;
  csprof_state_t  *state;
  sigjmp_buf_t    bad_unwind;
  sigjmp_buf_t    mem_error;
  int             eventSet[MAX_POSSIBLE_SAMPLE_SOURCES];
  source_state_t  ss_state[MAX_POSSIBLE_SAMPLE_SOURCES];
  int             handling_sample;
  int             splay_lock;
  int             fnbounds_lock;
  int             suspend_sampling;
  FILE*           trace_file;
  uint64_t        last_time_us; // microseconds

  lush_pthr_t     pthr_metrics;
} thread_data_t;

#define TD_GET(field) csprof_get_thread_data()->field

extern thread_data_t *(*csprof_get_thread_data)(void);
extern bool          (*csprof_td_avail)(void);
extern thread_data_t *csprof_allocate_thread_data(void);
extern void           csprof_init_pthread_key(void);
extern void           csprof_thread_data_init(int id,offset_t sz1,offset_t sz2);
extern void           csprof_set_thread_data(thread_data_t *td);
extern void           csprof_set_thread0_data(void);
extern void           csprof_unthreaded_data(void);
extern void           csprof_threaded_data(void);

// utilities to match previous api
#define csprof_get_state()  TD_GET(state)

#endif // !defined(THREAD_DATA_H)
